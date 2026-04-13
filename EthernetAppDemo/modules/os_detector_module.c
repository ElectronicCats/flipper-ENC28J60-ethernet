#include <furi.h>
#include "os_detector_module.h"
#include "app_user.h"
#include "../libraries/protocol_tools/tcp.h"
#include "../libraries/protocol_tools/ipv4.h"
#include "../libraries/protocol_tools/arp.h"
#include "../libraries/protocol_tools/ethernet_protocol.h"
#include "../modules/tcp_module.h"
#include "../libraries/protocol_tools/icmp.h"
#include "../modules/ping_module.h"
#define OPTS_LEN     13
#define OPTS_PROBES  6
#define packet_count 9
#define TCP_OPTS_MAX 9
#define MAX_RETRIES  9

const char* os_names[] = {"WINDOWS", "LINUX", "IOS", "NO_DETECTED"};

static const char* ipid_pattern_str(ipid_pattern_t p) {
    switch(p) {
    case IPID_ZERO:
        return "ZERO";

    case IPID_INCREMENTAL:
        return "INCREMENTAL";

    case IPID_RANDOM:
        return "RANDOM";

    case IPID_CONSTANT:
        return "CONSTANT";

    default:
        return "UNKNOWN";
    }
}

static const char* seq_pattern_to_string(uint8_t pattern) {
    switch(pattern) {
    case 0:
        return "CONSTANT";

    case 1:
        return "INCREMENTAL";

    case 2:
        return "RANDOM";

    case 3:
        return "LOW_VARIANCE";

    default:
        return "UNKNOWN";
    }
}

// diferencia de IPID con manejo de wrap-around de 16 bits
int32_t ipid_diff(uint16_t prev, uint16_t curr) {
    int32_t diff = (int32_t)curr - (int32_t)prev;

    // corrección por wrap-around
    if(diff < -32768) {
        diff += 65536;
    } else if(diff > 32767) {
        diff -= 65536;
    }
    return diff;
}

double varianza(int32_t* d, int len) {
    if(len <= 1) return 0;
    double suma = 0.0, suma2 = 0.0;
    for(int i = 0; i < len; i++) {
        suma += d[i];
        suma2 += (double)d[i] * (double)d[i];
    }
    double media = suma / len;
    return (suma2 / len) - (media * media);
}

static ipid_pattern_t detectar_patron_ipid(uint16_t* ids, int n) {
    if(n < 3) return IPID_UNKNOWN;

    // ---- Detectar todos cero ----
    bool all_zero = true;
    for(int i = 0; i < n; i++) {
        if(ids[i] != 0) {
            all_zero = false;
            break;
        }
    }
    if(all_zero) return IPID_ZERO;

    // ---- Detectar constante ----
    bool constante = true;
    for(int i = 1; i < n; i++) {
        if(ids[i] != ids[0]) {
            constante = false;
            break;
        }
    }
    if(constante) return IPID_CONSTANT;

    // ---- Calcular diferencias ----
    int32_t diffs[100] = {0};
    int positivos = 0;
    int pequenos = 0;
    int grandes = 0;

    if(n > 100) n = 100;

    for(int i = 0; i < n - 1; i++) {
        diffs[i] = ipid_diff(ids[i], ids[i + 1]);

        if(diffs[i] > 0) positivos++;

        if(diffs[i] > 0 && diffs[i] < 1000) pequenos++;

        if(diffs[i] >= 1000) grandes++;
    }

    double var = varianza(diffs, n - 1);

    // ---- Incremental pequeño (Linux clásico) ----
    if(positivos >= 0.8 * (n - 1) && pequenos >= 0.7 * (n - 1) && var < 20000) {
        return IPID_INCREMENTAL;
    }

    // ---- Incremental con saltos grandes ----
    if(positivos >= 0.8 * (n - 1) && grandes >= 0.5 * (n - 1)) {
        return IPID_INCREMENTAL_LARGE;
    }

    // ---- Random ----
    if(var > 500000) {
        return IPID_RANDOM;
    }

    return IPID_UNKNOWN;
}

typedef enum {
    SEQ_UNKNOWN,
    SEQ_CONSTANT,
    SEQ_INCREMENTAL,
    SEQ_RANDOM
} seq_pattern_t;

static uint8_t clasificar_ipid_por_patron(ipid_pattern_t pattern) {
    switch(pattern) {
    case IPID_ZERO:
        return LINUX;
    case IPID_CONSTANT:
        return WINDOWS;

    case IPID_INCREMENTAL:
    case IPID_INCREMENTAL_LARGE:
        return LINUX;

    case IPID_RANDOM:
        return IOS;

    default:
        return NO_DETECTED;
    }
}

static seq_pattern_t detectar_patron_seq(uint32_t* seq, int n) {
    if(n < 4) return SEQ_UNKNOWN;

    int32_t diffs[100];

    for(int i = 0; i < n - 1; i++) {
        diffs[i] = (int32_t)(seq[i + 1] - seq[i]);
    }

    double var = varianza(diffs, n - 1);

    bool all_same = true;

    for(int i = 1; i < n; i++) {
        if(seq[i] != seq[0]) {
            all_same = false;
            break;
        }
    }

    if(all_same) return SEQ_CONSTANT;

    int small_steps = 0;

    for(int i = 0; i < n - 1; i++) {
        if(diffs[i] > 0 && diffs[i] < 100000) small_steps++;
    }

    if(small_steps > (n - 1) * 0.7 && var < 100000000) return SEQ_INCREMENTAL;

    if(var > 1000000000) return SEQ_RANDOM;

    return SEQ_UNKNOWN;
}

static uint8_t clasificar_seq(seq_pattern_t p) {
    switch(p) {
    case SEQ_CONSTANT:
        return WINDOWS;

    case SEQ_INCREMENTAL:
        return LINUX;

    case SEQ_RANDOM:
        return IOS;

    default:
        return NO_DETECTED;
    }
}

