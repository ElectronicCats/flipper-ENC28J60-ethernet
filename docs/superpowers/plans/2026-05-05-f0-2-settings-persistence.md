# F0.2 — Settings Persistence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Persist user-mutable state (MAC, IP, static-vs-DHCP flag, all `scan_params` fields) across app launches via a single `flipper_format` file at `/ext/apps_data/ethernet/settings.cfg`.

**Architecture:** New module `libraries/settings/settings.{c,h}` exposes `settings_load(App*)` and `settings_save(App*)`. Load is called once at boot after the App is fully constructed and storage is open; save is called once at app teardown before storage is closed. Missing-file and parse-error paths silently fall back to in-memory defaults — never block boot, never throw a dialog.

**Tech Stack:** C, Flipper FAP (uFBT), `flipper_format` library (already verified in F0.0 D3 — `flipper_format_buffered_file_alloc`, `flipper_format_read_hex`, `flipper_format_read_uint32`, `flipper_format_read_bool`).

---

## File Structure

Files created in F0.2:
- `EthernetAppDemo/libraries/settings/settings.h` — `settings_load(App*)`, `settings_save(App*)` declarations.
- `EthernetAppDemo/libraries/settings/settings.c` — implementation using `flipper_format`.

Files modified in F0.2:
- `EthernetAppDemo/app_user.h` — `#include "libraries/settings/settings.h"` near the other library includes.
- `EthernetAppDemo/app_user.c` — call `settings_load(app)` near the end of `app_alloc()` (after the chip is allocated, after `make_paths`); call `settings_save(app)` at the beginning of `app_free()` (before any free / record_close).

No other files touched. No tests added (no test framework on FAP). Verification is build-green + manual smoke (change MAC in SettingsScene → exit app → reopen → MAC persists).

uFBT auto-discovers `.c` files in subdirectories of the app source root, so creating `libraries/settings/*.c` adds it to the build automatically (same pattern as `libraries/chip/`, `libraries/protocol_tools/`).

---

## File Format

```
Filetype: Flipper ENC28J60 Ethernet App Settings
Version: 1

# 6 bytes — MAC address of the ENC28J60 chip
mac_address: BA 3F 91 C2 7E 5D

# 4 bytes — static IPv4 (only meaningful when is_static_ip=1)
ip_address: C0 A8 00 02

# 0 = DHCP, 1 = static IP
is_static_ip: 0

# scan_params (F0.1)
scan_target_ip: 00 00 00 00
scan_target_port: 22
scan_range_port: 1000
scan_protocols_index: 0
scan_ip_ping: 00 00 00 00
scan_ip_start: 00 00 00 00
scan_range_ip: 30
```

This is the format `flipper_format` natively produces; the implementer does not write the textual representation by hand. Use the helper API (`flipper_format_write_header_cstr`, `flipper_format_write_hex`, `flipper_format_write_uint32`, `flipper_format_write_bool`).

---

## Task 1: Implement `libraries/settings/settings.{c,h}`

**Files:**
- Create: `EthernetAppDemo/libraries/settings/settings.h`
- Create: `EthernetAppDemo/libraries/settings/settings.c`

The header must NOT directly include `app_user.h` (would create an include cycle since `app_user.h` will also include `settings.h`). Use a forward-declaration of `App`.

- [ ] **Step 1: Write the header**

```bash
mkdir -p EthernetAppDemo/libraries/settings
```

Create `EthernetAppDemo/libraries/settings/settings.h`:

```c
#pragma once

// Forward declaration — full definition is in app_user.h.
// Including app_user.h here would create a cycle.
typedef struct App App;

/**
 * Load persisted settings from /ext/apps_data/ethernet/settings.cfg into the
 * App. If the file is missing, malformed, or has a version mismatch, leaves
 * the App's in-memory defaults intact and returns silently. Never shows a
 * dialog or blocks the caller.
 *
 * Must be called only after the App has been fully constructed (storage
 * record open, ethernet allocated) and after make_paths() so the
 * containing directory exists.
 */
void settings_load(App* app);

/**
 * Persist the current App settings to /ext/apps_data/ethernet/settings.cfg.
 * Errors are silent — a failed write does not propagate.
 *
 * Must be called before app_free() begins tearing down storage records or
 * the ethernet instance.
 */
void settings_save(App* app);
```

- [ ] **Step 2: Write the implementation**

Create `EthernetAppDemo/libraries/settings/settings.c`:

