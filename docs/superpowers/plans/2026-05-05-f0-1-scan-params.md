# F0.1 — Centralize scan_params Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the 7 file-static target/scan globals (3 of which collide on the same name `target_ip[4]`, one of which leaks as a non-`static` symbol) with a single `scan_params_t` struct embedded in `App`, persisted across scenes.

**Architecture:** New struct `scan_params_t` lives in `app_user.h` next to the existing `App` struct definition. `App` gains a `scan_params_t scan_params` member. Each scene replaces references to its file-static via `app->scan_params.<field>`. No new files, no behavior change. Build verification after each scene migration; one commit per scene migration so a regression bisects to the exact scene.

**Tech Stack:** C, Flipper FAP (uFBT). No unit-test framework available; verification is `ufbt` build green + manual smoke-test on hardware (post-F0.1, not gating per-task).

---

## File Structure

Files modified in F0.1:
- `EthernetAppDemo/app_user.h` — add `scan_params_t` struct definition, add `scan_params_t scan_params;` member to `App`.
- `EthernetAppDemo/app_user.c` — initialize `app->scan_params` to sensible defaults at boot.
- `EthernetAppDemo/scenes/PortsScannerScene.c` — drop file-static `target_ip`, `target_port`, `range_port`, `protocols_index`; rewrite call-sites to `app->scan_params.*`.
- `EthernetAppDemo/scenes/OsDetector.c` — drop file-static `target_ip`; rewrite call-sites.
- `EthernetAppDemo/scenes/ArpSpoofingSpecificIP.c` — drop file-static `target_ip`; rewrite call-sites.
- `EthernetAppDemo/scenes/PingScene.c` — drop file-static `ip_ping`; rewrite call-sites.
- `EthernetAppDemo/scenes/ArpScannerScene.c` — drop file-static `ip_start`, `range_ip`; rewrite call-sites.

Files NOT touched in F0.1:
- `target_port_bytes`/`range_port_bytes` in `PortsScannerScene.c:14-15` — dead code per audit; will be removed in F0.6 scaffolding cleanup, not F0.1. Leave them as-is.
- Module signatures in `tcp_module`, `udp_module`, `os_detector_module` — they take `target_ip` as a parameter; passing `app->scan_params.target_ip` works without changing module APIs.

Each task ends with a `git commit` so the history records each migration step independently.

---

## Task 1: Define `scan_params_t` and embed in `App`

**Files:**
- Modify: `EthernetAppDemo/app_user.h`
- Modify: `EthernetAppDemo/app_user.c`

- [ ] **Step 1: Add the struct definition to `app_user.h`**

Use Edit on `EthernetAppDemo/app_user.h`. Insert the struct definition immediately before the `App` struct (which starts at line 70 with `typedef struct {`):

Old context (line 67-70):
```c
typedef enum {
    wait_ip_event = 1,
    ip_no_gotten_event,
    ip_gotten_event,
} get_ip_events;

// Struct for the App
typedef struct {
```

New content (insert between the `}` of `get_ip_events` and the `// Struct for the App` comment):
```c
typedef enum {
    wait_ip_event = 1,
    ip_no_gotten_event,
    ip_gotten_event,
} get_ip_events;

// Cross-scene scan parameters (formerly file-static globals scattered
// across scenes; centralized in F0.1 to remove name collisions and
// enable persistence in F0.2).
typedef struct {
    uint8_t target_ip[4];          // generic target IPv4 (port scan / OS detect / ARP spoof)
    uint16_t target_port;          // single port (port scan start)
    uint16_t range_port;           // count of ports to scan from target_port
    uint8_t protocols_index;       // 0=TCP, 1=UDP (PORTS_SCANNER_TCP/UDP)
    uint8_t ip_ping[4];            // ping target IPv4
    uint8_t ip_start[4];           // ARP scan start IPv4
    uint8_t range_ip;              // ARP scan count
} scan_params_t;

// Struct for the App
typedef struct {
```

- [ ] **Step 2: Add the member to `App`**

Use Edit on `EthernetAppDemo/app_user.h`. Find the existing line:

```c
    uint16_t selected_menu_index;
} App;
```

Replace with:

```c
    uint16_t selected_menu_index;

    scan_params_t scan_params;     // F0.1 — centralized cross-scene targets
} App;
```

