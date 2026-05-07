#include "rx_dispatch.h"

#define RX_DISPATCH_MAX_HANDLERS 8
#define RX_DISPATCH_STACK_BYTES  4096

struct rx_handle {
    rx_predicate_fn predicate;
    rx_handler_fn handler;
    void* ctx;
    bool in_use;
};

typedef struct {
    enc28j60_t* ethernet;
    FuriThread* thread;
    FuriMutex* mutex;
    volatile bool running;
    struct rx_handle slots[RX_DISPATCH_MAX_HANDLERS];
} rx_dispatch_t;

static rx_dispatch_t g_dispatch;

static int32_t rx_dispatch_thread_fn(void* context) {
    rx_dispatch_t* d = (rx_dispatch_t*)context;
    enc28j60_t* eth = d->ethernet;
    uint8_t* rx = eth->rx_buffer;

    while(d->running) {
        uint16_t len = receive_packet(eth, rx, MAX_FRAMELEN);
        if(len == 0) {
            furi_delay_us(100);
            continue;
        }

        // Snapshot handlers under mutex so a concurrent register/unregister
        // doesn't tear the iteration. Each call is short; the mutex isn't
        // held while handlers run.
        struct rx_handle snapshot[RX_DISPATCH_MAX_HANDLERS];
        furi_mutex_acquire(d->mutex, FuriWaitForever);
        memcpy(snapshot, d->slots, sizeof(snapshot));
        furi_mutex_release(d->mutex);

        for(uint8_t i = 0; i < RX_DISPATCH_MAX_HANDLERS; i++) {
            if(!snapshot[i].in_use) continue;
            if(snapshot[i].predicate(rx, len, snapshot[i].ctx)) {
                snapshot[i].handler(rx, len, snapshot[i].ctx);
            }
        }
    }
    return 0;
}

void rx_dispatch_init(App* app) {
    furi_assert(app);
    furi_assert(app->ethernet);

    if(g_dispatch.running) return;

    memset(&g_dispatch, 0, sizeof(g_dispatch));
    g_dispatch.ethernet = app->ethernet;
    g_dispatch.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    g_dispatch.running = true;
    g_dispatch.thread = furi_thread_alloc_ex(
        "RX Dispatch", RX_DISPATCH_STACK_BYTES, rx_dispatch_thread_fn, &g_dispatch);
    furi_thread_start(g_dispatch.thread);
}

void rx_dispatch_deinit(App* app) {
    UNUSED(app);
    if(!g_dispatch.running) return;

    g_dispatch.running = false;
    furi_thread_join(g_dispatch.thread);
    furi_thread_free(g_dispatch.thread);
    g_dispatch.thread = NULL;

    furi_mutex_free(g_dispatch.mutex);
    g_dispatch.mutex = NULL;
}

rx_handle_t* rx_register(rx_predicate_fn predicate, rx_handler_fn handler, void* ctx) {
    furi_assert(predicate);
    furi_assert(handler);
    if(!g_dispatch.mutex) return NULL;

    furi_mutex_acquire(g_dispatch.mutex, FuriWaitForever);
    rx_handle_t* slot = NULL;
    for(uint8_t i = 0; i < RX_DISPATCH_MAX_HANDLERS; i++) {
        if(!g_dispatch.slots[i].in_use) {
            g_dispatch.slots[i].predicate = predicate;
            g_dispatch.slots[i].handler = handler;
            g_dispatch.slots[i].ctx = ctx;
            g_dispatch.slots[i].in_use = true;
            slot = &g_dispatch.slots[i];
            break;
        }
    }
    furi_mutex_release(g_dispatch.mutex);
    return slot;
}

void rx_unregister(rx_handle_t* handle) {
    if(!handle || !g_dispatch.mutex) return;
    furi_mutex_acquire(g_dispatch.mutex, FuriWaitForever);
    handle->in_use = false;
    handle->predicate = NULL;
    handle->handler = NULL;
    handle->ctx = NULL;
    furi_mutex_release(g_dispatch.mutex);
}
