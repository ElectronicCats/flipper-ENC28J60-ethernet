# F0.3a — Scanner Session POC + ping_thread Migration

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development.

**Goal:** Build the new `scanner_session_t` primitive in `libraries/scanner/scanner_session.{c,h}` and migrate `ping_thread` as the proof-of-concept. Subsequent sub-phases (F0.3b–f) migrate the other 5 scanners onto this primitive.

**Architecture:** The session is a stack-allocated value type that captures the chip handle, view-dispatcher reference, and a 4-entry MAC resolution cache. It exposes:

- `scanner_session_init(s, app)` / `scanner_session_deinit(s)` — lifecycle.
- `scanner_resolve_next_hop(s, target_ip, mac_out)` — given a target IPv4, returns the MAC of the next hop (subnet-aware: target itself if same /24-or-mask, otherwise the gateway). Caches the result. Used by `ping_thread` in F0.3a; will be reused by `tcp_syn_scan`, `udp_port_scan`, `os_scan`, `arp_get_specific_mac` callers.
- `scanner_wait_for_packet(s, pred, pred_ctx, len_out, timeout_ms)` — blocks until a frame matching `pred` arrives or the timeout elapses. Defined now; not called in F0.3a; first caller is F0.3c (`arp_scan_network`).
- `scanner_cancel_requested(s)` — non-blocking back-button check.

**Tech Stack:** C, Flipper FAP. No new SDK dependencies — uses existing `enc28j60_t`, `arp_get_specific_mac`, `furi_get_tick`, `furi_hal_gpio_read(&gpio_button_back)`, `furi_delay_us`.

---

## File Structure

Files created in F0.3a:

- `EthernetAppDemo/libraries/scanner/scanner_session.h`
- `EthernetAppDemo/libraries/scanner/scanner_session.c`

Files modified in F0.3a:

- `EthernetAppDemo/scenes/PingScene.c` — `ping_thread` switches its subnet-check + ARP-resolve block to a single call to `scanner_resolve_next_hop`. No other behavior change.
- `EthernetAppDemo/app_user.h` — `#include "libraries/scanner/scanner_session.h"` after the App typedef (same pattern F0.2 used for `settings.h`).

The cache lives inside the session struct (no heap), so no `_alloc`/`_free` pair — caller stack-allocates `scanner_session_t s; scanner_session_init(&s, app);` and the deinit is a no-op for now (kept for future expansion).

---

## API Sketch (informative — implementer codes from this)

```c
// scanner_session.h
#pragma once
#include "../../app_user.h"

#define SCANNER_RESOLVE_CACHE_ENTRIES 4

typedef bool (*scanner_packet_predicate_fn)(const uint8_t* frame, uint16_t len, void* ctx);

typedef struct {
    enc28j60_t* ethernet;
    ViewDispatcher* view_dispatcher;
    uint8_t* ip_gateway;          // borrowed pointer to App.ip_gateway (4 bytes)
    uint8_t* mac_gateway;         // borrowed pointer to App.mac_gateway (6 bytes)
    uint8_t* subnet_mask;         // borrowed pointer to ethernet->subnet_mask (4 bytes)
    struct {
        uint8_t ip[4];
        uint8_t mac[6];
        bool valid;
    } cache[SCANNER_RESOLVE_CACHE_ENTRIES];
    uint8_t cache_next;           // round-robin replacement
} scanner_session_t;

void scanner_session_init(scanner_session_t* s, App* app);
void scanner_session_deinit(scanner_session_t* s);

bool scanner_resolve_next_hop(
    scanner_session_t* s,
    const uint8_t target_ip[4],
    uint8_t mac_out[6]);

bool scanner_wait_for_packet(
    scanner_session_t* s,
    scanner_packet_predicate_fn pred,
    void* pred_ctx,
    uint16_t* len_out,
    uint32_t timeout_ms);

bool scanner_cancel_requested(scanner_session_t* s);
```

---

## Task 1: Implement `libraries/scanner/scanner_session.{c,h}`