static uint8_t detectar_ts_rate(uint32_t* ts, int n) {
    if(n < 4) return NO_DETECTED;

    int32_t diffs[100];

    for(int i = 0; i < n - 1; i++)
        diffs[i] = ts[i + 1] - ts[i];

    double var = varianza(diffs, n - 1);

    if(var < 10000) return LINUX;

    if(var > 1000000) return WINDOWS;

    return NO_DETECTED;
}

void clasificar_window(uint16_t* win, int n, uint8_t* value_ptr) {
    if(n == 0) return;

    // Buscar valor más frecuente (modo simple)
    int max_count = 0;
    uint16_t valor_dominante = 0;

    for(int i = 0; i < n; i++) {
        int count = 0;
        for(int j = 0; j < n; j++) {
            if(win[j] == win[i]) {
                count++;
            }
        }

        if(count > max_count) {
            max_count = count;
            valor_dominante = win[i];
        }
    }

    // Si el 70% o más coincide → patrón fuerte
    if(max_count >= (0.7 * n)) {
        // Clasificación por rangos (más robusto)
        if(valor_dominante == 65535) {
            *value_ptr = IOS;
        } else if(valor_dominante >= 64000) {
            *value_ptr = WINDOWS;
        } else if(valor_dominante >= 5800 && valor_dominante <= 6000) {
            *value_ptr = LINUX;
        } else if(valor_dominante >= 29000 && valor_dominante <= 30000) {
            *value_ptr = LINUX;
        }
    }
}

static bool os_icmp_probe(App* app, uint8_t* target_ip, uint8_t* out_ttl, uint32_t* out_rtt) {
    uint32_t start_time = furi_get_tick();

    uint8_t packet[MAX_FRAMELEN] = {0};
    uint8_t target_mac[6] = {0};

    arp_get_specific_mac(
        app->ethernet,
        app->ethernet->ip_address,
        target_ip,
        app->ethernet->mac_address,
        target_mac);

    uint16_t packet_len = create_flipper_ping_packet(
        packet,
        app->ethernet->mac_address,
        target_mac,
        app->ethernet->ip_address,
        target_ip,
        0xBEEF,
        1,
        (uint8_t*)"OSPROBE",
        7);

    if(packet_len == 0) return false;

    send_packet(app->ethernet, packet, packet_len);

    while((furi_get_tick() - start_time) < 1000) {
        uint16_t len = receive_packet(app->ethernet, app->ethernet->rx_buffer, MAX_FRAMELEN);
        if(len == 0) continue;

        if(!is_icmp(app->ethernet->rx_buffer)) continue;

        icmp_header_t icmp = icmp_get_header(app->ethernet->rx_buffer);
        if(icmp.type != ICMP_TYPE_ECHO_REPLY) continue;

        ipv4_header_t ip = ipv4_get_header(app->ethernet->rx_buffer);

        *out_ttl = ip.ttl;
        *out_rtt = furi_get_tick() - start_time;

        return true;
    }

    return false;
}

/* 8 options:
 *  0~5: six options for SEQ/OPS/WIN/T1 probes.
 *  6:   ECN probe.
 *  7-12:   T2~T7 probes.
 *
 * option 0: WScale (10), Nop, MSS (1460), Timestamp, SackP
 * option 1: MSS (1400), WScale (0), SackP, T(0xFFFFFFFF,0x0), EOL
 * option 2: T(0xFFFFFFFF, 0x0), Nop, Nop, WScale (5), Nop, MSS (640)
 * option 3: SackP, T(0xFFFFFFFF,0x0), WScale (10), EOL
 * option 4: MSS (536), SackP, T(0xFFFFFFFF,0x0), WScale (10), EOL
 * option 5: MSS (265), SackP, T(0xFFFFFFFF,0x0)
 * option 6: WScale (10), Nop, MSS (1460), SackP, Nop, Nop
 * option 7-11: WScale (10), Nop, MSS (265), T(0xFFFFFFFF,0x0), SackP
 * option 12: WScale (15), Nop, MSS (265), T(0xFFFFFFFF,0x0), SackP
 */

static void parse_tcp_options(const uint8_t* tcp_start, uint8_t tcp_header_len, tcp_opts_t* opts) {
    memset(opts, 0, sizeof(tcp_opts_t));

    uint8_t opts_len = tcp_header_len - 20;
    opts->options_length = opts_len;

    if(opts_len == 0) return;

    const uint8_t* ptr = tcp_start + 20;
    uint8_t i = 0;

    while(i < opts_len) {
        uint8_t kind = ptr[i];

        if(kind == 0) {
            if(opts->count < TCP_OPTS_MAX) opts->order[opts->count++] = 0;
            break;
        }

        if(kind == 1) { // NOP
            opts->has_nop = true;
            opts->order[opts->count++] = 1;
            i++;
            continue;
        }

        if(i + 1 >= opts_len) break;

        uint8_t len = ptr[i + 1];

        if(len < 2 || (i + len) > opts_len) break;

        switch(kind) {
        case 2: // MSS
            opts->has_mss = true;
            opts->mss_value = (ptr[i + 2] << 8) | ptr[i + 3];
            if(opts->count < TCP_OPTS_MAX) opts->order[opts->count++] = 2;
            break;

        case 3: // Window Scale
            opts->has_ws = true;
            opts->ws_value = ptr[i + 2];
            if(opts->count < TCP_OPTS_MAX) opts->order[opts->count++] = 3;
            break;

        case 4: // SACK Permitted
            opts->has_sack = true;
            if(opts->count < TCP_OPTS_MAX) opts->order[opts->count++] = 4;
            break;

        case 5: // SACK blocks
            opts->has_sack = true;

            if(opts->count < TCP_OPTS_MAX) opts->order[opts->count++] = 5;

            break;

        case 8: // Timestamp
            opts->has_ts = true;

            opts->tsval = (ptr[i + 2] << 24) | (ptr[i + 3] << 16) | (ptr[i + 4] << 8) | ptr[i + 5];

            opts->tsecr = (ptr[i + 6] << 24) | (ptr[i + 7] << 16) | (ptr[i + 8] << 8) | ptr[i + 9];

            if(opts->count < TCP_OPTS_MAX) opts->order[opts->count++] = 8;

            break;
        }

        i += len;
    }
}

