#include "os_detector_module.h"

#include "app_user.h"

#include "../libraries/protocol_tools/tcp.h"
#include "../libraries/protocol_tools/ipv4.h"
#include "../libraries/protocol_tools/ethernet_protocol.h"
#include "../libraries/protocol_tools/icmp.h"
#include "../libraries/protocol_tools/arp.h"
#include "tcp_module.h"
#include "ping_module.h"

#define TAG "OsDetector"

#define OS_TCP_PROBE_PORT  80
#define OS_TCP_SOURCE_PORT 5005
#define OS_ICMP_TIMEOUT_MS 1000
#define OS_TCP_TIMEOUT_MS  3000
#define OS_MAX_TCP_OPTS    12

/* ── helpers ─────────────────────────────────────────────────── */

static uint16_t infer_initial_ttl(uint8_t observed) {
    if(observed <= 32) return 32;
    if(observed <= 64) return 64;
    if(observed <= 128) return 128;
    return 255;
}

/* Check if a received packet is addressed to us (MAC match). */
static bool packet_is_for_us(enc28j60_t* eth) {
    return (*(uint32_t*)eth->mac_address == *(uint32_t*)eth->rx_buffer) &&
           (*(uint16_t*)(eth->mac_address + 4) == *(uint16_t*)(eth->rx_buffer + 4));
}

/* ── ICMP probe ──────────────────────────────────────────────── */

static bool os_icmp_probe(App* app, uint8_t* target_ip, uint8_t* target_mac, uint8_t* out_ttl) {
    uint32_t start = furi_get_tick();

    uint8_t packet[MAX_FRAMELEN] = {0};

    uint16_t pkt_len = create_flipper_ping_packet(
        packet,
        app->ethernet->mac_address,
        target_mac,
        app->ethernet->ip_address,
        target_ip,
        0xBEEF,
        1,
        (uint8_t*)"OSPROBE",
        7);

    if(pkt_len == 0) return false;

    send_packet(app->ethernet, packet, pkt_len);

    while((furi_get_tick() - start) < OS_ICMP_TIMEOUT_MS) {
        uint16_t len = receive_packet(app->ethernet, app->ethernet->rx_buffer, MAX_FRAMELEN);
        if(len == 0) continue;

        if(is_arp(app->ethernet->rx_buffer)) {
            arp_reply_requested(
                app->ethernet, app->ethernet->rx_buffer, app->ethernet->ip_address);
            continue;
        }

        if(!is_icmp(app->ethernet->rx_buffer)) continue;

        icmp_header_t icmp = icmp_get_header(app->ethernet->rx_buffer);
        if(icmp.type != ICMP_TYPE_ECHO_REPLY) continue;

        ipv4_header_t ip = ipv4_get_header(app->ethernet->rx_buffer);
        *out_ttl = ip.ttl;

        FURI_LOG_D(TAG, "ICMP reply: TTL=%u", ip.ttl);
        return true;
    }

    FURI_LOG_D(TAG, "ICMP probe: no reply");
    return false;
}

/* ── TCP Options parser ──────────────────────────────────────── */

typedef struct {
    uint8_t kinds[OS_MAX_TCP_OPTS]; /* sequence of option kind values */
    uint8_t count; /* how many options found         */
    uint16_t mss; /* MSS value (0 if absent)        */
    uint8_t wscale; /* Window Scale value (0 if absent) */
} TcpOptSignature;

static void parse_tcp_options(tcp_header_t* hdr, TcpOptSignature* sig) {
    memset(sig, 0, sizeof(*sig));

    uint8_t data_offset = (hdr->data_offset_flags[0] >> 4) * 4;
    if(data_offset <= TCP_HEADER_LEN) return;

    uint8_t opts_len = data_offset - TCP_HEADER_LEN;
    uint8_t* opts = hdr->options;
    uint8_t i = 0;

    while(i < opts_len && sig->count < OS_MAX_TCP_OPTS) {
        uint8_t kind = opts[i];

        if(kind == TCP_EOL) {
            break;
        }
        if(kind == TCP_NOP) {
            sig->kinds[sig->count++] = TCP_NOP;
            i++;
            continue;
        }

        /* All other options have a length byte at opts[i+1]. */
        if((i + 1) >= opts_len) break;
        uint8_t olen = opts[i + 1];
        if(olen < 2 || (i + olen) > opts_len) break;

        sig->kinds[sig->count++] = kind;

        if(kind == TCP_MSS && olen == 4) {
            sig->mss = (uint16_t)(opts[i + 2] << 8) | opts[i + 3];
        } else if(kind == TCP_WS && olen == 3) {
            sig->wscale = opts[i + 2];
        }

        i += olen;
    }
}

