#include <furi.h>
#include "os_detector_module.h"
#include "app_user.h"
#include "../libraries/protocol_tools/tcp.h"
#include "../libraries/protocol_tools/ipv4.h"
#include "../libraries/protocol_tools/arp.h"
#include "../libraries/protocol_tools/ethernet_protocol.h"
#include "tcp_module.h"
#include "../libraries/protocol_tools/icmp.h"
#include "../modules/ping_module.h"
#define OPTS_LEN     13
#define OPTS_PROBES  6
#define packet_count 20
#define TCP_OPTS_MAX 10

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

    for(int i = 0; i < n - 1; i++) {
        diffs[i] = ipid_diff(ids[i], ids[i + 1]);

        if(n > 100) n = 100;

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
        if(valor_dominante >= 64000) {
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

    if(opts_len == 0) return;

    const uint8_t* ptr = tcp_start + 20;
    uint8_t i = 0;

    while(i < opts_len) {
        uint8_t kind = ptr[i];

        if(kind == 0) { // End of Option List
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
            opts->order[opts->count++] = 2;
            break;

        case 3: // Window Scale
            opts->has_ws = true;
            opts->ws_value = ptr[i + 2];
            opts->order[opts->count++] = 3;
            break;

        case 4: // SACK Permitted
            opts->has_sack = true;
            opts->order[opts->count++] = 4;
            break;

        case 8: // Timestamp
            opts->has_ts = true;
            opts->order[opts->count++] = 8;
            break;

        default:
            if(opts->count < TCP_OPTS_MAX) opts->order[opts->count++] = kind;
            break;
        }

        i += len;
    }
}

static uint8_t clasificar_tcp_options(const tcp_opts_t* opts) {
    if(opts->count < 3) return NO_DETECTED;

    /*
    Firmas típicas SYN-ACK reales:

    Linux común:
    MSS(2) NOP(1) NOP(1) SACK(4) NOP(1) WS(3)

    Windows común:
    MSS(2) SACK(4) WS(3)
    */

    if(opts->order[0] == 2) {
        /* Linux clásico */
        if(opts->count >= 4 && opts->order[1] == 1 && opts->order[2] == 1 && opts->order[3] == 4) {
            return LINUX;
        }

        /* Windows típico */
        if(opts->count >= 3 && opts->order[1] == 4 && opts->order[2] == 3) {
            return WINDOWS;
        }
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

uint8_t seq_act = OFP_UNSET;

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

/*void make_tseq_packet(
    uint8_t* source_mac,
    uint8_t* target_mac,
    uint8_t* source_ip,
    uint8_t* target_ip,
    uint8_t* source_port,
    uint8_t* target_port) {
    set_tcp_header_tseq(
        app->ethernet->tx_buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
        app->ethernet->ip_address,
        target_ip,
        source_port + 1,
        target_port,
        sequence,
        ack_number,
        prbWindowSz[i],
        0,
        &prbOpts[i].len,
        prbOpts[i].val,
        &tcp_len);

    set_ipv4_header(
        app->ethernet->tx_buffer + ETHERNET_HEADER_LEN,
        6, // Protocolo TCP
        tcp_len,
        app->ethernet->ip_address,
        target_ip);

    set_ethernet_header(app->ethernet->tx_buffer, app->ethernet->mac_address, target_mac, 0x0800);

    //send_packet(app->ethernet, app->ethernet->tx_buffer, ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len);

    printf("BUFFER: ");
    for(uint16_t j = 0; j < (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len); j++) {
        printf(
            "%02X%c",
            app->ethernet->tx_buffer[j],
            j == (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len - 1) ? '\n' : ' ');
    }
}*/

void ofp_tseq(App* app, uint8_t* target_ip) {
    printf("IMPRIMIR OPCIONES:\n");
    for(uint8_t i = 0; i < OPTS_LEN; i++) {
        printf("OPTIONS: %u: %u -> %u ", i, prbOpts[i].len, prbWindowSz[i]);
        for(uint8_t j = 0; j < prbOpts[i].len; j++) {
            printf("%02X%c", prbOpts[i].val[j], j == (prbOpts[i].len - 1) ? '\n' : ' ');
        }
    }

    UNUSED(target_ip);
    uint8_t source_mac[6] = {0x00, 0xE0, 0x4C, 0x68, 0x0E, 0xC5};
    uint8_t target_mac[6] = {0xCC, 0x73, 0x14, 0x17, 0xA3, 0x45};
    uint8_t source_ip[4] = {192, 168, 0, 105};
    uint8_t target_ip_debug[4] = {192, 168, 0, 103};
    uint16_t ip_id[6] = {63073, 63326, 35094, 25039, 36881, 19500};
    uint16_t ip_flags_offset = 0;
    uint8_t ttl_vec[6] = {55, 51, 38, 52, 50, 52};
    uint16_t source_port = 63954;
    uint16_t target_port = 2121;
    uint32_t sequence = 1354199039;
    uint32_t ack_number = 64547392;

    uint16_t tcp_len = 0;

    for(uint8_t i = 0; i < OPTS_PROBES; i++) {
        printf("No. SEQ: %lu -> %04lx\n", sequence + i, sequence + i);

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

        //send_packet(app->ethernet, app->ethernet->tx_buffer, ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len);

        printf("BUFFER: ");
        for(uint16_t j = 0; j < (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len); j++) {
            printf(
                "%02x%c",
                app->ethernet->tx_buffer[j],
                j == (ETHERNET_HEADER_LEN + IP_HEADER_LEN + tcp_len - 1) ? '\n' : ' ');
        }

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
    printf("Scoring Feedback\n");

    printf("Final Scores -> W:%d L:%d I:%d\n", sb->windows_score, sb->linux_score, sb->ios_score);

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

    printf("Max score: %d\n", max);
    printf("Proposed result: %s\n", os_names[result]);

    if(sb->windows_score == sb->linux_score && sb->linux_score == sb->ios_score) {
        printf("All scores equal -> NO_DETECTED\n");
        return NO_DETECTED;
    }

    // empate parcial
    if((result == WINDOWS && sb->windows_score == sb->linux_score) ||
       (result == LINUX && sb->linux_score == sb->ios_score) ||
       (result == WINDOWS && sb->windows_score == sb->ios_score)) {
        printf("Tie detected -> NO_DETECTED\n");
        return NO_DETECTED;
    }

    if(max < 25) {
        printf("Below confidence threshold (5) -> NO_DETECTED\n");
        return NO_DETECTED;
    }

    printf("SCORING END.\n");
    return result;
}

int32_t os_scan(void* context, uint8_t* target_ip) {
    printf("OS DETECTOR INITIALIZED:\n");
    printf("- Target IP: %u.%u.%u.%u\n", target_ip[0], target_ip[1], target_ip[2], target_ip[3]);
    os_scoreboard_t sb = {0};
    os_scoreboard_init(&sb);
    App* app = context;

    // TSEQ unabled
    seq_act = OFP_UNSET;

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
    bool respuestas[packet_count] = {0};
    uint16_t ids_an[packet_count] = {0};

    uint8_t target_mac[6] = {0};

    arp_get_specific_mac(
        app->ethernet,
        app->ethernet->ip_address,
        (*(uint32_t*)app->ip_gateway & *(uint32_t*)app->ethernet->subnet_mask) ==
                (*(uint32_t*)target_ip & *(uint32_t*)app->ethernet->subnet_mask) ?
            target_ip :
            app->ip_gateway,
        app->ethernet->mac_address,
        target_mac);

    uint32_t sequence = 1;
    uint32_t ack_number = 0;

    uint32_t last_time;

    uint8_t attemp = 0;
    ipv4_header_t ipv4_header;
    tcp_header_t tcp_header;
    uint16_t probe_ports[] = {80, 443};
    uint8_t probe_port_count = 2;
    while(attemp != packet_count) {
        memset(app->ethernet->rx_buffer, 0, 1500);

        for(uint8_t p = 0; p < probe_port_count; p++) {
            tcp_send_syn(
                app->ethernet,
                app->ethernet->mac_address,
                app->ethernet->ip_address,
                target_mac,
                target_ip,
                5005,
                probe_ports[p],
                sequence,
                ack_number);

            last_time = furi_get_tick();
            while(furi_get_tick() - last_time < 300) {
                uint16_t packen_len = 0;

                packen_len = receive_packet(app->ethernet, app->ethernet->rx_buffer, 1500);

                if(packen_len) {
                    if(is_arp(app->ethernet->rx_buffer)) {
                        arp_reply_requested(
                            app->ethernet, app->ethernet->rx_buffer, app->ethernet->ip_address);
                    } else if(is_tcp(app->ethernet->rx_buffer)) {
                        if((*(uint16_t*)(app->ethernet->mac_address + 4) ==
                            *(uint16_t*)(app->ethernet->rx_buffer + 4)) &&
                           (*(uint32_t*)app->ethernet->mac_address ==
                            *(uint32_t*)app->ethernet->rx_buffer)) {
                            ipv4_header = ipv4_get_header(app->ethernet->rx_buffer);

                            if(memcmp(ipv4_header.source_ip, target_ip, 4) != 0) {
                                continue;
                            }

                            tcp_header = tcp_get_header(app->ethernet->rx_buffer);

                            /* ---- VALIDACIÓN DE FLAGS AQUÍ ---- */
                            uint8_t flags = ((uint8_t*)&tcp_header)[13];

                            bool is_synack = (flags & 0x12) == 0x12; // SYN + ACK
                            bool is_rstack = (flags & 0x14) == 0x14; // RST + ACK

                            if(is_rstack && ttl_tcp == 128) {
                                os_score_add(&sb, WINDOWS, 2);
                            }

                            if(!(is_synack || is_rstack)) {
                                continue; // ignora paquetes que no sean respuesta válida al SYN
                            }

                            uint32_t ack_recv;
                            bytes_to_uint(&ack_recv, tcp_header.ack_number, sizeof(uint32_t));

                            if(ack_recv != (sequence + 1)) {
                                continue; // no es respuesta a nuestro SYN
                            }
                            /* ---- VALIDACIÓN PUERTO DESTINO (NUESTRO 5005) ---- */
                            uint16_t dst_port;
                            bytes_to_uint(&dst_port, tcp_header.dest_port, sizeof(uint16_t));

                            if(dst_port != 5005) {
                                continue;
                            }

                            uint16_t src_port;
                            bytes_to_uint(&src_port, tcp_header.source_port, sizeof(uint16_t));

                            printf("\n[TCP SYN RESPONSE]\n");

                            printf("Source Port: %u\n", probe_ports[p]);
                            printf("Server Port: %u\n", src_port);
                            printf("Dest Port (ours): %u\n", dst_port);

                            printf("Flags: 0x%02X (", flags);
                            if(flags & 0x02) printf("SYN ");
                            if(flags & 0x10) printf("ACK ");
                            if(flags & 0x04) printf("RST ");
                            if(flags & 0x01) printf("FIN ");
                            printf(")\n");

                            printf("ACK Number: %lu\n", ack_recv);

                            ttl_tcp = ipv4_header.ttl;
                            ttl_valid = true;

                            /* ---------- TCP OPTIONS CAPTURE ---------- */
                            tcp_opts_t tcp_opts;
                            memset(&tcp_opts, 0, sizeof(tcp_opts_t));

                            if(is_synack) {
                                uint8_t tcp_header_len = (((uint8_t*)&tcp_header)[12] >> 4) * 4;

                                parse_tcp_options(
                                    (uint8_t*)&tcp_header, tcp_header_len, &tcp_opts);

                                printf("[TCP OPTS] Count: %u\n", tcp_opts.count);
                                printf("[TCP OPTS] Order: ");
                                for(uint8_t k = 0; k < tcp_opts.count; k++) {
                                    printf("%u ", tcp_opts.order[k]);
                                }
                                printf("\n");
                            }

                            uint16_t windows_size;
                            uint16_t ipid;

                            bytes_to_uint(&windows_size, tcp_header.window_size, sizeof(uint16_t));
                            bytes_to_uint(&ipid, ipv4_header.identification, sizeof(uint16_t));

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
                            printf("- Windows Size: %u\n", windows_size);
                            printf("- TTL: %u\n", ipv4_header.ttl);
                            printf("- IPID: %u\n", ipid);

                            ids[attemp] = ipid;
                            windows[attemp] = windows_size;
                            respuestas[attemp] = true;

                            tcp_opts_vec[attemp] = tcp_opts;

                            uint8_t count_valid = 0;
                            for(uint8_t i = 0; i <= attemp; i++) {
                                if(respuestas[i]) {
                                    count_valid++;
                                }
                            }

                            if(count_valid >= 6) {
                                break;
                            }

                            printf("- Packet: ");
                            for(uint16_t i = 0; i < packen_len; i++) {
                                printf(
                                    "%02X%c",
                                    app->ethernet->rx_buffer[i],
                                    i == (packen_len - 1) ? '\n' : ' ');
                            }

                            break;
                        }
                    }
                }
            }
        }
        attemp++;
    }

    printf("[OS] ICMP probe started\n");

    if(os_icmp_probe(app, target_ip, &ttl, &rtt)) {
        ttl_icmp = ttl;
        icmp_valid = true;
        printf("[OS] ICMP reply received\n");
        printf("[OS] TTL: %u\n", ttl);
        printf("[OS] RTT: %lu ms\n", rtt);
    } else {
        printf("[OS] ICMP probe failed\n");
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

            else {
                ttl_guess = 255;
            }

            ttl_weight = 10; // TCP pesa más
        } else if(icmp_valid) {
            if(ttl_icmp <= 64)
                ttl_guess = 64;
            else if(ttl_icmp <= 128)
                ttl_guess = 128;
            else
                ttl_guess = 255;

            ttl_weight = 10; // ICMP pesa menos
        }

        /* Asignación de puntuación */
        if(ttl_guess == 64) {
            os_score_add(&sb, LINUX, ttl_weight);
        }

        else if(ttl_guess == 128) {
            os_score_add(&sb, WINDOWS, ttl_weight);
        }

        else if(ttl_guess == 255) {
            os_score_add(&sb, IOS, ttl_weight);
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

            /* Calcular varianza manual para peso dinámico */
            int32_t diffs[100] = {0};
            for(int i = 0; i < sum_true - 1; i++) {
                diffs[i] = ipid_diff(ids_an[i], ids_an[i + 1]);
            }

            double var = varianza(diffs, sum_true - 1);

            const char* ipid_names[] = {
                "UNKNOWN", "CONSTANT", "INCREMENTAL", "INCREMENTAL_LARGE", "RANDOM", "ZERO"};

            printf("[IPID] Pattern: %s\n", ipid_names[pattern]);
            printf("[IPID] Variance: %.2f\n", var);

            switch(pattern) {
            case IPID_ZERO:
                os_score_add(&sb, LINUX, 12);
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
                if(var > 500000) {
                    os_score_add(&sb, IOS, 15);
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

                    printf("[IPID] Pattern: %s\n", ipid_names[pattern]);
                }
            }
        }

        /* ---------- TCP OPTIONS GLOBAL SCORING ---------- */

        int linux_opt = 0;
        int windows_opt = 0;

        for(uint8_t i = 0; i < sum_true; i++) {
            uint8_t guess = clasificar_tcp_options(&tcp_opts_vec[i]);

            if(guess == LINUX) linux_opt++;
            if(guess == WINDOWS) windows_opt++;
        }

        if(linux_opt >= 3) {
            os_score_add(&sb, LINUX, 20);
            printf("[TCP OPTS] Majority Linux signature\n");
        }

        if(windows_opt >= 3) {
            os_score_add(&sb, WINDOWS, 20);
            printf("[TCP OPTS] Majority Windows signature\n");
        }

        /* ---------- CONSISTENCY BONUS ---------- */

        if(linux_opt >= 5) {
            os_score_add(&sb, LINUX, 10);
        }
        if(windows_opt >= 5) {
            os_score_add(&sb, WINDOWS, 10);
        }

        /* ---------- WINDOW SCORING ---------- */
        uint8_t value_win = NO_DETECTED;
        clasificar_window(windows, sum_true, &value_win);

        if(value_win == LINUX)
            os_score_add(&sb, LINUX, 10);
        else if(value_win == WINDOWS)
            os_score_add(&sb, WINDOWS, 10);
    }

    /* ---------- CROSS SIGNAL CONSISTENCY ---------- */

    if(ttl_guess == 64 && sb.linux_score > sb.windows_score) os_score_add(&sb, LINUX, 5);

    if(ttl_guess == 128 && sb.windows_score > sb.linux_score) os_score_add(&sb, WINDOWS, 5);

    if(ttl_guess == 255 && sb.ios_score > sb.linux_score) os_score_add(&sb, IOS, 5);

    printf("\n[SCORE]\n");
    printf("WINDOWS: %d\n", sb.windows_score);
    printf("LINUX:   %d\n", sb.linux_score);
    printf("IOS:     %d\n\n", sb.ios_score);

    return os_score_resolve(&sb);
}