```c
#include "settings.h"
#include "../../app_user.h"
#include <flipper_format/flipper_format.h>

#define SETTINGS_FILETYPE "Flipper ENC28J60 Ethernet App Settings"
#define SETTINGS_VERSION 1
#define SETTINGS_PATH    PATHAPPEXT "/settings.cfg"

void settings_load(App* app) {
    furi_assert(app);
    furi_assert(app->storage);
    furi_assert(app->ethernet);

    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    FuriString* file_type = furi_string_alloc();
    uint32_t version = 0;
    uint32_t u32_tmp = 0;
    bool flag = false;
    uint8_t buf6[6];
    uint8_t buf4[4];

    do {
        if(!flipper_format_file_open_existing(ff, SETTINGS_PATH)) break;
        if(!flipper_format_read_header(ff, file_type, &version)) break;
        if(version != SETTINGS_VERSION) break;
        if(furi_string_cmp_str(file_type, SETTINGS_FILETYPE) != 0) break;

        // MAC — also push to chip registers if changed.
        if(flipper_format_read_hex(ff, "mac_address", buf6, 6)) {
            memcpy(app->ethernet->mac_address, buf6, 6);
            enc28j60_set_mac(app->ethernet, buf6);
        }

        // Static IPv4 (used only when is_static_ip == true).
        if(flipper_format_read_hex(ff, "ip_address", buf4, 4)) {
            memcpy(app->ethernet->ip_address, buf4, 4);
            memcpy(app->ip_helper, buf4, 4);
        }

        // is_static_ip flag.
        if(flipper_format_read_bool(ff, "is_static_ip", &flag, 1)) {
            app->is_static_ip = flag;
        }

        // scan_params block (F0.1).
        flipper_format_read_hex(ff, "scan_target_ip", app->scan_params.target_ip, 4);

        if(flipper_format_read_uint32(ff, "scan_target_port", &u32_tmp, 1)) {
            app->scan_params.target_port = (uint16_t)u32_tmp;
        }
        if(flipper_format_read_uint32(ff, "scan_range_port", &u32_tmp, 1)) {
            app->scan_params.range_port = (uint16_t)u32_tmp;
        }
        if(flipper_format_read_uint32(ff, "scan_protocols_index", &u32_tmp, 1)) {
            app->scan_params.protocols_index = (uint8_t)u32_tmp;
        }

        flipper_format_read_hex(ff, "scan_ip_ping", app->scan_params.ip_ping, 4);
        flipper_format_read_hex(ff, "scan_ip_start", app->scan_params.ip_start, 4);

        if(flipper_format_read_uint32(ff, "scan_range_ip", &u32_tmp, 1)) {
            app->scan_params.range_ip = (uint8_t)u32_tmp;
        }
    } while(false);

    furi_string_free(file_type);
    flipper_format_free(ff);
}

void settings_save(App* app) {
    furi_assert(app);
    furi_assert(app->storage);
    furi_assert(app->ethernet);

    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    uint32_t u32_tmp = 0;

    do {
        if(!flipper_format_file_open_always(ff, SETTINGS_PATH)) break;
        if(!flipper_format_write_header_cstr(ff, SETTINGS_FILETYPE, SETTINGS_VERSION)) break;

        if(!flipper_format_write_hex(ff, "mac_address", app->ethernet->mac_address, 6)) break;
        if(!flipper_format_write_hex(ff, "ip_address", app->ethernet->ip_address, 4)) break;
        if(!flipper_format_write_bool(ff, "is_static_ip", &app->is_static_ip, 1)) break;

        if(!flipper_format_write_hex(ff, "scan_target_ip", app->scan_params.target_ip, 4)) break;

        u32_tmp = app->scan_params.target_port;
        if(!flipper_format_write_uint32(ff, "scan_target_port", &u32_tmp, 1)) break;
        u32_tmp = app->scan_params.range_port;
        if(!flipper_format_write_uint32(ff, "scan_range_port", &u32_tmp, 1)) break;
        u32_tmp = app->scan_params.protocols_index;
        if(!flipper_format_write_uint32(ff, "scan_protocols_index", &u32_tmp, 1)) break;

        if(!flipper_format_write_hex(ff, "scan_ip_ping", app->scan_params.ip_ping, 4)) break;
        if(!flipper_format_write_hex(ff, "scan_ip_start", app->scan_params.ip_start, 4)) break;

        u32_tmp = app->scan_params.range_ip;
        if(!flipper_format_write_uint32(ff, "scan_range_ip", &u32_tmp, 1)) break;
    } while(false);

    flipper_format_free(ff);
}
```

Notes for the implementer:

