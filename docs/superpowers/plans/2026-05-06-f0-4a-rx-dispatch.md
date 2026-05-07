# F0.4a — RX Dispatch + auto-replies migration

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development.

**Goal:** Replace the polling-and-auto-reply loop in `app_worker.c` with a single `rx_dispatch` thread driving a registered-handler list. Migrate the two auto-replies (ARP request → ARP reply, ICMP echo request → ICMP echo reply) as static handlers registered at boot. The DORA flag handling stays inside the worker thread (now stripped of its RX loop) to avoid scope creep.

**Architecture:** `libraries/chip/rx_dispatch.{c,h}` exposes a singleton dispatcher. It owns the chip's RX cycle: a FuriThread loops `receive_packet → for each registered (predicate, handler, ctx); if predicate true → handler runs`. Handlers receive a `const uint8_t*` valid only during the call (copy-once if they need to keep it). Register/unregister use a FuriMutex for thread-safety.

**What stays for later sub-phases:**
- Scanner session still uses `receive_packet` directly (F0.4b migrates it).
- Scene `furi_thread_suspend(app->thread)` calls still exist and keep working — `app_worker.c`'s `ethernet_thread` is kept alive but stripped to a DORA-only service thread, so suspend/resume on it still has well-defined semantics (suspended thread does nothing). F0.4c removes the suspend/resume calls.
- SnifferScene polls directly. F0.4d migrates it.

---

## File Structure

Files created in F0.4a:
- `EthernetAppDemo/libraries/chip/rx_dispatch.h` — public API.
- `EthernetAppDemo/libraries/chip/rx_dispatch.c` — implementation.

Files modified in F0.4a:
- `EthernetAppDemo/app_user.h` — add `#include "libraries/chip/rx_dispatch.h"` (after the App typedef, beside settings.h and scanner_session.h).
- `EthernetAppDemo/app_user.c` — call `rx_dispatch_init(app)` after `enc28j60_alloc()` and `enc28j60_start()`. Register two static handlers (`auto_arp_reply_handler`, `auto_icmp_reply_handler`) right after init. Call `rx_dispatch_deinit(app)` at the start of `app_free()` (before `free_enc28j60`).
- `EthernetAppDemo/app_worker.c` — strip the `receive_packet` polling and auto-reply logic. Keep only the DORA flag service path.

No `app_worker.c` deletion in F0.4a — the file is reduced. Full removal is F0.4c.

---

## API Sketch

```c
// rx_dispatch.h
#pragma once
#include "../../app_user.h"

/**
 * Predicate: examine the frame, return true if this handler should run.
 * Handler:   process the matched frame.
 *
 * Both receive a `const uint8_t* frame` valid only for the duration of
 * the call. The dispatcher owns the chip's rx_buffer; handlers must
 * copy data they want to keep.
 */
typedef bool (*rx_predicate_fn)(const uint8_t* frame, uint16_t len, void* ctx);
typedef void (*rx_handler_fn)(const uint8_t* frame, uint16_t len, void* ctx);

typedef struct rx_handle rx_handle_t;

/** Start the dispatcher thread. Idempotent. */
void rx_dispatch_init(App* app);

/** Stop the dispatcher thread, free state. Must run before chip teardown. */
void rx_dispatch_deinit(App* app);

/**
 * Register a handler. The handler runs in the dispatcher thread context;
 * keep its body short. For long work, push the frame to a FuriMessageQueue
 * and process it in a worker.
 */
rx_handle_t* rx_register(rx_predicate_fn predicate, rx_handler_fn handler, void* ctx);

/** Unregister a previously registered handler. Safe to call from any thread. */
void rx_unregister(rx_handle_t* handle);
```

Internal capacity: 8 handler slots is plenty for now (auto-ARP + auto-ICMP + per-scene wait + sniffer + headroom). Stored as fixed-size array; register/unregister O(1).

---

## Task 1 — Create `rx_dispatch.{c,h}`

**Files:**
- Create: `EthernetAppDemo/libraries/chip/rx_dispatch.h`
- Create: `EthernetAppDemo/libraries/chip/rx_dispatch.c`

`rx_dispatch.h`:

```c
#pragma once
#include "../../app_user.h"

typedef bool (*rx_predicate_fn)(const uint8_t* frame, uint16_t len, void* ctx);
typedef void (*rx_handler_fn)(const uint8_t* frame, uint16_t len, void* ctx);

typedef struct rx_handle rx_handle_t;

void rx_dispatch_init(App* app);
void rx_dispatch_deinit(App* app);

rx_handle_t* rx_register(rx_predicate_fn predicate, rx_handler_fn handler, void* ctx);
void rx_unregister(rx_handle_t* handle);
```

