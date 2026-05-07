---
name: enc28j60-fase-actual
description: Active context for the ENC28J60 refactor. Consult before any commit, file creation, or direction change. Updated at the start of every sub-phase.
---

# Current phase: F0.4 — RX dispatch decoupling (entering)

## What is done

- v2.0-f0.0  Bootstrap. License audit (driver is GPL-2.0-or-later,
             carve-out applied). INT pin on Flipper pin 10 / PA14 /
             SWCLK. CI uses Unleashed unlshd-087 (API 87.8). Issue
             templates added. Hardware-validated.
- v2.0-f0.1  scan_params centralized in App. 7 file-static globals
             collapsed across 6 scenes. Hardware-validated.
- v2.0-f0.2  Settings persistence via libraries/settings/settings.{c,h}
             on /ext/apps_data/ethernet/settings.cfg. MAC/IP/static-flag/
             scan_params survive reboots. Hardware-validated.
- v2.0-f0.3  Scanner session refactor. All 6 scanners share
             libraries/scanner/scanner_session.{c,h} (init/deinit/
             resolve_next_hop/wait_for_packet/cancel_requested).
             Sub-phases v2.0-f0.3a..f. Hardware-validated against
             dnsmasq + http.server lab on 10.10.10.0/24.
- ux fix     Main menu reordered to follow audit flow (Get IP first,
             then Scan Hosts, etc.). DORA Process → "Get IP".
             IP Scanner → "Scan Hosts".

## Where we are

Entering F0.4 — RX dispatch decoupling. This is the architectural fix
that makes most of the deferred bugs in docs/BACKLOG.md disappear at
once.

Sub-phase plan (proposed, awaiting Sabas confirm):
  F0.4a — libraries/chip/rx_dispatch.{c,h} (single thread + handler
          registry). Migrate auto-ARP-reply and auto-ICMP-reply
          handlers. Delete app_worker.c.
  F0.4b — Rewrite scanner_session.wait_for_packet on top of
          rx_dispatch (register/wait_semaphore/unregister).
  F0.4c — Remove all furi_thread_suspend/resume from scenes/.
  F0.4d — SnifferScene re-implemented as a capture-everything handler.
          Bug B-2 (busy-loop block) and B-3 (dead window) close as a
          byproduct.
  F0.4 final — verification + tag.

## What NOT to touch

- The two driver files (`enc28j60.c`, `enc28j60.h`) keep their SPDX
  GPL-2.0-or-later header. New code added to them inherits the
  license. Don't rename the ETHERCARD_* macros — they document the
  derivation boundary.
- `application.fam` — already at F0 baseline. No SDK pin needed
  (Unleashed doesn't support `fap_min_sdk_version`).
- ARP-spoof-to-IP feature — gated by current `DEV_MODE`, leave as-is
  until F0.8 introduces `PENTEST_MODE` flag.
- ofp_tseq dev scaffolding (`os_detector_module.c:517-585`). F0.6
  scaffolding cleanup will delete it.
- TestingScene. F0.6 will delete.
- The pre-existing `dnsmasq --no-ping` workaround documented in
  /tmp/eth-testrig-up.sh — that's a *test rig* concession, not a
  Flipper fix. Real fix is bumping the DORA timeout (B-1, F0.7).

## Extra rules during F0

- All commit messages in English.
- Each sub-phase ends with one annotated tag `vMAJOR.MINOR-fN.M`.
- Hardware-validate before tagging F0.X final whenever Flipper is
  available.
- After every `ufbt` build, run `git checkout -- EthernetAppDemo/dist/`
  to discard tracked-but-rebuilt artifacts before commit, unless the
  task explicitly requires committing the new `.fap` (only at release
  tags).
- Do not delete files claiming they are "dead" without a fresh
  `grep -rn` confirming zero callers.

## Backlog of deferred bugs

See `docs/BACKLOG.md`. Five entries from F0.3 hardware testing:
  B-1 DORA timeout too short → F0.7
  B-2 SnifferScene busy-loop blocks BACK → F0.4
  B-3 dead window between sniffer entry and capture → F0.4
  B-4 UDP scanner conflates open/filtered → F1
  B-5 LoadingView animation lag during ARP scan → F0.4

## Quick references

- Master plan: `ENC28J60_REFACTOR_PLAN.md` (root).
- Roadmap design: `docs/superpowers/specs/2026-05-05-roadmap-design.md`.
- F0.x plans: `docs/superpowers/plans/`.
- Decisions: `docs/DECISIONS.md`.
- Backlog: `docs/BACKLOG.md`.
- Architecture: `docs/ARCHITECTURE.md`.
- Hardware: `docs/HARDWARE.md`.
- Test rig (laptop side): `/tmp/eth-testrig-up.sh`,
  `/tmp/eth-testrig-down.sh`. Pcap goes to `/tmp/eth-testrig.pcap`.