- `app_user.h` already defines `PATHAPPEXT` and includes `furi.h`, `enc28j60.h` (which has `enc28j60_set_mac`).
- `enc28j60_t.mac_address[6]` and `.ip_address[4]` are confirmed at `enc28j60.h:39-40`.
- `enc28j60_set_mac` signature is at `enc28j60.h:84+` — it takes `(enc28j60_t*, uint8_t*)`. The driver also writes the MAC to `MAADR0..5` registers, so calling it on load is the correct way to actually program the chip.
- `flipper_format_file_open_always` truncates and creates the file (right for save).
- `flipper_format_file_open_existing` opens read-only; missing-file returns false (right for load).
- The `do { ... } while(false);` pattern lets us `break` out cleanly on any error and still run the cleanup at the bottom — same pattern used by Flipper's own examples in the `flipper_format.h` header doc.
- `furi_assert` on `app`/`storage`/`ethernet` is intentional: these three are invariants of the call contract; a NULL means the caller violated the lifecycle order.
- Reads use `flipper_format_read_*` which already returns false on missing key — the function silently keeps the default. This is by design.

- [ ] **Step 3: Verify the file location is correct**

```bash
ls -la EthernetAppDemo/libraries/settings/settings.h EthernetAppDemo/libraries/settings/settings.c
```

Expected: both files listed.

- [ ] **Step 4: Build green**

```bash
cd EthernetAppDemo && ufbt 2>&1 | tail -10 && cd .. && git checkout -- EthernetAppDemo/dist/
```

Expected:
- `FAP /home/sabas/.ufbt/build/ethernet_app.fap` line.
- `Target: 7, API: 87.8`.
- No errors.

uFBT should auto-discover `libraries/settings/settings.c`. If it doesn't (older ufbt versions), check `application.fam` for `sources` field — if absent, ufbt globs `*.c` recursively from the app dir, which is what we expect.

If you see `error: 'App' has incomplete type`, it means the `#include "../../app_user.h"` failed to resolve. Verify the relative path — from `libraries/settings/settings.c` to `app_user.h` is `../../app_user.h`.

If you see `undefined reference to 'flipper_format_*'` at link time, the SDK link group is missing — investigate but this should not happen on a stock Unleashed SDK.

- [ ] **Step 5: Commit**

```bash
git status   # only the two new files
git add EthernetAppDemo/libraries/settings/
git commit -m "$(cat <<'COMMIT_EOF'
feat(f0.2): add settings load/save module

New libraries/settings/settings.{c,h} with settings_load(App*) and
settings_save(App*) using flipper_format. File path is
PATHAPPEXT/settings.cfg ("/ext/apps_data/ethernet/settings.cfg").

Persists: MAC, static IPv4, is_static_ip, and the full scan_params
block (target_ip, target_port, range_port, protocols_index, ip_ping,
ip_start, range_ip).

Missing-file and parse-error paths silently fall back to defaults —
no dialogs, never block boot.

This commit only adds the module; lifecycle wiring is in the next
commit.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
COMMIT_EOF
)"
```

---

## Task 2: Wire `settings_load` / `settings_save` into the app lifecycle

**Files:**
- Modify: `EthernetAppDemo/app_user.h` (one `#include` line).
- Modify: `EthernetAppDemo/app_user.c` (one call in `app_alloc`, one call in `app_free`).

- [ ] **Step 1: Add include to `app_user.h`**

Use Edit. Find the block of module includes (around `app_user.h:24-32`):

```c
#include "libraries/chip/enc28j60.h"
#include "modules/arp_module.h"
#include "modules/dhcp_protocol.h"
#include "modules/tcp_module.h"
#include "modules/udp_module.h"
#include "modules/capture_module.h"
#include "modules/analysis_module.h"
#include "modules/ping_module.h"
#include "modules/os_detector_module.h"
```

Replace with the same block plus the new include (insert AFTER `enc28j60.h` and BEFORE the `modules/` includes — alphabetical-ish ordering by directory):

```c
#include "libraries/chip/enc28j60.h"
#include "libraries/settings/settings.h"
#include "modules/arp_module.h"
#include "modules/dhcp_protocol.h"
#include "modules/tcp_module.h"
#include "modules/udp_module.h"
#include "modules/capture_module.h"
#include "modules/analysis_module.h"
#include "modules/ping_module.h"
#include "modules/os_detector_module.h"
```

- [ ] **Step 2: Call `settings_load` at end of `app_alloc`**

Use Edit on `EthernetAppDemo/app_user.c`. Find:

```c
    memcpy(app->ip_helper, IP_DEFAULT, 4);

    app->is_static_ip = false;
    app->is_dora = false;

    return app;
}
```

Replace with:

