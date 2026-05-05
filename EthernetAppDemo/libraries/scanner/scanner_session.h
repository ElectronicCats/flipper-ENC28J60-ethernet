#pragma once
#include "../../app_user.h"

#define SCANNER_RESOLVE_CACHE_ENTRIES 4

/**
 * Predicate over a received frame. Return true if the frame matches.
 * The frame and length are valid only for the duration of the call.
 */
typedef bool (*scanner_packet_predicate_fn)(const uint8_t* frame, uint16_t len, void* ctx);

typedef struct {
    enc28j60_t* ethernet;
    ViewDispatcher* view_dispatcher;
    uint8_t* ip_gateway;       // borrowed: App.ip_gateway (4 bytes)
    uint8_t* mac_gateway;      // borrowed: App.mac_gateway (6 bytes)
    uint8_t* subnet_mask;      // borrowed: ethernet->subnet_mask (4 bytes)

    // Round-robin MAC cache. Avoids paying ARP-resolve cost on every call.
    struct {
        uint8_t ip[4];
        uint8_t mac[6];
        bool valid;
    } cache[SCANNER_RESOLVE_CACHE_ENTRIES];
    uint8_t cache_next;
} scanner_session_t;

/**
 * Initialize a session. Borrows pointers from App; caller must keep App
 * alive for the session's lifetime. Stack-allocate the session in the
 * worker thread.
 */
void scanner_session_init(scanner_session_t* s, App* app);

/**
 * Currently a no-op (no heap inside the session). Kept for future expansion
 * (e.g. when F0.4 plumbs a per-session RX handler subscription that needs
 * unregistration here).
 */
void scanner_session_deinit(scanner_session_t* s);

/**
 * Given a target IPv4, return via mac_out the MAC of the next hop:
 *   - if target_ip is on the local subnet (per ethernet->subnet_mask),
 *     ARP-resolve target_ip directly;
 *   - otherwise, return the cached gateway MAC.
 *
 * Cached results are reused. Cache misses pay one arp_get_specific_mac
 * call, which today blocks up to ~20 s. Returns true on success.
 */
bool scanner_resolve_next_hop(
    scanner_session_t* s,
    const uint8_t target_ip[4],
    uint8_t mac_out[6]);

/**
 * Block until a received frame satisfies pred(frame, len, pred_ctx) or
 * timeout_ms elapses. On match, copies the matched length into *len_out
 * (the frame itself stays in ethernet->rx_buffer; caller reads from
 * there before invoking the next chip operation). On timeout, *len_out=0
 * and returns false.
 *
 * Honors cancel: if the back button is pressed during the wait, returns
 * false immediately with *len_out=0.
 */
bool scanner_wait_for_packet(
    scanner_session_t* s,
    scanner_packet_predicate_fn pred,
    void* pred_ctx,
    uint16_t* len_out,
    uint32_t timeout_ms);

/**
 * Non-blocking back-button poll. Returns true if the user pressed back.
 */
bool scanner_cancel_requested(scanner_session_t* s);