/* Compare a parsed signature against an expected pattern.
 * `expected` is a zero-terminated array of option kinds.
 * Returns true if sig->kinds matches exactly. */
static bool opts_match(const TcpOptSignature* sig, const uint8_t* expected, uint8_t expected_len) {
    if(sig->count != expected_len) return false;
    return memcmp(sig->kinds, expected, expected_len) == 0;
}

/* ── TCP SYN probe ───────────────────────────────────────────── */

typedef struct {
    bool got_reply;
    uint8_t ttl;
    uint16_t window_size;
    bool df_set;
    TcpOptSignature opts;
} TcpProbeResult;

static void os_tcp_probe(App* app, uint8_t* target_ip, uint8_t* target_mac, TcpProbeResult* res) {
    memset(res, 0, sizeof(*res));

    tcp_send_syn(
        app->ethernet,
        app->ethernet->mac_address,
        app->ethernet->ip_address,
        target_mac,
        target_ip,
        OS_TCP_SOURCE_PORT,
        OS_TCP_PROBE_PORT,
        1,
        0);

    uint32_t start = furi_get_tick();

    while((furi_get_tick() - start) < OS_TCP_TIMEOUT_MS) {
        uint16_t len = receive_packet(app->ethernet, app->ethernet->rx_buffer, MAX_FRAMELEN);
        if(len == 0) continue;

        if(is_arp(app->ethernet->rx_buffer)) {
            arp_reply_requested(
                app->ethernet, app->ethernet->rx_buffer, app->ethernet->ip_address);
            continue;
        }

        if(!is_tcp(app->ethernet->rx_buffer)) continue;
        if(!packet_is_for_us(app->ethernet)) continue;

        tcp_header_t tcp = tcp_get_header(app->ethernet->rx_buffer);

        /* Check it's a SYN-ACK (or RST) from our target port. */
        uint16_t flags = 0;
        bytes_to_uint(&flags, tcp.data_offset_flags, sizeof(uint16_t));
        flags &= 0x01FF;

        uint16_t src_port = 0;
        bytes_to_uint(&src_port, tcp.source_port, sizeof(uint16_t));

        if(src_port != OS_TCP_PROBE_PORT) continue;

        /* RST means port closed – still useful for TTL. */
        if(flags & TCP_RST) {
            ipv4_header_t ip = ipv4_get_header(app->ethernet->rx_buffer);
            res->got_reply = true;
            res->ttl = ip.ttl;
            FURI_LOG_D(TAG, "TCP RST: TTL=%u", ip.ttl);
            return;
        }

        if((flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
            ipv4_header_t ip = ipv4_get_header(app->ethernet->rx_buffer);

            res->got_reply = true;
            res->ttl = ip.ttl;

            uint16_t flags_offset = 0;
            bytes_to_uint(&flags_offset, ip.flags_offset, sizeof(uint16_t));
            res->df_set = (flags_offset & 0x4000) != 0;

            bytes_to_uint(&res->window_size, tcp.window_size, sizeof(uint16_t));

            parse_tcp_options(&tcp, &res->opts);

            FURI_LOG_D(
                TAG,
                "SYN-ACK: TTL=%u Win=%u DF=%u OptCount=%u",
                res->ttl,
                res->window_size,
                res->df_set,
                res->opts.count);

            return;
        }
    }

    FURI_LOG_D(TAG, "TCP probe: no reply");
}

/* ── Classifier ──────────────────────────────────────────────── */

/* Known TCP option signatures (order of kinds in SYN-ACK). */

/* Windows 10/11: MSS, NOP, WScale, NOP, NOP, Timestamp, NOP, NOP, SACK-Permitted */
static const uint8_t OPTS_WIN10[] =
    {TCP_MSS, TCP_NOP, TCP_WS, TCP_NOP, TCP_NOP, TCP_TS, TCP_NOP, TCP_NOP, TCP_SACK_P};
#define OPTS_WIN10_LEN 9

/* Windows 7/XP: MSS, NOP, NOP, SACK-Permitted, NOP, WScale */
static const uint8_t OPTS_WIN7[] = {TCP_MSS, TCP_NOP, TCP_NOP, TCP_SACK_P, TCP_NOP, TCP_WS};
#define OPTS_WIN7_LEN 6

/* Linux (modern): MSS, SACK-Permitted, Timestamp, NOP, WScale */
static const uint8_t OPTS_LINUX[] = {TCP_MSS, TCP_SACK_P, TCP_TS, TCP_NOP, TCP_WS};
#define OPTS_LINUX_LEN 5

/* macOS / iOS: MSS, NOP, WScale, NOP, NOP, Timestamp, SACK-Permitted, EOL */
static const uint8_t OPTS_MACOS[] =
    {TCP_MSS, TCP_NOP, TCP_WS, TCP_NOP, TCP_NOP, TCP_TS, TCP_SACK_P};
#define OPTS_MACOS_LEN 7

/* FreeBSD: MSS, NOP, WScale, SACK-Permitted, Timestamp */
static const uint8_t OPTS_FREEBSD[] = {TCP_MSS, TCP_NOP, TCP_WS, TCP_SACK_P, TCP_TS};
#define OPTS_FREEBSD_LEN 5

static OsType classify_os(uint8_t icmp_ttl, bool icmp_ok, TcpProbeResult* tcp) {
    /* No responses at all. */
    if(!icmp_ok && !tcp->got_reply) return OS_UNKNOWN;

    /* Determine best available TTL source. */
    uint8_t ttl = tcp->got_reply ? tcp->ttl : icmp_ttl;
    uint16_t init_ttl = infer_initial_ttl(ttl);

    FURI_LOG_I(TAG, "Classification: InitTTL=%u WinSize=%u", init_ttl, tcp->window_size);

    /* Network equipment typically uses TTL 255. */
    if(init_ttl == 255) return OS_NETWORK_DEVICE;

    /* If we only have ICMP (port 80 filtered/closed with no useful TCP options). */
    if(!tcp->got_reply || tcp->opts.count == 0) {
        if(init_ttl == 128) return OS_WINDOWS;
        if(init_ttl == 64) return OS_LINUX; /* best guess */
        return OS_UNKNOWN;
    }

    /* ── TTL ~128 family: Windows variants ─────────────────── */
    if(init_ttl == 128) {
        if(opts_match(&tcp->opts, OPTS_WIN10, OPTS_WIN10_LEN)) return OS_WINDOWS;
        if(opts_match(&tcp->opts, OPTS_WIN7, OPTS_WIN7_LEN)) return OS_WINDOWS;
        /* Unknown Windows variant – still Windows by TTL. */
        return OS_WINDOWS;
    }

    /* ── TTL ~64 family: Linux / macOS / FreeBSD / Android ── */
    if(init_ttl == 64) {
        /* macOS/iOS: very distinctive option order. */
        if(opts_match(&tcp->opts, OPTS_MACOS, OPTS_MACOS_LEN)) return OS_MACOS;

        /* FreeBSD: MSS, NOP, WS, SACK, TS. */
        if(opts_match(&tcp->opts, OPTS_FREEBSD, OPTS_FREEBSD_LEN)) return OS_FREEBSD;

        /* Linux pattern: MSS, SACK, TS, NOP, WS. */
        if(opts_match(&tcp->opts, OPTS_LINUX, OPTS_LINUX_LEN)) {
            /* Android vs generic Linux heuristic:
             * Android commonly uses window sizes 14600, 26883, 28960.
             * Modern Linux kernels typically use 29200 or 65535. */
            if(tcp->window_size == 14600 || tcp->window_size == 26883 ||
               tcp->window_size == 28960) {
                return OS_ANDROID;
            }
            return OS_LINUX;
        }

        /* Fallback for TTL 64 with unrecognised options. */
        return OS_LINUX;
    }

    return OS_UNKNOWN;
}

/* ── Public API ──────────────────────────────────────────────── */

void os_scan(void* context, uint8_t* target_ip, OsResult* result) {
    App* app = context;

    result->type = OS_UNKNOWN;
    result->ttl = 0;
    result->window_size = 0;

    /* Step 1: Resolve target MAC via ARP. */
    uint8_t target_mac[6] = {0};
    bool same_subnet = (*(uint32_t*)app->ip_gateway & *(uint32_t*)app->ethernet->subnet_mask) ==
                       (*(uint32_t*)target_ip & *(uint32_t*)app->ethernet->subnet_mask);

    if(!arp_get_specific_mac(
           app->ethernet,
           app->ethernet->ip_address,
           same_subnet ? target_ip : app->ip_gateway,
           app->ethernet->mac_address,
           target_mac)) {
        FURI_LOG_E(TAG, "ARP resolution failed");
        return;
    }

    /* Step 2: ICMP probe – get TTL and verify host is alive. */
    uint8_t icmp_ttl = 0;
    bool icmp_ok = os_icmp_probe(app, target_ip, target_mac, &icmp_ttl);

    /* Step 3: TCP SYN probe – get TTL, Window Size, Options, DF. */
    TcpProbeResult tcp = {0};
    os_tcp_probe(app, target_ip, target_mac, &tcp);

    /* Step 4: Classify. */
    result->type = classify_os(icmp_ttl, icmp_ok, &tcp);
    result->ttl = tcp.got_reply ? tcp.ttl : icmp_ttl;
    result->window_size = tcp.window_size;

    FURI_LOG_I(
        TAG, "Result: type=%u ttl=%u win=%u", result->type, result->ttl, result->window_size);
}