`rx_dispatch.c`:

```c
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
```

Notes:
- Singleton via file-static `g_dispatch`. Future expansion (multi-chip) would extend to per-instance, not needed now.
- Stack 4 KB: handlers run on this stack. Auto-replies allocate small (≤60-byte) ARP/ICMP frames.
- `furi_delay_us(100)` polling sleep: lower than worker thread's old `furi_delay_ms(1)` — chosen to keep RX latency near 100 µs while still yielding to the scheduler. Reconsidered in F0.5 if latency matters more.
- The mutex protects the slot table only. Handlers run **without** the mutex held (safer; users can call `rx_register`/`unregister` from inside other handlers without deadlock).

### Build green

```bash
cd EthernetAppDemo && ufbt 2>&1 | tail -8 && cd .. && git checkout -- EthernetAppDemo/dist/
```

The new file is added but no caller exists yet, so it's dead code (linker may strip). That's expected for this task.

### Commit

```
feat(f0.4a): add rx_dispatch — single chip-RX thread + handler registry
```

---

## Task 2 — Migrate auto-ARP-reply and auto-ICMP-reply

**Files:**
- Modify: `EthernetAppDemo/app_user.h` — `#include "libraries/chip/rx_dispatch.h"` after settings.h / scanner_session.h.
- Modify: `EthernetAppDemo/app_user.c`:
  - Define two static predicate/handler pairs at file scope.
  - Call `rx_dispatch_init(app)` at end of `app_alloc` (after `enc28j60_start`, before `furi_thread_start(app->thread)`).
  - Register the two handlers right after init.
  - Call `rx_dispatch_deinit(app)` at start of `app_free` (before any chip teardown).

The two handlers replace the inline logic that lived in `app_worker.c:42-45`:

```c
// In app_user.c at file scope.

static bool auto_arp_predicate(const uint8_t* frame, uint16_t len, void* ctx) {
    UNUSED(ctx);
    UNUSED(len);
    return is_arp((uint8_t*)frame);
}

static void auto_arp_handler(const uint8_t* frame, uint16_t len, void* ctx) {
    UNUSED(len);
    App* app = (App*)ctx;
    arp_reply_requested(app->ethernet, (uint8_t*)frame, app->ethernet->ip_address);
}

static bool auto_icmp_predicate(const uint8_t* frame, uint16_t len, void* ctx) {
    UNUSED(ctx);
    UNUSED(len);
    return is_icmp((uint8_t*)frame);
}

static void auto_icmp_handler(const uint8_t* frame, uint16_t len, void* ctx) {
    App* app = (App*)ctx;
    ping_reply_to_request(app->ethernet, (uint8_t*)frame, len);
}
```

Add a `rx_handle_t* auto_arp_handle;` and `rx_handle_t* auto_icmp_handle;` field to `App` for unregister-at-shutdown discipline. (Optional — `rx_dispatch_deinit` will free everything anyway. But cleanly storing them is good practice.)

Wiring in `app_alloc`:

```c
    enc28j60_soft_reset(app->ethernet);
    app->enc28j60_connected = enc28j60_start(app->ethernet) != 0xff;

    // F0.4a — start RX dispatcher and register the two auto-reply handlers.
    // These previously lived inline in ethernet_thread (app_worker.c).
    rx_dispatch_init(app);
    app->auto_arp_handle = rx_register(auto_arp_predicate, auto_arp_handler, app);
    app->auto_icmp_handle = rx_register(auto_icmp_predicate, auto_icmp_handler, app);

    app->thread = furi_thread_alloc_ex("Ethernet Thread", 10 * 1024, ethernet_thread, app);
    furi_thread_start(app->thread);
```

Wiring in `app_free`:

```c
void app_free(App* app) {
    settings_save(app);

    // F0.4a — stop dispatcher first so handlers don't race teardown.
    rx_unregister(app->auto_arp_handle);
    rx_unregister(app->auto_icmp_handle);
    rx_dispatch_deinit(app);

    furi_thread_flags_set(furi_thread_get_id(app->thread), flag_stop);
    ...
```

Add the `rx_handle_t*` fields to `App` in `app_user.h`:

```c
    rx_handle_t* auto_arp_handle;
    rx_handle_t* auto_icmp_handle;
```

Build verify and commit.

---