- [ ] **Step 3: Initialize defaults in `app_user.c`**

Find where `App* app` is allocated in `app_user.c`. Run:

```bash
grep -n "App\* app\s*=\s*malloc\|App\* app\s*=\s*calloc\|memset(app" EthernetAppDemo/app_user.c
```

Locate the App-construction block. Right after the `App` is allocated/zeroed and before `view_dispatcher_alloc`, add:

```c
    // F0.1 — initialize cross-scene scan parameters with sensible defaults.
    // Matches the prior file-static initial values:
    //   PortsScannerScene.c:11   target_port = 22
    //   PortsScannerScene.c:12   range_port = 1000
    //   PortsScannerScene.c:40   protocols_index = PORTS_SCANNER_TCP
    //   ArpScannerScene.c:17     range_ip = 30
    // Other arrays default to all zeros (already done by calloc/memset).
    app->scan_params.target_port = 22;
    app->scan_params.range_port = 1000;
    app->scan_params.protocols_index = 0; // PORTS_SCANNER_TCP
    app->scan_params.range_ip = 30;
```

If the App is allocated via `malloc` (not `calloc`) and not `memset`-zeroed, also explicitly zero the byte arrays:

```c
    memset(app->scan_params.target_ip, 0, sizeof(app->scan_params.target_ip));
    memset(app->scan_params.ip_ping, 0, sizeof(app->scan_params.ip_ping));
    memset(app->scan_params.ip_start, 0, sizeof(app->scan_params.ip_start));
```

The implementer must inspect the App allocation site to know which case applies.

- [ ] **Step 4: Build green**

```bash
cd EthernetAppDemo && ufbt 2>&1 | tail -8 && cd ..
```

Expected: `FAP: ethernet_app.fap` produced. If the build fails with `unknown type name 'scan_params_t'`, the struct was placed below `App` instead of above. Move it.

After build, restore working tree:

```bash
git checkout -- EthernetAppDemo/dist/
```

- [ ] **Step 5: Commit**