static uint8_t clasificar_tcp_options(const tcp_opts_t* opts) {
    if(opts->count < 3) return NO_DETECTED;

    /* ---- Firma Windows común ---- */
    if(opts->count >= 4) {
        if(opts->order[0] == 2 && opts->order[1] == 4 && opts->order[2] == 8 &&
           opts->order[3] == 3) {
            return WINDOWS;
        }
    }

    /* ---- Firma Linux clásica ---- */
    if(opts->count >= 7) {
        if(opts->order[0] == 2 && // MSS
           opts->order[1] == 1 && // NOP
           opts->order[2] == 1 && // NOP
           opts->order[3] == 4 && // SACK
           opts->order[4] == 8 && // TS
           opts->order[5] == 1 && // NOP
           opts->order[6] == 3) // WS
        {
            return LINUX;
        }
    }

    /* Apple / BSD */
    if(opts->count >= 6 && opts->order[1] == 1 && opts->order[2] == 3 && opts->order[3] == 1 &&
       opts->order[4] == 1 && opts->order[5] == 8) {
        return IOS;
    }

    /* ---- heurística fallback ---- */

    if(opts->has_mss && opts->has_ws && opts->has_ts && opts->has_sack) {
        if(opts->ws_value == 8) return WINDOWS;

        if(opts->ws_value == 7 || opts->ws_value == 6) return LINUX;
    }

    if(opts->has_mss && opts->has_ws && opts->has_ts) {
        if(opts->ws_value == 6) return IOS;
    }

    /* ---- heurística por longitud de opciones ---- */

    if(opts->options_length == 20 && opts->has_mss && opts->has_ws && opts->has_ts &&
       opts->has_sack) {
        if(opts->ws_value == 7) return LINUX;

        if(opts->ws_value == 8) return WINDOWS;
    }

    /* ---- timestamp fuerte para iOS ---- */

    if(opts->has_ts && opts->ws_value == 6) {
        return IOS;
    }

    return NO_DETECTED;
}

static struct {
    uint8_t* val;
    uint16_t len;
} prbOpts[OPTS_LEN] = {
    {(uint8_t*)"\x03\x03\x0A\x01\x02\x04\x05\xb4\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x04\x02",
     20},
    {(uint8_t*)"\x02\x04\x05\x78\x03\x03\x00\x04\x02\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x00",
     20},
    {(uint8_t*)"\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x01\x01\x03\x03\x05\x01\x02\x04\x02\x80",
     20},
    {(uint8_t*)"\x04\x02\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x03\x03\x0A\x00", 16},
    {(uint8_t*)"\x02\x04\x02\x18\x04\x02\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x03\x03\x0A\x00",
     20},
    {(uint8_t*)"\x02\x04\x01\x09\x04\x02\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00", 16},
    {(uint8_t*)"\x03\x03\x0A\x01\x02\x04\x05\xb4\x04\x02\x01\x01", 12},
    {(uint8_t*)"\x03\x03\x0A\x01\x02\x04\x01\x09\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x04\x02",
     20},
    {(uint8_t*)"\x03\x03\x0A\x01\x02\x04\x01\x09\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x04\x02",
     20},
    {(uint8_t*)"\x03\x03\x0A\x01\x02\x04\x01\x09\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x04\x02",
     20},
    {(uint8_t*)"\x03\x03\x0A\x01\x02\x04\x01\x09\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x04\x02",
     20},
    {(uint8_t*)"\x03\x03\x0A\x01\x02\x04\x01\x09\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x04\x02",
     20},
    {(uint8_t*)"\x03\x03\x0f\x01\x02\x04\x01\x09\x08\x0A\xff\xff\xff\xff\x00\x00\x00\x00\x04\x02",
     20}};

/* TCP Window sizes. Numbering is the same as for prbOpts[] */
uint16_t prbWindowSz[] = {1, 63, 4, 4, 16, 512, 3, 128, 256, 1024, 31337, 32768, 65535};

uint8_t seq_act = OFP_TSEQ;

void ofp_tseq(App* app, uint8_t* target_ip);

void doSeqTests(App* app, uint8_t* target_ip) {
    if(seq_act == OFP_UNSET) return;

    switch(seq_act) {
    case OFP_TSEQ:
        ofp_tseq(app, target_ip);
        break;
    default:
        break;
    }
}