## Task 3 — Strip `ethernet_thread` of receive loop

**File:** `EthernetAppDemo/app_worker.c`

Replace the body. Keep only the DORA service path. The thread remains alive (so existing `furi_thread_suspend(app->thread)` calls in scenes still have a valid target — F0.4c will remove those calls; until then they suspend a thread that isn't doing anything chip-related).

New body:

```c
#include "app_user.h"

int32_t ethernet_thread(void* context) {
    App* app = (App*)context;
    enc28j60_t* ethernet = app->ethernet;

    while(true) {
        uint32_t event = furi_thread_flags_wait(ALL_FLAGS, FuriFlagWaitAny, FuriWaitForever);

        if(event == MASK_FLAGS) continue;

        if(event & flag_stop) {
            break;
        }

        if(event & flag_dhcp_dora) {
            view_dispatcher_send_custom_event(app->view_dispatcher, wait_ip_event);
            if(flipper_process_dora_with_host_name(
                   ethernet,
                   ethernet->ip_address,
                   app->ip_gateway,
                   ethernet->subnet_mask,
                   "Flippa 0")) {
                app->is_dora = true;
                send_arp_gratuitous(ethernet, ethernet->mac_address, ethernet->ip_address);
                view_dispatcher_send_custom_event(app->view_dispatcher, ip_gotten_event);
                app->is_static_ip = true;
            } else {
                view_dispatcher_send_custom_event(app->view_dispatcher, ip_no_gotten_event);
            }
            furi_thread_flags_clear(flag_dhcp_dora);
        }
    }

    return 0;
}
```

Differences vs the old body:
- No more `receive_packet` polling.
- No more inline `arp_reply_requested` / `ping_reply_to_request` calls — those moved to rx_dispatch handlers (Task 2).
- No more 1-ms tight loop. Thread sleeps on `furi_thread_flags_wait` until something's queued.
- The `is_link_up` early-return path is gone. Link state checks belong in scene UI now (or in F0.4d sniffer).
- `IS_NOT_LINK_UP` custom event: was sent when link dropped during the worker's poll. After F0.4a no one sends it. Scenes that needed to show "link down" must detect it themselves (or it's dead). Verify no scene relies on it; if some do, they currently depend on a side effect of the polling worker, which is fragile anyway. Leave the constant defined; flag any user as a follow-up.

Build verify and commit.

---

## Task 4 — Hardware verify + tag

- Build green.
- Flash via `cd EthernetAppDemo && ufbt launch`.
- On Flipper:
  - Get IP (DORA) → must complete (the worker still serves the DORA flag).
  - Ping laptop → must work (auto-ICMP-reply lives in dispatch handler now).
  - Laptop pings Flipper → Flipper auto-replies (auto-ARP + auto-ICMP handlers).
- Verify on laptop:
  - tcpdump shows ICMP echo replies from Flipper to laptop pings.
  - dnsmasq shows DHCPACK.
- Tag `v2.0-f0.4a` with summary.

---

## Self-review

**Spec coverage** (against master plan §4 and §6 F0.4):
- "single rx_dispatch thread + register/unregister handlers": ✅ Task 1.
- "Auto-ARP-reply + auto-ICMP-reply as registered handlers": ✅ Task 2.
- "Delete app_worker.c": ❌ deferred to F0.4c. F0.4a only strips its body. Reason: the file is also the home of the DORA service path, and removing it means refactoring DORA delivery in the same sub-phase. Keeping it as a stub is safer for incremental validation.
- "Migrate scanner scenes": ❌ that's F0.4b/c.

**Type/identifier consistency:** `rx_predicate_fn`, `rx_handler_fn`, `rx_handle_t`, `rx_dispatch_init`, `rx_register`, `rx_unregister` used consistently across header, impl, and call sites.

**Risks (per master plan §4 ownership):**
- R1 frame ownership: `const uint8_t*` valid only during call → enforced by const + comments. Predicate/handler must not call back into chip during execution (would corrupt rx_buffer). Verify Task 2 handlers respect this — `arp_reply_requested` and `ping_reply_to_request` write to `tx_buffer` and call `send_packet`, which is OK because TX/RX use disjoint chip SRAM regions.
- R2 handler latency: auto-replies are ~50-150 µs of SPI work. Acceptable within dispatch loop. SnifferScene's future "capture-all" handler would just memcpy the frame — also fast.
- R3 stack: dispatcher 4 KB, handler stacks share it. ARP/ICMP reply functions don't recurse and use ≤ 100 bytes locals. Safe.