```bash
git add EthernetAppDemo/app_user.h EthernetAppDemo/app_user.c
git commit -m "$(cat <<'EOF'
refactor(f0.1): introduce App.scan_params for cross-scene targets

Adds scan_params_t struct (target_ip, target_port, range_port,
protocols_index, ip_ping, ip_start, range_ip) and embeds it as
App.scan_params with the same default values previously held in
file-static globals scattered across scenes.

No call-sites migrated yet — subsequent commits in F0.1 do that one
scene at a time.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Migrate `PortsScannerScene` (5 fields)

**Files:** `EthernetAppDemo/scenes/PortsScannerScene.c`

This scene owns 5 of the migrated globals: `target_ip`, `target_port`, `range_port`, `protocols_index`. (`target_port_bytes`/`range_port_bytes` stay; they're scheduled for F0.6.)

- [ ] **Step 1: Delete the file-static declarations**

Use Edit. Find:

```c
uint8_t target_ip[4] = {0};
uint16_t target_port = 22;
uint16_t range_port = 1000;
```

Replace with: (empty — delete the 3 lines and the surrounding comments if any)

```c
```

(Edit's `new_string` is the empty string in this case, but to keep the diff readable, replace with a single comment line:)

```c
// scan target/port/range now live in app->scan_params (F0.1)
```

Then find:

```c
uint8_t protocols_index = PORTS_SCANNER_TCP;
```

Replace with:

```c
// protocols_index now lives in app->scan_params (F0.1)
```

- [ ] **Step 2: Rewrite the call-sites**

Edit each occurrence in `PortsScannerScene.c`. The scene receives `App* app` via the scene callbacks, so all references become `app->scan_params.*`.

Approximate edits (verify line numbers — they shift as edits land):

| Was | Becomes |
|---|---|
| `target_port = value;` (line 49) | `app->scan_params.target_port = value;` |
| `range_port = value;` (line 51) | `app->scan_params.range_port = value;` |
| `bytes_to_uint(&range_port, range_port_bytes, ...);` (line 83) | `bytes_to_uint(&app->scan_params.range_port, range_port_bytes, ...);` |
| `bytes_to_uint(&target_port, target_port_bytes, ...);` (line 84) | `bytes_to_uint(&app->scan_params.target_port, target_port_bytes, ...);` |
| `switch(protocols_index)` (line 102) | `switch(app->scan_params.protocols_index)` |
| `tcp_syn_scan(app, target_ip, target_port, range_port);` (line 104) | `tcp_syn_scan(app, app->scan_params.target_ip, app->scan_params.target_port, app->scan_params.range_port);` |
| `udp_port_scan(app, target_ip, target_port, range_port);` (line 108) | `udp_port_scan(app, app->scan_params.target_ip, app->scan_params.target_port, app->scan_params.range_port);` |
| `int32_t current = (index == TARGET_PORT) ? target_port : range_port;` (line 188) | `int32_t current = (index == TARGET_PORT) ? app->scan_params.target_port : app->scan_params.range_port;` |
| `protocols_index = index;` (line 213) | `app->scan_params.protocols_index = index;` |
| `target_ip[0]` ... `target_ip[3]` (lines 233-236) | `app->scan_params.target_ip[0]` ... `app->scan_params.target_ip[3]` |
| `furi_string_cat_printf(app->text, "Target Port [%u]", target_port);` (line 247) | `..., app->scan_params.target_port);` |
| `furi_string_cat_printf(app->text, "Range [%u]", range_port);` (line 258) | `..., app->scan_params.range_port);` |
| `furi_string_cat_printf(app->text, "Protocol [%s]", protocols[protocols_index]);` (line 269) | `..., protocols[app->scan_params.protocols_index]);` |

Verify by:

```bash
grep -nE "\btarget_ip\b|\btarget_port\b|\brange_port\b|\bprotocols_index\b" EthernetAppDemo/scenes/PortsScannerScene.c
```

Every match must either be qualified with `app->scan_params.` or be a function-parameter name (e.g. inside a function signature `void foo(uint8_t* target_ip, ...)` — those are local parameters, not the global, and stay as-is).

The number_input setter callback (around `number_input_set_result_callback` if any) might pass `&app->scan_params.target_port` rather than `&target_port` — check the callback signature.

If any match remains unqualified and isn't a parameter, fix it before proceeding.

- [ ] **Step 3: Verify no other file references the deleted symbols**

```bash
grep -rn "extern uint8_t target_ip\|extern uint16_t target_port\|extern uint16_t range_port\|extern uint8_t protocols_index" EthernetAppDemo/
```

Expected: 0 matches. (The non-`static` `target_ip[4]` in the old code could in principle be `extern`-referenced from another file. The audit said it isn't, but verify.)

- [ ] **Step 4: Build green**

```bash
cd EthernetAppDemo && ufbt 2>&1 | tail -8 && cd .. && git checkout -- EthernetAppDemo/dist/
```

Expected: `FAP: ethernet_app.fap` produced. Common errors:
- `'target_ip' undeclared` → missed a call-site; grep for the bare name and qualify it.
- `'app' undeclared` → the call-site is in a callback that doesn't receive `App*`; pass it via the callback `void* context` cast.

- [ ] **Step 5: Commit**

```bash
git add EthernetAppDemo/scenes/PortsScannerScene.c
git commit -m "$(cat <<'EOF'
refactor(f0.1): migrate PortsScannerScene to app->scan_params

Replaces file-static target_ip[4], target_port, range_port, and
protocols_index with the corresponding fields under app->scan_params.

Eliminates the leaked-global target_ip[4] (was non-static at
PortsScannerScene.c:10).

target_port_bytes/range_port_bytes (PortsScannerScene.c:14-15) and
their byte_input_ports_scanner_callback path are NOT touched here —
they are dead code (no scene state switches to BYTE_INPUT) and
scheduled for removal in F0.6.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Migrate `OsDetector` (`target_ip`)

**Files:** `EthernetAppDemo/scenes/OsDetector.c`

- [ ] **Step 1: Delete the file-static declaration**

Use Edit. Find:

```c
static uint8_t target_ip[4] = {0};
```

Replace with:

```c
// target_ip now lives in app->scan_params (F0.1)
```

- [ ] **Step 2: Rewrite call-sites**

| Was | Becomes |
|---|---|
| `target_ip[0]` ... `target_ip[3]` (lines 83-86) | `app->scan_params.target_ip[0]` ... `app->scan_params.target_ip[3]` |
| `furi_string_cat_printf(app->text, "Target [%u.%u.%u.%u]", target_ip[0], ...);` (line 172) | `..., app->scan_params.target_ip[0], ...` |

