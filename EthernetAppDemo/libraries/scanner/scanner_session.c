#include "scanner_session.h"
#include "../chip/enc28j60.h"
#include "../chip/rx_dispatch.h"
#include "../../modules/arp_module.h"

void scanner_session_init(scanner_session_t* s, App* app) {
    furi_assert(s);
    furi_assert(app);
    furi_assert(app->ethernet);

    s->ethernet = app->ethernet;
    s->view_dispatcher = app->view_dispatcher;
    s->ip_gateway = app->ip_gateway;
    s->mac_gateway = app->mac_gateway;
    s->subnet_mask = app->ethernet->subnet_mask;
    s->cache_next = 0;
    for(uint8_t i = 0; i < SCANNER_RESOLVE_CACHE_ENTRIES; i++) {
        s->cache[i].valid = false;
    }
}

void scanner_session_deinit(scanner_session_t* s) {
    UNUSED(s);
    // No-op for now. F0.4 will add RX handler unsubscription here.
}

static bool same_subnet(const uint8_t a[4], const uint8_t b[4], const uint8_t mask[4]) {
    return ((*(uint32_t*)a) & (*(uint32_t*)mask)) ==
           ((*(uint32_t*)b) & (*(uint32_t*)mask));
}

static bool cache_lookup(scanner_session_t* s, const uint8_t ip[4], uint8_t mac_out[6]) {
    for(uint8_t i = 0; i < SCANNER_RESOLVE_CACHE_ENTRIES; i++) {
        if(s->cache[i].valid && memcmp(s->cache[i].ip, ip, 4) == 0) {
            memcpy(mac_out, s->cache[i].mac, 6);
            return true;
        }
    }
    return false;
}

static void cache_insert(scanner_session_t* s, const uint8_t ip[4], const uint8_t mac[6]) {
    uint8_t slot = s->cache_next;
    memcpy(s->cache[slot].ip, ip, 4);
    memcpy(s->cache[slot].mac, mac, 6);
    s->cache[slot].valid = true;
    s->cache_next = (slot + 1) % SCANNER_RESOLVE_CACHE_ENTRIES;
}

bool scanner_resolve_next_hop(
    scanner_session_t* s,
    const uint8_t target_ip[4],
    uint8_t mac_out[6]) {
    furi_assert(s);
    furi_assert(target_ip);
    furi_assert(mac_out);

    const uint8_t* resolve_ip;
    if(same_subnet(target_ip, s->ethernet->ip_address, s->subnet_mask)) {
        resolve_ip = target_ip;
    } else {
        resolve_ip = s->ip_gateway;
    }

    if(cache_lookup(s, resolve_ip, mac_out)) {
        return true;
    }

    // F0.4b — pause rx_dispatch around arp_get_specific_mac so its inline
    // poll loop has exclusive chip access. Stop-gap: arp_get_specific_mac
    // still uses receive_packet directly. F0.4c will migrate it onto
    // rx_dispatch via a registered ARP-reply handler.
    rx_dispatch_pause();
    bool ok = arp_get_specific_mac(
        s->ethernet,
        s->ethernet->ip_address,
        (uint8_t*)resolve_ip,
        s->ethernet->mac_address,
        mac_out);
    rx_dispatch_resume();

    if(ok) {
        cache_insert(s, resolve_ip, mac_out);
        // Backward-compat: keep app->mac_gateway in sync when the
        // resolved hop IS the gateway.
        if(resolve_ip == s->ip_gateway) {
            memcpy(s->mac_gateway, mac_out, 6);
        }
    }
    return ok;
}

// State for the rx_dispatch handler that scanner_wait_for_packet registers
// for the duration of a single wait. Lives on the stack of the caller of
// scanner_wait_for_packet; the handler runs in the rx_dispatch thread.
typedef struct {
    scanner_packet_predicate_fn user_pred;
    void* user_ctx;
    FuriSemaphore* signal;
    volatile uint16_t matched_len;
    volatile bool matched;
} scanner_wait_state_t;

static bool wait_predicate(const uint8_t* frame, uint16_t len, void* ctx) {
    scanner_wait_state_t* w = (scanner_wait_state_t*)ctx;
    if(w->matched) return false;
    if(w->user_pred(frame, len, w->user_ctx)) {
        w->matched_len = len;
        return true;
    }
    return false;
}

static void wait_signal_handler(const uint8_t* frame, uint16_t len, void* ctx) {
    UNUSED(frame);
    UNUSED(len);
    scanner_wait_state_t* w = (scanner_wait_state_t*)ctx;
    if(!w->matched) {
        w->matched = true;
        furi_semaphore_release(w->signal);
    }
}

bool scanner_wait_for_packet(
    scanner_session_t* s,
    scanner_packet_predicate_fn pred,
    void* pred_ctx,
    uint16_t* len_out,
    uint32_t timeout_ms) {
    furi_assert(s);
    furi_assert(pred);
    furi_assert(len_out);
    UNUSED(s);

    *len_out = 0;

    scanner_wait_state_t state = {
        .user_pred = pred,
        .user_ctx = pred_ctx,
        .signal = furi_semaphore_alloc(1, 0),
        .matched_len = 0,
        .matched = false,
    };
    if(!state.signal) return false;

    rx_handle_t* handle = rx_register(wait_predicate, wait_signal_handler, &state);
    if(!handle) {
        furi_semaphore_free(state.signal);
        return false;
    }

    // Sleep on the semaphore in short slices so we can also poll the back
    // button (cancel). 50 ms per slice keeps cancel latency under 50 ms
    // without busy-polling.
    bool got = false;
    uint32_t start = furi_get_tick();
    while((furi_get_tick() - start) < timeout_ms) {
        if(scanner_cancel_requested(s)) break;
        uint32_t slice = timeout_ms - (furi_get_tick() - start);
        if(slice > 50) slice = 50;
        if(furi_semaphore_acquire(state.signal, slice) == FuriStatusOk) {
            got = true;
            *len_out = state.matched_len;
            break;
        }
    }

    rx_unregister(handle);
    furi_semaphore_free(state.signal);
    return got;
}

bool scanner_cancel_requested(scanner_session_t* s) {
    UNUSED(s);
    return !furi_hal_gpio_read(&gpio_button_back);
}