**Files:**
- Create: `EthernetAppDemo/libraries/scanner/scanner_session.h`
- Create: `EthernetAppDemo/libraries/scanner/scanner_session.c`

- [ ] **Step 1: Create the directory and header**

```bash
mkdir -p EthernetAppDemo/libraries/scanner
```

`scanner_session.h`:

```c
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
 * Initialize a session. Borrows pointers from App; caller must keep App alive
 * for the session's lifetime. Stack-allocate the session in the worker thread.
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
 *
 * Replaces the inline subnet check + arp_get_specific_mac block duplicated
 * in 4+ scenes.
 */
bool scanner_resolve_next_hop(
    scanner_session_t* s,
    const uint8_t target_ip[4],
    uint8_t mac_out[6]);

/**
 * Block until a received frame satisfies pred(frame, len, pred_ctx) or
 * timeout_ms elapses. On match, copies the matched length into *len_out
 * (the frame itself stays in ethernet->rx_buffer; caller reads from there
 * before invoking the next chip operation). On timeout, *len_out=0 and
 * returns false.
 *
 * Honors cancel: if the back button is pressed during the wait, returns
 * false immediately with *len_out=0 (caller can distinguish via
 * scanner_cancel_requested(s) afterward).
 *
 * Defined in F0.3a; first caller in F0.3c (arp_scan_network).
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
```

- [ ] **Step 2: Implement `scanner_session.c`**

```c
#include "scanner_session.h"
#include "../chip/enc28j60.h"
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

    bool ok = arp_get_specific_mac(
        s->ethernet,
        s->ethernet->ip_address,
        (uint8_t*)resolve_ip,
        s->ethernet->mac_address,
        mac_out);

    if(ok) {
        cache_insert(s, resolve_ip, mac_out);
        // For backward compatibility with code that still reads
        // app->mac_gateway directly, also keep that updated when the
        // resolved IP IS the gateway.
        if(resolve_ip == s->ip_gateway) {
            memcpy(s->mac_gateway, mac_out, 6);
        }
    }
    return ok;
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

    *len_out = 0;
    uint32_t start = furi_get_tick();
    uint8_t* rx = s->ethernet->rx_buffer;

    while((furi_get_tick() - start) < timeout_ms) {
        if(scanner_cancel_requested(s)) return false;
        uint16_t got = receive_packet(s->ethernet, rx, MAX_FRAMELEN);
        if(got > 0 && pred(rx, got, pred_ctx)) {
            *len_out = got;
            return true;
        }
        furi_delay_us(1);
    }
    return false;
}

bool scanner_cancel_requested(scanner_session_t* s) {
    UNUSED(s);
    return !furi_hal_gpio_read(&gpio_button_back);
}
```

Notes for the implementer:
- The session does not own any heap; init/deinit are cheap. The implementer may NOT add allocations in F0.3a.
- `arp_get_specific_mac` signature is `bool arp_get_specific_mac(enc28j60_t*, uint8_t* sender_ip, uint8_t* target_ip, uint8_t* sender_mac, uint8_t* target_mac)` per `arp_module.h:79+`. Verify before using.
- `enc28j60_t.subnet_mask[4]` — verify field exists in `enc28j60.h`. Should — DHCP module writes to it. If the field name differs (e.g. `subnetmask`), adjust the init function.
- `gpio_button_back` is a global `GpioPin` from Flipper SDK (`furi_hal_gpio.h`); `furi_hal_gpio_read` returns the level (high = released, low = pressed). The `!` inverts that.
- `receive_packet` signature: `uint16_t receive_packet(enc28j60_t*, uint8_t* buf, uint16_t max_len)` per `enc28j60.h:124+`.

- [ ] **Step 3: Add include to `app_user.h`**

Use Edit. Find:

```c
#include "libraries/settings/settings.h"
```

Replace with:

```c
#include "libraries/settings/settings.h"
#include "libraries/scanner/scanner_session.h"
```