Also check if `os_scan` (in `modules/os_detector_module.c`) is called from this scene with `target_ip` as an argument — if so, replace with `app->scan_params.target_ip`. Run:

```bash
grep -n "os_scan\|target_ip" EthernetAppDemo/scenes/OsDetector.c
```

Verify every `target_ip` reference is qualified.

- [ ] **Step 3: Build green**

```bash
cd EthernetAppDemo && ufbt 2>&1 | tail -8 && cd .. && git checkout -- EthernetAppDemo/dist/
```

- [ ] **Step 4: Commit**

```bash
git add EthernetAppDemo/scenes/OsDetector.c
git commit -m "$(cat <<'EOF'
refactor(f0.1): migrate OsDetector to app->scan_params

Replaces file-static target_ip[4] with app->scan_params.target_ip.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Migrate `ArpSpoofingSpecificIP` (`target_ip`)

**Files:** `EthernetAppDemo/scenes/ArpSpoofingSpecificIP.c`

- [ ] **Step 1: Delete the file-static declaration**

Use Edit. Find:

```c
static uint8_t target_ip[4] = {0};
```

Replace with:

```c
// target_ip now lives in app->scan_params (F0.1)
```

- [ ] **Step 2: Rewrite call-sites**

Currently (per audit) the references are at lines 59-62:

```c
target_ip[0],
target_ip[1],
target_ip[2],
target_ip[3]);
```

Replace each with `app->scan_params.target_ip[0..3]`. Verify with:

```bash
grep -nE "\btarget_ip\b" EthernetAppDemo/scenes/ArpSpoofingSpecificIP.c
```

Also check the worker thread function in this same file (audit mentioned it calls `arp_get_specific_mac` twice with `target_ip` as the IP-to-resolve argument). If those calls pass the file-static `target_ip`, switch them to `app->scan_params.target_ip`:

```bash
grep -nE "arp_get_specific_mac" EthernetAppDemo/scenes/ArpSpoofingSpecificIP.c
```

The thread context passes `app` already (see the thread body); the call probably looks like `arp_get_specific_mac(ethernet, target_ip, mac_dest)` — change `target_ip` to `app->scan_params.target_ip`.

- [ ] **Step 3: Build green**

```bash
cd EthernetAppDemo && ufbt 2>&1 | tail -8 && cd .. && git checkout -- EthernetAppDemo/dist/
```

- [ ] **Step 4: Commit**

```bash
git add EthernetAppDemo/scenes/ArpSpoofingSpecificIP.c
git commit -m "$(cat <<'EOF'
refactor(f0.1): migrate ArpSpoofingSpecificIP to app->scan_params

Replaces file-static target_ip[4] with app->scan_params.target_ip
across the scene callbacks and the worker thread that resolves the
target MAC via arp_get_specific_mac.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Migrate `PingScene` (`ip_ping`)

**Files:** `EthernetAppDemo/scenes/PingScene.c`

- [ ] **Step 1: Delete the file-static declaration**

Use Edit. Find:

```c
uint8_t ip_ping[4] = {0, 0, 0, 0};
```

Replace with:

```c
// ip_ping now lives in app->scan_params (F0.1)
```

- [ ] **Step 2: Rewrite call-sites**

References at lines 65, 74, 114 (per audit). Each line uses `ip_ping[0..3]` in a `furi_string_cat_printf`. Replace with `app->scan_params.ip_ping[0..3]`.

Also: any callback that mutates `ip_ping` (e.g. byte_input setter) — replace its target with `app->scan_params.ip_ping`. Find via:

```bash
grep -nE "\bip_ping\b" EthernetAppDemo/scenes/PingScene.c
```

- [ ] **Step 3: Build green**

```bash
cd EthernetAppDemo && ufbt 2>&1 | tail -8 && cd .. && git checkout -- EthernetAppDemo/dist/
```

- [ ] **Step 4: Commit**