void ofp_tseq(App* app, uint8_t* target_ip) {
    for(uint8_t i = 0; i < OPTS_LEN; i++) {
        for(uint8_t j = 0; j < prbOpts[i].len; j++)
            ;
    }

    UNUSED(target_ip);
    uint8_t source_mac[6] = {0x00, 0xE0, 0x4C, 0x68, 0x0E, 0xC5};
    uint8_t target_mac[6] = {0xCC, 0x73, 0x14, 0x17, 0xA3, 0x45};
    uint8_t source_ip[4] = {192, 168, 0, 105};
    uint8_t target_ip_debug[4] = {192, 168, 0, 103};
    uint16_t ip_id[6] = {63073, 63326, 35094, 25039, 36881, 19500};
    uint16_t ip_flags_offset = 0;
    uint8_t ttl_vec[6];

    for(uint8_t i = 0; i < 6; i++) {
        ttl_vec[i] = 64 + (furi_hal_random_get() % 64); // rango 64–127
    }
    uint16_t source_port = 63954;
    uint16_t target_port = 2121;
    uint32_t sequence = 1354199039;
    uint32_t ack_number = 64547392;

    uint16_t tcp_len = 0;

    for(uint8_t i = 0; i < OPTS_PROBES; i++) {
        set_tcp_header_tseq(
            app->ethernet->tx_buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
            source_ip,
            target_ip_debug,
            source_port + i,
            target_port,
            sequence + i,
            ack_number,
            prbWindowSz[i],
            0,
            &(prbOpts[i].len),
            prbOpts[i].val,
            &tcp_len);

        set_ipv4_header(
            app->ethernet->tx_buffer + ETHERNET_HEADER_LEN,
            6,
            tcp_len,
            source_ip,
            target_ip_debug,
            ip_id[i],
            ip_flags_offset,
            ttl_vec[i]);

        set_ethernet_header(app->ethernet->tx_buffer, source_mac, target_mac, 0x0800);

        send_packet(
            app->ethernet,
            app->ethernet->tx_buffer,
            ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len);

        for(uint16_t j = 0; j < (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len); j++)
            ;

        uint32_t sequences_vector[6] = {0};
        uint16_t len_receive = receive_packet(app->ethernet, app->ethernet->rx_buffer, 1500);
        UNUSED(len_receive);
        if(is_tcp(app->ethernet->rx_buffer)) {
            tcp_header_t tcp_header = tcp_get_header(app->ethernet->rx_buffer);
            bytes_to_uint(sequences_vector + i, tcp_header.sequence, sizeof(uint32_t));
        }
    }
}

static void os_scoreboard_init(os_scoreboard_t* sb) {
    sb->windows_score = 0;
    sb->linux_score = 0;
    sb->ios_score = 0;
}

static void os_score_add(os_scoreboard_t* sb, OS_DETECTOR_OS os, int weight) {
    switch(os) {
    case WINDOWS:
        sb->windows_score += weight;
        break;
    case LINUX:
        sb->linux_score += weight;
        break;
    case IOS:
        sb->ios_score += weight;
        break;
    default:
        break;
    }
}

static OS_DETECTOR_OS os_score_resolve(os_scoreboard_t* sb) {
    int max = sb->windows_score;
    OS_DETECTOR_OS result = WINDOWS;

    if(sb->windows_score == sb->linux_score && sb->linux_score == sb->ios_score)
        return NO_DETECTED;

    if(sb->linux_score > max) {
        max = sb->linux_score;
        result = LINUX;
    }

    if(sb->ios_score > max) {
        max = sb->ios_score;
        result = IOS;
    }

    if(sb->windows_score == sb->linux_score && sb->linux_score == sb->ios_score) {
        return NO_DETECTED;
    }

    // empate parcial
    if((result == WINDOWS && sb->windows_score == sb->linux_score) ||
       (result == LINUX && sb->linux_score == sb->ios_score) ||
       (result == WINDOWS && sb->windows_score == sb->ios_score)) {
        return NO_DETECTED;
    }

    if(max < 25) {
        return NO_DETECTED;
    }

    return result;
}

int get_port_index(uint16_t port, uint16_t* probe_ports, uint8_t count) {
    for(uint8_t i = 0; i < count; i++) {
        if(probe_ports[i] == port) return i;
    }
    return -1;
}