(Same placement rationale as F0.2 — after the App typedef. The settings.h include is already in the right position; scanner_session.h goes immediately below it.)

- [ ] **Step 4: Build green**

```bash
cd EthernetAppDemo && ufbt 2>&1 | tail -10 && cd .. && git checkout -- EthernetAppDemo/dist/
```

Expected: `FAP` produced, no errors.

If the build fails on `subnet_mask` field reference: read `enc28j60.h` to find the actual field name and adjust the init function.

If it fails on `arp_get_specific_mac` signature mismatch: confirm via `grep -nE "^bool arp_get_specific_mac" EthernetAppDemo/modules/arp_module.{c,h}`.

- [ ] **Step 5: Commit**

```bash
git add EthernetAppDemo/libraries/scanner/ EthernetAppDemo/app_user.h
git commit -m "$(cat <<'COMMIT_EOF'
feat(f0.3a): add scanner_session_t primitive

New libraries/scanner/scanner_session.{c,h} with:

  - scanner_session_init / _deinit (stack-allocated, no heap)
  - scanner_resolve_next_hop  — subnet-aware ARP resolve with 4-entry
                                round-robin cache, replaces the inline
                                subnet-check + arp_get_specific_mac
                                block duplicated across 4+ scenes
  - scanner_wait_for_packet   — generic predicate+timeout poll loop;
                                first caller is F0.3c (arp_scan_network)
  - scanner_cancel_requested  — non-blocking back-button check

This commit adds the primitive but no scanner is migrated yet.
F0.3a Task 2 migrates ping_thread as the POC.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
COMMIT_EOF
)"
```

---

## Task 2: Migrate `ping_thread` to use `scanner_resolve_next_hop`

**Files:** `EthernetAppDemo/scenes/PingScene.c` only.

`ping_thread` currently does (lines 336-350):

```c
// Get the MAC gateway
if(!arp_get_specific_mac(
       ethernet,
       app->ethernet->ip_address,
       (*(uint32_t*)ethernet->ip_address & *(uint32_t*)ethernet->subnet_mask) ==
               (*(uint32_t*)app->scan_params.ip_ping &
                *(uint32_t*)ethernet->subnet_mask) ?
           app->scan_params.ip_ping :
           app->ip_gateway,
       app->ethernet->mac_address,
       app->mac_gateway) &&
   start_ping && is_connected) {
    start_ping = false;
} else {
    memcpy(mac_to_send, app->mac_gateway, 6);
}
```

This block does the subnet check + MAC resolve inline. Migrate to `scanner_resolve_next_hop`.

- [ ] **Step 1: Read the current file state**

```bash
grep -n "ping_thread\|scanner_session\|arp_get_specific_mac\|scan_params.ip_ping" EthernetAppDemo/scenes/PingScene.c
```

Confirm the lines around 336-350 still match the description above (commit may have shifted them by ±5).

- [ ] **Step 2: Add session local at the top of `ping_thread`**

Use Edit. Find the variable declarations near the start of `ping_thread` — specifically the `uint8_t mac_to_send[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};` line.

Replace:

```c
    // Array to get the MAC for the GATEWAY
    uint8_t mac_to_send[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
```

with:

```c
    // Array to get the MAC for the next hop (target if on-subnet, else gateway).
    uint8_t mac_to_send[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    // F0.3a — scanner session (subnet-aware MAC resolve + cache).
    scanner_session_t scanner;
    scanner_session_init(&scanner, app);
```

- [ ] **Step 3: Replace the inline ARP resolve with `scanner_resolve_next_hop`**

Use Edit. Find the block:

```c
    // Get the MAC gateway
    if(!arp_get_specific_mac(
           ethernet,
           app->ethernet->ip_address,
           (*(uint32_t*)ethernet->ip_address & *(uint32_t*)ethernet->subnet_mask) ==
                   (*(uint32_t*)app->scan_params.ip_ping &
                    *(uint32_t*)ethernet->subnet_mask) ?
               app->scan_params.ip_ping :
               app->ip_gateway,
           app->ethernet->mac_address,
           app->mac_gateway) &&
       start_ping && is_connected) {
        start_ping = false;
    } else {
        memcpy(mac_to_send, app->mac_gateway, 6);
    }
```