```c
    memcpy(app->ip_helper, IP_DEFAULT, 4);

    app->is_static_ip = false;
    app->is_dora = false;

    // F0.2 — overlay persisted settings on top of the in-memory defaults.
    // Silent fallback to defaults if /ext/apps_data/ethernet/settings.cfg is
    // missing or malformed.
    settings_load(app);

    return app;
}
```

The placement is intentional: `settings_load` runs AFTER:
- `app->scan_params` defaults are set (lines 48-54).
- `app->ethernet` is allocated and the chip's MAC has been programmed via `enc28j60_alloc`.
- `make_paths` has created the directory.
- `app->is_static_ip = false` default is set.

This ordering means the persisted values cleanly overlay the defaults; if any field is missing from the file, the default already in place stays.

- [ ] **Step 3: Call `settings_save` at start of `app_free`**

Use Edit. Find:

```c
void app_free(App* app) {
    furi_thread_flags_set(furi_thread_get_id(app->thread), flag_stop);
```

Replace with:

```c
void app_free(App* app) {
    // F0.2 — persist current settings before tearing down storage and the
    // ethernet instance. Errors are silent; a failed save must not block
    // app exit.
    settings_save(app);

    furi_thread_flags_set(furi_thread_get_id(app->thread), flag_stop);
```

The placement is intentional: `settings_save` runs BEFORE:
- The ethernet thread stops.
- The ethernet instance is freed (line `free_enc28j60(app->ethernet)`).
- The storage record is closed (line `furi_record_close(RECORD_STORAGE)`).

So `app->storage` and `app->ethernet->mac_address` are both still valid when save reads them.

- [ ] **Step 4: Build green**

```bash
cd EthernetAppDemo && ufbt 2>&1 | tail -10 && cd .. && git checkout -- EthernetAppDemo/dist/
```

Expected:
- `FAP` line.
- `Target: 7, API: 87.8`.
- No new warnings.

- [ ] **Step 5: Commit**

```bash
git status   # app_user.h and app_user.c modified
git add EthernetAppDemo/app_user.h EthernetAppDemo/app_user.c
git commit -m "$(cat <<'COMMIT_EOF'
feat(f0.2): wire settings_load/save into app lifecycle

Calls settings_load(app) at the end of app_alloc(), after defaults are
in place and after the ethernet instance + storage record are open.
Calls settings_save(app) at the start of app_free(), before the
ethernet/storage teardown begins.

Effect: MAC, static IP, is_static_ip flag, and the entire scan_params
block now survive across app launches via
/ext/apps_data/ethernet/settings.cfg.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
COMMIT_EOF
)"
```

---

## Task 3: Final verification + close F0.2 with tag

**Files:** none modified. Verification + tag.

- [ ] **Step 1: Re-run final build**

```bash
cd EthernetAppDemo && ufbt 2>&1 | tail -5 && cd .. && git checkout -- EthernetAppDemo/dist/
```

Expected: green, FAP produced.

- [ ] **Step 2: Capture FAP byte size**

```bash
cd EthernetAppDemo && ufbt 2>&1 > /dev/null && ls -la dist/ethernet_app.fap && cd ..
git checkout -- EthernetAppDemo/dist/
```