```bash
git add EthernetAppDemo/scenes/PingScene.c
git commit -m "$(cat <<'EOF'
refactor(f0.1): migrate PingScene to app->scan_params

Replaces file-static ip_ping[4] with app->scan_params.ip_ping.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Migrate `ArpScannerScene` (`ip_start`, `range_ip`)

**Files:** `EthernetAppDemo/scenes/ArpScannerScene.c`

- [ ] **Step 1: Delete the file-static declarations**

Use Edit. Find:

```c
uint8_t ip_start[4] = {0, 0, 0, 0}; // The IP address to start the scan
uint8_t range_ip = 30; // The count of the IP addresses to scan
```

Replace with:

```c
// ip_start and range_ip now live in app->scan_params (F0.1)
```

- [ ] **Step 2: Rewrite call-sites**

References from the grep:
- `range_ip = value;` (line 67)
- `variable_item_set_current_value_index(item, range_ip);` (line 70)
- `furi_string_cat_printf(app->text, "%u", range_ip);` (line 74)
- `range_ip = (uint8_t)value;` (line 86)
- `number_input_set_result_callback(app->number_input, range_input_callback, app, range_ip, 1, 255);` (line 95)
- `furi_string_cat_printf(app->text, "Set IP [%u.%u.%u.%u]", ip_start[0], ip_start[1], ip_start[2], ip_start[3]);` (line 148)
- `furi_string_cat_printf(app->text, "Range [%u]", range_ip);` (line 155)
- `arp_scan_network(ethernet, app->ip_list, ip_start, &app->ip_counter, range_ip);` (line 462)

Each reference becomes `app->scan_params.ip_start[*]` or `app->scan_params.range_ip`. Verify:

```bash
grep -nE "\bip_start\b|\brange_ip\b" EthernetAppDemo/scenes/ArpScannerScene.c
```

Every match must be qualified with `app->scan_params.` or be a function-parameter name in a signature (none expected — these are scene-local symbols).

- [ ] **Step 3: Build green**

```bash
cd EthernetAppDemo && ufbt 2>&1 | tail -8 && cd .. && git checkout -- EthernetAppDemo/dist/
```

- [ ] **Step 4: Commit**

```bash
git add EthernetAppDemo/scenes/ArpScannerScene.c
git commit -m "$(cat <<'EOF'
refactor(f0.1): migrate ArpScannerScene to app->scan_params

Replaces file-static ip_start[4] and range_ip with the corresponding
fields under app->scan_params.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: Final verification + close F0.1 with tag

**Files:** none modified. Verification + tag.

- [ ] **Step 1: Final grep — zero remaining file-static target globals**

```bash
grep -rn "^static uint8_t target_ip\[\|^uint8_t target_ip\[\|^static uint16_t target_port\|^uint16_t target_port\|^static uint16_t range_port\|^uint16_t range_port\|^static uint8_t protocols_index\|^uint8_t protocols_index\|^uint8_t ip_ping\[\|^uint8_t ip_start\[\|^uint8_t range_ip" EthernetAppDemo/ 2>&1
```

Expected: zero matches.

If any match remains, fix it (was missed in Tasks 2-6) before proceeding.

- [ ] **Step 2: Bare-name grep — no unqualified references inside scenes**

```bash
grep -rnE "[^\.>](\btarget_ip\b|\btarget_port\b|\brange_port\b|\bprotocols_index\b|\bip_ping\b|\bip_start\b|\brange_ip\b)[^_a-zA-Z]" \
  EthernetAppDemo/scenes/ 2>&1 | grep -v "scan_params\." | grep -v "^Binary"
```

Each remaining hit must be examined: is it a parameter name (legal — local) or an actual reference to a deleted global (illegal — must be qualified)? Parameter names are typically inside function signatures, e.g. `void foo(uint8_t* target_ip, ...)`. References inside function BODIES that aren't qualified are bugs.

- [ ] **Step 3: Re-build green**

```bash
cd EthernetAppDemo && ufbt 2>&1 | tail -5 && cd .. && git checkout -- EthernetAppDemo/dist/
```

Expected: `FAP: ethernet_app.fap`, no warnings about unused/undeclared identifiers.

- [ ] **Step 4: Verify clean tree and commit count**

```bash
git status
git log --oneline v2.0-f0.0..HEAD
```

Expected: tree clean. Log shows 6 commits (one per Task 1-6).

- [ ] **Step 5: Annotated tag**