Replace with:

```c
    // F0.3a — resolve next-hop MAC for the ping target.
    // scanner_resolve_next_hop handles subnet check + ARP + cache, and
    // also updates app->mac_gateway when the resolved hop IS the gateway.
    if(start_ping && is_connected &&
       !scanner_resolve_next_hop(&scanner, app->scan_params.ip_ping, mac_to_send)) {
        start_ping = false;
    }
```

The semantic is identical:
- If resolution succeeds, `mac_to_send` is populated and `start_ping` stays true.
- If resolution fails, `start_ping` becomes false and the loop is skipped.
- Note: previously, the success path did `memcpy(mac_to_send, app->mac_gateway, 6)`. That assumed the resolved IP was always the gateway, which is only true when the target was off-subnet. In the on-subnet case, the previous code wrote the target's MAC into `app->mac_gateway` (a side effect that confused state). The new code copies the resolved MAC into `mac_to_send` directly via the `mac_out` parameter, AND only updates `app->mac_gateway` when the resolved hop genuinely IS the gateway — fixing a latent state-pollution bug.

- [ ] **Step 4: Add `scanner_session_deinit` before `finalize:`**

Use Edit. Find:

```c
    furi_delay_ms(1);

finalize:

    return 0;
}
```

Replace with:

```c
    furi_delay_ms(1);

finalize:
    scanner_session_deinit(&scanner);

    return 0;
}
```

- [ ] **Step 5: Build green**

```bash
cd EthernetAppDemo && ufbt 2>&1 | tail -10 && cd .. && git checkout -- EthernetAppDemo/dist/
```

Common errors:
- `'scanner' undeclared` — Step 2 didn't apply; Edit's `old_string` may not have matched. Re-grep, fix.
- `'scanner_session_init' undeclared implicit` — `app_user.h` is missing the include from F0.3a Task 1 Step 3.
- Implicit declaration of `arp_get_specific_mac` somewhere — should be transitively included via `app_user.h` → `modules/arp_module.h`. Verify no warnings.

- [ ] **Step 6: Verify the inline `arp_get_specific_mac` reference is gone**

```bash
grep -n "arp_get_specific_mac" EthernetAppDemo/scenes/PingScene.c
```

Expected: zero matches in PingScene.c. (Other scenes still call it directly until F0.3b-f migrate them.)

- [ ] **Step 7: Commit**

```bash
git status   # only PingScene.c modified
git add EthernetAppDemo/scenes/PingScene.c
git commit -m "$(cat <<'COMMIT_EOF'
refactor(f0.3a): migrate ping_thread to scanner_resolve_next_hop

Replaces the inline subnet check + arp_get_specific_mac block with a
single scanner_resolve_next_hop() call from F0.3a's new primitive.

Side effect fix: the previous code unconditionally wrote the resolved
MAC into app->mac_gateway, even when the resolved IP was the
on-subnet target (not the gateway). The new code only updates
app->mac_gateway when the resolved hop genuinely IS the gateway.

This is the POC migration. F0.3b-f migrate the remaining 5 scanners
(arpspoofing_thread, arp_scan_network, udp_port_scan, tcp_syn_scan,
os_scan) onto the same primitive.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
COMMIT_EOF
)"
```

---

## Task 3: Final verify + tag `v2.0-f0.3a`

- [ ] **Step 1: Re-run build, capture FAP size**

```bash
cd EthernetAppDemo && ufbt 2>&1 > /dev/null && ls -la dist/ethernet_app.fap && cd ..
git checkout -- EthernetAppDemo/dist/
```

Note the byte size — record in tag message.

- [ ] **Step 2: Verify deliverables**