int32_t os_scan(void* context, uint8_t* target_ip) {
    App* app = context;
    os_scoreboard_t sb = {0};
    os_scoreboard_init(&sb);

    uint8_t ttl = 0;
    uint32_t rtt = 0;

    bool ttl_valid = false;
    uint8_t ttl_tcp = 0;

    bool icmp_valid = false;
    uint8_t ttl_icmp = 0;

    uint8_t ttl_guess = 0;
    uint8_t ttl_weight = 0;

    tcp_opts_t tcp_opts_vec[packet_count];

    uint16_t ids[packet_count] = {0};
    uint16_t windows[packet_count] = {0};
    uint32_t sequences[packet_count] = {0};
    uint32_t last_seq_seen[9] = {0}; // uno por probe_port
    bool respuestas[packet_count] = {0};
    uint16_t ids_an[packet_count] = {0};

    uint8_t target_mac[6] = {0};

    uint32_t ts_vals[packet_count] = {0};

    uint16_t dst_port;

    uint16_t src_port = 40000 + (furi_hal_random_get() % 20000);
    uint16_t current_src_port = src_port;
    app->src_port = src_port;
    uint16_t resp_src_port;

    uint32_t ack_number = 0;

    arp_get_specific_mac(
        app->ethernet,
        app->ethernet->ip_address,
        (*(uint32_t*)app->ip_gateway & *(uint32_t*)app->ethernet->subnet_mask) ==
                (*(uint32_t*)target_ip & *(uint32_t*)app->ethernet->subnet_mask) ?
            target_ip :
            app->ip_gateway,
        app->ethernet->mac_address,
        target_mac);

    seq_act = OFP_TSEQ;
    doSeqTests(app, target_ip);

    uint8_t attemp = 0;
    ipv4_header_t ipv4_header;
    tcp_header_t tcp_header;
    uint16_t probe_ports[] = {22, 80, 135, 139, 443, 445, 3389, 5000, 7000, 49152, 62078};
    uint8_t probe_port_count = 11;

    /* Estado de puertos */
    bool port_closed[11] = {false};
    bool port_responded[11] = {false};
    bool port_filtered[11] = {false};
    uint8_t port_retries[11] = {0};
    uint32_t seq_per_port[11];

    port_result_t port_results[probe_port_count];
    for(uint8_t i = 0; i < probe_port_count; i++) {
        port_results[i].port = probe_ports[i];
        port_results[i].state = PORT_UNKNOWN;
        port_results[i].last_sequence = 0;
        port_results[i].window_size = 0;
        port_results[i].ttl = 0;
    }

    while(attemp != packet_count) {
        memset(app->ethernet->rx_buffer, 0, 1500);

        for(uint8_t p = 0; p < probe_port_count; p++) {
            if(port_closed[p] || port_responded[p] || port_filtered[p]) {
                continue;
            }

            uint32_t sequence = furi_hal_random_get() ^ (furi_get_tick() << 16) ^ (p * 1000);

            seq_per_port[p] = sequence;

            tcp_send_syn(
                app->ethernet,
                app->ethernet->mac_address,
                app->ethernet->ip_address,
                target_mac,
                target_ip,
                src_port,
                probe_ports[p],
                sequence,
                ack_number);

            furi_delay_ms(5 + (furi_hal_random_get() % 10));
        }

        uint32_t start_time = furi_get_tick();
        while(furi_get_tick() - start_time < 800) {
            uint16_t packen_len = 0;

            packen_len = receive_packet(app->ethernet, app->ethernet->rx_buffer, 1500);

            if(packen_len) {
                if(is_arp(app->ethernet->rx_buffer)) {
                    arp_reply_requested(
                        app->ethernet, app->ethernet->rx_buffer, app->ethernet->ip_address);
                } else if(is_tcp(app->ethernet->rx_buffer)) {
                    if(memcmp(app->ethernet->rx_buffer, app->ethernet->mac_address, 6) != 0) {
                        continue;
                    }
                    {
                        ipv4_header = ipv4_get_header(app->ethernet->rx_buffer);

                        if(memcmp(ipv4_header.source_ip, target_ip, 4) != 0) {
                            continue;
                        }

                        tcp_header = tcp_get_header(app->ethernet->rx_buffer);

                        uint16_t win_debug;
                        bytes_to_uint(&win_debug, tcp_header.window_size, sizeof(uint16_t));

                        /* ---- VALIDACIÓN DE FLAGS AQUÍ ---- */
                        uint8_t flags = ((uint8_t*)&tcp_header)[13];

                        bool is_synack = (flags & 0x12) == 0x12; // SYN + ACK
                        bool is_rstack = (flags & 0x14) == 0x14; // RST + ACK
                        bool is_rst = (flags & 0x04) == 0x04; // RST

                        /* ---- PUERTO CERRADO (RST+ACK) ---- */

                        if(is_rstack && ipv4_header.ttl == 128) {
                            os_score_add(&sb, WINDOWS, 2);
                        }

                        uint16_t resp_port;

                        if(is_rstack || is_rst) {
                            bytes_to_uint(&resp_port, tcp_header.source_port, sizeof(uint16_t));

                            for(uint8_t k = 0; k < probe_port_count; k++) {
                                if(probe_ports[k] == resp_port) {
                                    port_closed[k] = true;
                                    port_retries[k] = 0;
                                    break;
                                }
                            }

                            continue; // salir del while de espera
                        }

                        if(!is_synack) {
                            continue;
                        }

                        /* ---- VALIDACIÓN PUERTO DESTINO (NUESTRO 5005) ---- */

                        bytes_to_uint(&dst_port, tcp_header.dest_port, sizeof(uint16_t));

                        if(dst_port != current_src_port) {
                            continue;
                        }

                        bytes_to_uint(&resp_src_port, tcp_header.source_port, sizeof(uint16_t));

                        int port_idx =
                            get_port_index(resp_src_port, probe_ports, probe_port_count);
                        if(port_idx < 0) {
                            continue; // puerto que no estamos escaneando
                        }

                        uint32_t ack_recv;

                        if(port_idx >= 0) {
                            port_results[port_idx].last_sequence = ack_recv;
                            port_results[port_idx].window_size = win_debug;
                            port_results[port_idx].ttl = ipv4_header.ttl;

                            if(is_rstack || is_rst) {
                                port_results[port_idx].state = PORT_CLOSED;
                                port_closed[port_idx] = true;
                                port_retries[port_idx] = 0;
                            } else if(is_synack) {
                                port_results[port_idx].state = PORT_OPEN;
                                port_responded[port_idx] = true;
                            } else {
                                port_results[port_idx].state = PORT_FILTERED;
                                port_filtered[port_idx] = true;
                            }
                        }

                        bytes_to_uint(&ack_recv, tcp_header.ack_number, sizeof(uint32_t));

                        if(ack_recv != (seq_per_port[port_idx] + 1)) {
                            continue;
                        }

                        if(port_responded[port_idx]) {
                            continue;
                        }

                        ttl_tcp = ipv4_header.ttl;
                        ttl_valid = true;

                        /* ---------- TCP OPTIONS CAPTURE ---------- */
                        tcp_opts_t tcp_opts;
                        memset(&tcp_opts, 0, sizeof(tcp_opts_t));

                        if(is_synack) {
                            uint8_t tcp_header_len = (((uint8_t*)&tcp_header)[12] >> 4) * 4;

                            uint16_t resp_src_port;
                            bytes_to_uint(
                                &resp_src_port, tcp_header.source_port, sizeof(uint16_t));
                            bytes_to_uint(&dst_port, tcp_header.dest_port, sizeof(uint16_t));

                            uint32_t server_seq;
                            bytes_to_uint(&server_seq, tcp_header.sequence, sizeof(uint32_t));

                            /* ---- FILTRO DE DUPLICADOS POR SEQUENCE ---- */
                            if(last_seq_seen[port_idx] == server_seq) {
                                continue;
                            }
                            last_seq_seen[port_idx] = server_seq;

                            sequences[attemp] = server_seq;

                            parse_tcp_options(
                                app->ethernet->rx_buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
                                tcp_header_len,
                                &tcp_opts_vec[attemp]);

                            if(tcp_opts_vec[attemp].has_ts) {
                                ts_vals[attemp] = tcp_opts_vec[attemp].tsval;
                            }

                            for(uint8_t k = 0; k < tcp_opts_vec[attemp].count; k++)
                                ;
                        }

                        uint16_t windows_size;
                        uint16_t ipid;

                        bytes_to_uint(&windows_size, tcp_header.window_size, sizeof(uint16_t));
                        bytes_to_uint(&ipid, ipv4_header.identification, sizeof(uint16_t));

                        /* Heurística fuerte Linux */

                        if(windows_size == 29200 && ipv4_header.ttl <= 64) {
                            os_score_add(&sb, LINUX, 12);
                        }

                        if(tcp_opts_vec[attemp].has_ts && tcp_opts_vec[attemp].ws_value == 6 &&
                           windows_size == 65535 && ttl_tcp <= 64) {
                            os_score_add(&sb, IOS, 15);
                        }

                        /* ---- CONTROL DE DUPLICADOS ---- */
                        bool duplicated = false;

                        /* Permitir múltiples IPID = 0 (Linux moderno) */
                        if(ipid != 0) {
                            for(uint8_t i = 0; i < attemp; i++) {
                                if(respuestas[i] && ids[i] == ipid) {
                                    duplicated = true;
                                    break;
                                }
                            }
                        }

                        if(duplicated) {
                            continue;
                        }

                        /* ---- MARCAR PUERTO COMO COMPLETADO (SYN-ACK) ---- */
                        port_retries[port_idx] = 0;
                        port_responded[port_idx] = true;

                        ids[attemp] = ipid;
                        windows[attemp] = windows_size;
                        respuestas[attemp] = true;

                        uint8_t count_valid = 0;
                        for(uint8_t i = 0; i <= attemp; i++) {
                            if(respuestas[i]) {
                                count_valid++;
                            }
                        }

                        if(count_valid >= 6) {
                            port_responded[port_idx] = true;
                        }

                        for(uint16_t i = 0; i < packen_len; i++)
                            ;

                        continue; // salir del while de espera para enviar nuevos SYN a puertos restantes
                    }
                }
            }
        }

        for(uint8_t p = 0; p < probe_port_count; p++) {
            if(port_closed[p] || port_responded[p] || port_filtered[p]) {
                continue;
            }

            // Si no hubo respuesta en esta ronda → retry
            port_retries[p]++;

            uint32_t seq = furi_hal_random_get();

            tcp_send_syn(
                app->ethernet,
                app->ethernet->mac_address,
                app->ethernet->ip_address,
                target_mac,
                target_ip,
                src_port,
                probe_ports[p],
                seq,
                0);

            if(port_retries[p] > 0 && (port_retries[p] % 3) == 0) {
                uint32_t seq = furi_hal_random_get();

                tcp_send_null_probe(
                    app->ethernet,
                    app->ethernet->mac_address,
                    app->ethernet->ip_address,
                    target_mac,
                    target_ip,
                    src_port,
                    probe_ports[p],
                    seq);

                furi_delay_ms(3 + (furi_hal_random_get() % 5));

                tcp_send_fin_probe(
                    app->ethernet,
                    app->ethernet->mac_address,
                    app->ethernet->ip_address,
                    target_mac,
                    target_ip,
                    src_port,
                    probe_ports[p],
                    seq + 1);

                furi_delay_ms(3 + (furi_hal_random_get() % 5));

                tcp_send_xmas_probe(
                    app->ethernet,
                    app->ethernet->mac_address,
                    app->ethernet->ip_address,
                    target_mac,
                    target_ip,
                    src_port,
                    probe_ports[p],
                    seq + 2);

                furi_delay_ms(3 + (furi_hal_random_get() % 5));

                tcp_send_ack_probe(
                    app->ethernet,
                    app->ethernet->mac_address,
                    app->ethernet->ip_address,
                    target_mac,
                    target_ip,
                    src_port,
                    probe_ports[p],
                    seq + 3);

                furi_delay_ms(3 + (furi_hal_random_get() % 5));
            }

            if(port_retries[p] >= MAX_RETRIES) {
                continue;
            }

            furi_delay_ms(20 + (furi_hal_random_get() % 30));
        }

        bool all_done = true;

        for(uint8_t i = 0; i < probe_port_count; i++) {
            if(!port_closed[i] && !port_filtered[i] && !port_responded[i]) {
                all_done = false;
                break;
            }
        }

        if(all_done) {
            break;
        }
        attemp++;

        uint8_t filtered_count = 0;

        for(uint8_t p = 0; p < probe_port_count; p++) {
            if(!port_responded[p] && port_retries[p] >= MAX_RETRIES) {
                port_filtered[p] = true;
                filtered_count++;
            }
        }
    }

    for(uint8_t i = 0; i < probe_port_count; i++) {
        printf(
            "Port %u -> State: %s, TTL: %u, Window: %u, Seq: %lu\n",
            port_results[i].port,
            port_results[i].state == PORT_OPEN     ? "OPEN" :
            port_results[i].state == PORT_CLOSED   ? "CLOSED" :
            port_results[i].state == PORT_FILTERED ? "FILTERED" :
                                                     "UNKNOWN",
            port_results[i].ttl,
            port_results[i].window_size,
            port_results[i].last_sequence);
    }

    if(os_icmp_probe(app, target_ip, &ttl, &rtt)) {
        ttl_icmp = ttl;
        icmp_valid = true;
    }

    uint8_t sum_true = 0;
    uint8_t an_index = 0;
    for(uint8_t i = 0; i < packet_count; i++) {
        sum_true += respuestas[i] ? 1 : 0;
        if(respuestas[i]) {
            ids_an[an_index] = ids[i];
            an_index++;
        }
    }

    uint8_t filtered_count = 0;

    for(uint8_t i = 0; i < probe_port_count; i++) {
        if(port_filtered[i]) filtered_count++;
    }

    /* ---------- INDIVIDUAL PORT SCORING ---------- */

    for(uint8_t i = 0; i < probe_port_count; i++) {
        if(!port_responded[i]) continue;

        uint16_t port = probe_ports[i];

        switch(port) {
        case 22:
            os_score_add(&sb, LINUX, 4);
            os_score_add(&sb, IOS, 2);
            break;

        case 135:
            os_score_add(&sb, WINDOWS, 5);
            break;

        case 139:
            os_score_add(&sb, WINDOWS, 3);
            break;

        case 445:
            os_score_add(&sb, WINDOWS, 6);
            os_score_add(&sb, LINUX, -2);
            os_score_add(&sb, IOS, -2);
            break;

        case 3389:
            os_score_add(&sb, WINDOWS, 5);
            break;

        case 49152:
            os_score_add(&sb, WINDOWS, 3);
            break;

        case 62078:
            os_score_add(&sb, IOS, 8);
            break;

        case 5000: // AirPlay / RAOP
        case 7000: // AirPlay
            os_score_add(&sb, IOS, 5);
            break;

        default:
            break;
        }
    }

    /* ---------- PORT RELATION SCORING ---------- */

    // Windows fuerte
    if(port_responded[get_port_index(135, probe_ports, probe_port_count)] &&
       port_responded[get_port_index(445, probe_ports, probe_port_count)]) {
        os_score_add(&sb, WINDOWS, 8);
    }

    // Windows muy fuerte
    if(port_responded[get_port_index(135, probe_ports, probe_port_count)] &&
       port_responded[get_port_index(445, probe_ports, probe_port_count)] &&
       port_responded[get_port_index(3389, probe_ports, probe_port_count)]) {
        os_score_add(&sb, WINDOWS, 6);
    }

    // Linux fuerte
    if(port_responded[get_port_index(22, probe_ports, probe_port_count)] &&
       port_closed[get_port_index(445, probe_ports, probe_port_count)]) {
        os_score_add(&sb, LINUX, 8);
    }

    // Apple fuerte (lockdownd)
    if(port_responded[get_port_index(62078, probe_ports, probe_port_count)]) {
        os_score_add(&sb, IOS, 10);
    }

    /* ---------- APPLE AIRPLAY RELATIONS ---------- */

    bool port_5000 = port_responded[get_port_index(5000, probe_ports, probe_port_count)];
    bool port_7000 = port_responded[get_port_index(7000, probe_ports, probe_port_count)];
    bool port_62078 = port_responded[get_port_index(62078, probe_ports, probe_port_count)];

    // 5000 + 7000 → Apple fuerte
    if(port_5000 && port_7000) {
        os_score_add(&sb, IOS, 6);
    }

    // 7000 + 62078 → Apple muy fuerte
    if(port_7000 && port_62078) {
        os_score_add(&sb, IOS, 6);
    }

    // 5000 + 62078 → también válido
    if(port_5000 && port_62078) {
        os_score_add(&sb, IOS, 5);
    }

    /* ---------- APPLE STRONG SIGNATURE ---------- */

    bool apple_ports = port_responded[get_port_index(5000, probe_ports, probe_port_count)] &&
                       port_responded[get_port_index(7000, probe_ports, probe_port_count)];

    if(apple_ports && ttl_guess == 64 && windows[0] == 65535) {
        os_score_add(&sb, IOS, 10);
    }

    if(apple_ports) {
        os_score_add(&sb, LINUX, -5);
    }

    if(sum_true || icmp_valid) {
        /* ---------- TTL SCORING ---------- */
        if(ttl_valid) {
            /* Estimación de TTL inicial */
            if(ttl_tcp <= 64) {
                ttl_guess = 64;
            }

            else if(ttl_tcp <= 128) {
                ttl_guess = 128;
            }

            ttl_weight = 10; // TCP pesa más
        } else if(icmp_valid) {
            if(ttl_icmp <= 64)
                ttl_guess = 64;
            else if(ttl_icmp <= 128)
                ttl_guess = 128;

            ttl_weight = 10; // ICMP pesa menos
        }

        /* Asignación de puntuación */
        if(ttl_guess == 64) {
            os_score_add(&sb, LINUX, ttl_weight - 3);
            os_score_add(&sb, IOS, 3);
        }

        else if(ttl_guess == 128) {
            os_score_add(&sb, WINDOWS, ttl_weight);
        }

        /* ---------- FILTERED PORTS SCORING ---------- */

        if(filtered_count >= 3) {
            os_score_add(&sb, LINUX, 5);
            os_score_add(&sb, IOS, 5);
        }

        if(filtered_count >= 5 && ttl_guess == 64) {
            os_score_add(&sb, IOS, 5);
        }

        if(filtered_count >= 6) {
            os_score_add(&sb, LINUX, 8);
        }

        if(filtered_count == probe_port_count) {
            os_score_add(&sb, LINUX, 10);
            os_score_add(&sb, IOS, 10);
        }

        /* ---------- IPID SCORING ---------- */

        if(sum_true >= 3) {
            ipid_pattern_t pattern = detectar_patron_ipid(ids_an, sum_true);
            printf("[IPID] Pattern: %s\n", ipid_pattern_str(pattern));
            uint8_t ipid_os = clasificar_ipid_por_patron(pattern);

            switch(ipid_os) {
            case WINDOWS:
                sb.windows_score += 3;
                break;

            case LINUX:
                sb.linux_score += 3;
                break;

            case IOS:
                sb.ios_score += 3;
                break;
            }

            /* Heurística Apple/BSD adicional */

            if(pattern == IPID_RANDOM && ttl_guess == 64) {
                os_score_add(&sb, IOS, 8);
            }

            /* Calcular varianza manual para peso dinámico */
            int32_t diffs[100] = {0};
            for(int i = 0; i < sum_true - 1; i++) {
                diffs[i] = ipid_diff(ids_an[i], ids_an[i + 1]);
            }

            double var = varianza(diffs, sum_true - 1);

            const char* ipid_names[] = {
                "UNKNOWN", "CONSTANT", "INCREMENTAL", "INCREMENTAL_LARGE", "RANDOM", "ZERO"};

            printf("[IPID] Pattern: %s\n", ipid_names[pattern]);

            switch(pattern) {
            case IPID_ZERO:
                os_score_add(&sb, LINUX, 4);
                os_score_add(&sb, IOS, 4);
                break;

            case IPID_INCREMENTAL:

                if(ttl_guess == 128) {
                    // TTL 128 + incremental → Windows fuerte
                    os_score_add(&sb, WINDOWS, 15);
                } else if(ttl_guess == 64) {
                    // TTL 64 + incremental → Linux posible
                    os_score_add(&sb, LINUX, 12);
                }
                break;

            case IPID_INCREMENTAL_LARGE:
                os_score_add(&sb, LINUX, 15);
                break;

            case IPID_RANDOM:

                if(ttl_guess == 64 && var > 500000) {
                    os_score_add(&sb, IOS, 18);
                } else {
                    os_score_add(&sb, IOS, 10);
                }

                break;

            case IPID_CONSTANT:
                os_score_add(&sb, WINDOWS, 15);
                break;

            default:
                break;
            }

            /* Bonus por coherencia con TTL */
            if(ttl_valid) {
                if(ttl_tcp <= 128 && pattern == IPID_CONSTANT) {
                    os_score_add(&sb, WINDOWS, 2);
                }
            }
        }

        /* ---------- TCP OPTIONS GLOBAL SCORING ---------- */

        int linux_opt = 0;
        int windows_opt = 0;
        int ios_opt = 0;

        for(uint8_t i = 0; i < sum_true; i++) {
            uint8_t guess = clasificar_tcp_options(&tcp_opts_vec[i]);

            if(guess == LINUX) linux_opt++;
            if(guess == WINDOWS) windows_opt++;
            if(guess == IOS) ios_opt++;
        }

        if(linux_opt >= 3) {
            os_score_add(&sb, LINUX, 20);
        }

        if(windows_opt >= 3) {
            os_score_add(&sb, WINDOWS, 20);
        }

        if(ios_opt >= 3) {
            os_score_add(&sb, IOS, 20);
        }

        /* ---------- TCP SEQUENCE SCORING ---------- */

        if(sum_true >= 4) {
            seq_pattern_t seq_pattern = detectar_patron_seq(sequences, sum_true);

            printf("[SEQ] Pattern: %d (%s)\n", seq_pattern, seq_pattern_to_string(seq_pattern));

            uint8_t seq_os = clasificar_seq(seq_pattern);

            if(seq_os == WINDOWS)
                os_score_add(&sb, WINDOWS, 10);

            else if(seq_os == LINUX)
                os_score_add(&sb, LINUX, 10);

            else if(seq_os == IOS)
                os_score_add(&sb, IOS, 10);
        }

        /* ---------- TCP TIMESTAMP SCORING ---------- */

        uint8_t ts_guess = detectar_ts_rate(ts_vals, sum_true);

        if(ts_guess == LINUX) os_score_add(&sb, LINUX, 5);

        if(ts_guess == WINDOWS) os_score_add(&sb, WINDOWS, 5);

        if(ts_guess == IOS) os_score_add(&sb, IOS, 5);

        /* ---------- CONSISTENCY BONUS ---------- */

        if(linux_opt >= 5) {
            os_score_add(&sb, LINUX, 10);
        }
        if(windows_opt >= 5) {
            os_score_add(&sb, WINDOWS, 10);
        }
        if(ios_opt >= 5) {
            os_score_add(&sb, IOS, 10);
        }

        /* ---------- WINDOW SCORING ---------- */
        uint8_t value_win = NO_DETECTED;
        clasificar_window(windows, sum_true, &value_win);

        if(value_win == LINUX)
            os_score_add(&sb, LINUX, 10);
        else if(value_win == WINDOWS)
            os_score_add(&sb, WINDOWS, 10);
        else if(value_win == IOS)
            os_score_add(&sb, IOS, 10);
    }

    /* ---------- CROSS SIGNAL CONSISTENCY ---------- */

    if(ttl_guess == 64 && sb.linux_score > sb.windows_score) {
        os_score_add(&sb, LINUX, 5);
    }

    if(ttl_guess == 128 && sb.windows_score > sb.linux_score) {
        os_score_add(&sb, WINDOWS, 5);
    }

    if(ttl_guess == 64 && sb.ios_score > sb.windows_score && sb.ios_score > sb.linux_score) {
        os_score_add(&sb, IOS, 5);
    }

    /* ---------- GLOBAL CONSISTENCY ---------- */

    if(sb.windows_score > sb.linux_score + 5 && sb.windows_score > sb.ios_score + 5) {
        os_score_add(&sb, WINDOWS, 5);
    }

    if(sb.linux_score > sb.windows_score + 5 && sb.linux_score > sb.ios_score + 5) {
        os_score_add(&sb, LINUX, 5);
    }

    if(sb.ios_score > sb.windows_score + 5 && sb.ios_score > sb.linux_score + 5) {
        os_score_add(&sb, IOS, 5);
    }

    app->ports_count = probe_port_count;

    for(uint8_t i = 0; i < probe_port_count; i++) {
        app->ports[i] = port_results[i];
    }

    /* ---------- SCORE ANALYSIS ---------- */

    int best = sb.windows_score;
    int second = sb.linux_score;
    uint8_t result = WINDOWS;

    if(sb.linux_score > best) {
        second = best;
        best = sb.linux_score;
        result = LINUX;
    } else {
        second = sb.linux_score;
    }

    if(sb.ios_score > best) {
        second = best;
        best = sb.ios_score;
        result = IOS;
    } else if(sb.ios_score > second) {
        second = sb.ios_score;
    }

    /* ---------- DETECTION STRENGTH ---------- */

#define STRONG_OS_THRESHOLD 25
#define GUESS_OS_THRESHOLD  10

    if(best >= STRONG_OS_THRESHOLD) {
        app->os_guess = false;
        return result;

    }

    else if(best >= GUESS_OS_THRESHOLD) {
        app->os_guess = true;
        return result;

    }

    else {
        app->os_guess = false;
        return NO_DETECTED;
    }
    return os_score_resolve(&sb);
}