```bash
git tag -a v2.0-f0.1 -m "$(cat <<'EOF'
F0.1 — Centralize scan_params

Migrates 7 file-static target/scan globals into App.scan_params:
  - target_ip[4]   (was: 3 colliding declarations across scenes,
                    one of them non-static / global symbol leak)
  - target_port    (uint16_t)
  - range_port     (uint16_t)
  - protocols_index (uint8_t)
  - ip_ping[4]
  - ip_start[4]
  - range_ip       (uint8_t)

Per-scene migration:
  Task 1: introduce App.scan_params
  Task 2: PortsScannerScene
  Task 3: OsDetector
  Task 4: ArpSpoofingSpecificIP
  Task 5: PingScene
  Task 6: ArpScannerScene

target_port_bytes/range_port_bytes (PortsScannerScene.c:14-15) are
left as-is — they are dead code per the F0 audit and scheduled for
removal in F0.6 scaffolding cleanup.

Module signatures unchanged (tcp_syn_scan, udp_port_scan,
arp_scan_network, etc. still take target_ip/port as parameters; the
caller passes &app->scan_params.target_ip etc.).

Build verified green at every task. FAP size at F0.1 close:
EthernetAppDemo/dist/ethernet_app.fap = <RECORD AT TASK 7>
Target: 7, API: 87.8 (Unleashed unlshd-087)

Pending smoke test (manual):
  - PortsScanner: TCP scan a known host
  - OsDetector: scan a known host
  - ArpSpoofingSpecificIP: target a known IP
  - PingScene: ping
  - ArpScanner: range scan
  - All 5 should produce the same output as pre-F0.1.
EOF
)"
```

Replace `<RECORD AT TASK 7>` with the actual byte size from `ls -la EthernetAppDemo/dist/ethernet_app.fap` immediately after the build (before the dist/ checkout).

- [ ] **Step 6: Stop and report**

Do NOT push the branch or tag. Produce a short report:

```
F0.1 complete on refactor/phase-0.

7 commits since v2.0-f0.0:
  - F0.1 task 1: scan_params struct + init
  - F0.1 task 2: PortsScannerScene migration
  - F0.1 task 3: OsDetector migration
  - F0.1 task 4: ArpSpoofingSpecificIP migration
  - F0.1 task 5: PingScene migration
  - F0.1 task 6: ArpScannerScene migration

Tag: v2.0-f0.1 (local only, not pushed)
Build: green (FAP size <N> bytes; baseline F0.0 was 95544)
Smoke test on hardware: pending Sabas verification

Awaiting authorization to proceed to F0.2 (settings persistence).
```

---

## Self-review

**Spec coverage** (against `ENC28J60_REFACTOR_PLAN.md` §6 F0.1 + roadmap §3 F0.1):

- 3 colliding `target_ip[4]` declarations eliminated → Tasks 2 (PortsScanner), 3 (OsDetector), 4 (ArpSpoofingSpecificIP).
- Other file-static targets (`target_port`, `range_port`, `protocols_index`, `ip_ping`, `ip_start`, `range_ip`, `hostname`) → Tasks 2, 5, 6. ⚠️ Note: the master plan §6 F0.1 mentions `hostname[64]` as a candidate field. The current code does NOT have `hostname` as a file-static — it's only used inline inside `flipper_process_dora_with_host_name`. **Decision:** include `char hostname[64]` field in `scan_params_t` for F0.2 use (settings persistence), but do not migrate any call-sites in F0.1 because there are none. This is a small forward-looking addition, not scope creep — F0.2 needs the field.

  **WAIT — re-evaluate.** Adding an unused field adds heap and confuses the diff. Better: add `hostname` only when F0.2 needs it. **Final decision: do NOT include `hostname` in F0.1.** The plan above already omits it.

- Per-scene incremental commits → Tasks 2-6 each produce one commit.
- Final grep verification → Task 7 step 1.
- Module APIs unchanged → confirmed in plan structure.
- Tag `v2.0-f0.1` → Task 7 step 5.

**Placeholder scan:** `<RECORD AT TASK 7>` is a deliberate placeholder filled at runtime from concrete `ls -la` output. `<N>` in the closing report is the same value. All other content is concrete.

**Type/identifier consistency:** `scan_params_t`, `target_ip`, `target_port`, etc. are used consistently across all 7 tasks. The struct lives in `app_user.h`, the member is `App.scan_params` (singular), all call-sites use `app->scan_params.<field>`.

---