```bash
ls -la \
  EthernetAppDemo/libraries/scanner/scanner_session.h \
  EthernetAppDemo/libraries/scanner/scanner_session.c
grep -nE "scanner_resolve_next_hop|scanner_session_init|scanner_session_deinit" EthernetAppDemo/scenes/PingScene.c
grep -nE "scanner_session.h" EthernetAppDemo/app_user.h
```

Expected:
- 2 files in `libraries/scanner/`.
- 3 references in PingScene.c (init + resolve + deinit).
- 1 include line in app_user.h.

- [ ] **Step 3: Verify clean tree, commit count**

```bash
git status
git log --oneline v2.0-f0.2..HEAD
```

Expected: tree clean; 3 commits since v2.0-f0.2 (plan + Task 1 + Task 2).

- [ ] **Step 4: Annotated tag**

```bash
git tag -a v2.0-f0.3a -m "$(cat <<'EOF'
F0.3a — Scanner session primitive + ping_thread POC

Adds libraries/scanner/scanner_session.{c,h}:
  - scanner_session_init / _deinit (stack-allocated, no heap)
  - scanner_resolve_next_hop  (subnet-aware ARP + 4-entry MAC cache)
  - scanner_wait_for_packet   (predicate + timeout; defined now,
                               first caller is F0.3c)
  - scanner_cancel_requested  (back-button check)

POC migration: ping_thread replaces ~15 lines of inline subnet check
+ arp_get_specific_mac with a single scanner_resolve_next_hop call.
Latent bug fixed in the process — the previous code wrote target MAC
into app->mac_gateway unconditionally; new code only updates that
field when the resolved hop IS the gateway.

Remaining migrations queued for sub-phases:
  F0.3b — arpspoofing_thread
  F0.3c — arp_scan_network        (first scanner_wait_for_packet user)
  F0.3d — udp_port_scan
  F0.3e — tcp_syn_scan
  F0.3f — os_scan                  (closes F0.3 with v2.0-f0.3 tag)

Build verified green. FAP size at F0.3a close: <FAP_BYTES> bytes
(was 97040 at F0.2 close).
Target: 7, API: 87.8 (Unleashed unlshd-087)

Pending smoke test (manual on hardware):
  - PingScene: ping a known on-subnet host (ARP resolves target).
  - PingScene: ping a known off-subnet host (ARP resolves gateway).
  - Both must produce the same response counts as pre-F0.3a.
EOF
)"
```

Replace `<FAP_BYTES>` with the value from Step 1.

- [ ] **Step 5: Verify tag and report**

```bash
git tag --list | grep v2.0-f0
git show v2.0-f0.3a --stat | head -15
```

---

## Self-review

**Spec coverage** (against master plan §6 F0.3 partial):

- "Generic `scanner_session_t` with `scanner_resolve_target`, `scanner_wait_for_packet`, `scanner_progress`" — F0.3a delivers `scanner_session_t` with `_resolve_next_hop` (renamed from `_resolve_target` for clarity — the function returns the next-hop MAC, not the target's MAC, when off-subnet) and `_wait_for_packet`. The `_progress` primitive is **deferred** because none of the 6 scanners has a uniform progress shape — direct `view_dispatcher_send_custom_event(s->view_dispatcher, code)` is one line and adding a wrapper buys nothing yet. **Decision: keep it inline; revisit if a non-trivial progress pattern emerges in F0.3c-f.**
- "Migrate 6 scanners" — F0.3a migrates 1 (ping_thread). The other 5 are queued for F0.3b-f, each its own sub-phase + tag, per the user's choice of incremental approach.

**Placeholder scan:** `<FAP_BYTES>` is a runtime value filled in Step 4. Everything else concrete.

**Type/identifier consistency:** `scanner_session_t`, `scanner_resolve_next_hop`, `scanner_wait_for_packet`, `scanner_cancel_requested` used consistently. `SCANNER_RESOLVE_CACHE_ENTRIES` capitalized macro. Field names match across header and impl.

---