Note the byte size (will be larger than F0.1's 94792 — settings.c adds code).

- [ ] **Step 3: Verify deliverables**

```bash
ls -la \
  EthernetAppDemo/libraries/settings/settings.h \
  EthernetAppDemo/libraries/settings/settings.c
grep -nE "settings_load|settings_save" EthernetAppDemo/app_user.c
grep -nE "libraries/settings/settings.h" EthernetAppDemo/app_user.h
```

Expected:
- Both .c/.h files exist.
- 1 call to `settings_load` and 1 call to `settings_save` in `app_user.c`.
- 1 include in `app_user.h`.

- [ ] **Step 4: Verify clean tree and commit count**

```bash
git status
git log --oneline v2.0-f0.1..HEAD
```

Expected:
- Tree clean.
- 2 commits since `v2.0-f0.1` (Task 1 + Task 2).

- [ ] **Step 5: Annotated tag**

Replace `<FAP_BYTES>` below with the value captured in Step 2.

```bash
git tag -a v2.0-f0.2 -m "$(cat <<'EOF'
F0.2 — Settings persistence

Adds libraries/settings/settings.{c,h} and wires it into the app
lifecycle. settings_load(app) runs at the end of app_alloc() to
overlay /ext/apps_data/ethernet/settings.cfg on top of in-memory
defaults; settings_save(app) runs at the start of app_free() to
persist the current state.

Persisted fields:
  - ethernet->mac_address (6 bytes, hex)
  - ethernet->ip_address  (4 bytes, hex; only meaningful when
                           is_static_ip=1)
  - is_static_ip          (bool)
  - scan_params.target_ip       (4 bytes hex)
  - scan_params.target_port     (uint32)
  - scan_params.range_port      (uint32)
  - scan_params.protocols_index (uint32)
  - scan_params.ip_ping         (4 bytes hex)
  - scan_params.ip_start        (4 bytes hex)
  - scan_params.range_ip        (uint32)

File format: flipper_format with header
  Filetype: "Flipper ENC28J60 Ethernet App Settings"
  Version:  1

Missing-file / malformed-file paths silently keep in-memory defaults.
No dialogs, never blocks boot.

Build verified green. FAP size at F0.2 close:
  EthernetAppDemo/dist/ethernet_app.fap = <FAP_BYTES> bytes
  (was 94792 at F0.1 close; net increase from settings.c +
   flipper_format link)
  Target: 7, API: 87.8 (Unleashed unlshd-087)

Pending smoke test (manual on hardware):
  1. Launch app. SettingsScene → change MAC to BB:CC:DD:EE:FF:00.
  2. Back out to MainMenu → back out to Flipper desktop (full app
     exit).
  3. Relaunch app. SettingsScene → MAC must read BB:CC:DD:EE:FF:00.
  4. (Optional) ls /ext/apps_data/ethernet/settings.cfg via qFlipper
     and inspect contents — should match the documented format.
EOF
)"
```

- [ ] **Step 6: Verify tag and report**

```bash
git tag --list | grep v2.0
git show v2.0-f0.2 --stat | head -15
```

Expected: `v2.0-f0.0`, `v2.0-f0.1`, `v2.0-f0.2` listed; the show output displays the tag message and the 2 commits.

Stop. Do NOT push. Produce a closing report:

```
F0.2 complete on refactor/phase-0.

2 commits since v2.0-f0.1:
  - Task 1: libraries/settings/settings.{c,h} (load + save impl)
  - Task 2: wire settings_load/save into app_alloc/app_free

Tag: v2.0-f0.2 (local only, not pushed).
Build: green. FAP size: <FAP_BYTES> bytes.
Smoke test on hardware: pending Sabas verification (steps in tag
message).

Awaiting authorization to proceed to F0.3 (generic scanner_session).
```

---

## Self-review

**Spec coverage** (against `ENC28J60_REFACTOR_PLAN.md` §6 F0.2 + roadmap §3 F0.2):

- "Implement `settings.cfg` via `flipper_format`" → Task 1.
- "`settings_load(app)` in boot" → Task 2 wires it into `app_alloc`.
- "`settings_save(app)` on each change" → **Note discrepancy.** The master plan / roadmap say "on each change", but this plan implements save-on-exit only. Rationale: simpler model, single save point catches everything that matters (the user only cares that values stick across launches; a Flipper crash is rare enough that we don't pay for save-after-every-byte-input). If save-on-change becomes important later (e.g. crash recovery, sync to companion app), it's a one-line change in each scene's `on_exit`. **Decision recorded here for transparency; no plan change needed in F0.2.**
- "Persist MAC, last gateway IP, last DORA hostname, last sniffer prefix" → MAC and ip_address (used as static IP) covered. **DORA hostname** is not yet persisted because the current code does not have a user-facing hostname field (it's hardcoded inside `flipper_process_dora_with_host_name`). Adding it is a separate F-task; recorded as DEFERRED.
- "Sniffer PCAP prefix" → not persisted; the PCAP filename is `pcap_DD_MM_YYYY_N.pcap` and there is no user-facing prefix knob today. DEFERRED.
- Criterion "change MAC in Settings, exit, reopen, MAC persists" → the save-on-exit model satisfies this directly.

**Placeholder scan:** `<FAP_BYTES>` is a runtime-filled value with explicit instruction (Step 2 captures it; Step 5 replaces it). All other content is concrete.

**Type/identifier consistency:** `settings_load(App*)`, `settings_save(App*)`, `app->scan_params.target_ip`, `app->ethernet->mac_address`, `app->ethernet->ip_address`, `app->is_static_ip` — used consistently across header, implementation, and lifecycle wiring. Field names match what's actually in the structs (`enc28j60_t.mac_address[6]` at `enc28j60.h:39`, `enc28j60_t.ip_address[4]` at `enc28j60.h:40`, `App.is_static_ip` at `app_user.h:79`, `scan_params_t` fields at `app_user.h:69-80`).

---
