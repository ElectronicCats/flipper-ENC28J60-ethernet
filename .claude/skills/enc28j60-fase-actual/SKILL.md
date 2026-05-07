---
name: enc28j60-fase-actual
description: Active context for the ENC28J60 app refactor. Consult before any commit, file creation, or direction change. Phase 0 closed and merged to main on 2026-05-07.
---

# Current phase: F0 closed (v2.0). F1 not yet started.

## Where we are now

- Phase 0 refactor merged to `main` via PR #7 on 2026-05-07
  (merge commit `e972464`). Local + remote `main` carry v2.0.
- 27 phase tags created locally (`v2.0-f0.0` .. `v2.0-f0.5h`) plus
  the release tag `v2.0` on the merge commit. **Push status of
  these tags fluctuates — verify with `git ls-remote --tags origin
  'v2.0*'` if it matters; only `v2.0-f0.5h` was confirmed on origin
  at close time.**
- Pre-commit hooks active in this repo (`.pre-commit-config.yaml`
  has trailing-whitespace, end-of-file-fixer, no-commit-to-branch,
  clang-format, conventional-pre-commit). `pre-commit install` was
  run; subsequent commits are validated automatically.
- Build artifacts are no longer tracked: `dist/`, `*.fap`, `*.elf`,
  `compile_commands.json`, etc. are in `.gitignore` since F0.5f.
  Distribution path: GitHub Release on tag `v2.0` (manual upload of
  the `.fap`) until F0.8 lands a CI workflow.

## What is done (high-level — see commit log + tags for details)

| Phase | Tag(s) | Summary |
|---|---|---|
| F0.0 | v2.0-f0.0 | Bootstrap, license carve-out, INT pin researched, SDK pinned, issue templates, project skill. |
| F0.1 | v2.0-f0.1 | `App.scan_params` centralizes 7 file-static `target_ip`s across 6 scenes. |
| F0.2 | v2.0-f0.2 | `libraries/settings/{c,h}` persists MAC/IP/`is_static_ip`/scan_params via `flipper_format`. v2 schema (F0.5f) adds `is_dora`, gateway IP, gateway MAC. |
| F0.3a-e | v2.0-f0.3a..e | `libraries/scanner/scanner_session.{c,h}` primitive. ping_thread, arp_scan, port scan, OS probe, ICMP probe migrated. |
| F0.4a-g | v2.0-f0.4..f0.4g.1 | RX dispatcher decoupling. INT-driven (F0.5c). Auto-ARP / auto-ICMP handlers. `app_worker.c` deleted (F0.4e). DORA in alt thread. `arp_get_specific_mac` migrated for `scanner_resolve_next_hop` (F0.4f). PingScene migrated off direct `receive_packet` (F0.4g.1). |
| F0.5a-h | v2.0-f0.5a/c/d/d-wave2/e/f/g/h | Chip-level mutex (B-6 closed). INT pin enabled (D2). Race fixes: H2 send→register, H3 handler-out-of-mutex UAF, H4 App malloc, H5 mac_helper sized [6], M7 pause kicks thread. PCAP overflow guards. Settings v2 + DORA cancellable. UDP redundant-ARP removed. OS Detector quick wins + EXPERIMENTAL label. **F0.5b skipped intentionally** — chip protocol forces a CS toggle per command, no real win from a "bulk SPI" rewrite. |
| F0.6 | (rolled into F0.5* commits) | Dev scaffolding cleanup: TestingScene, ofp_tseq, dead `flipper_process_dora`, prod printf hex-dumps, etc. |
| F0.7 | (rolled into F0.5* commits) | Eight audit bugs: PCAP timestamps, is_duplicated_ip underflow, tcp_send_xmas_probe, MAC/subnet length mismatch, TCP fall-through, pcap_scan overflow, pre-DORA auto-reply IP conflict, MainMenu logo blocking. |

`docs/ARCHITECTURE.md` (rewritten as v2.0 reference, F0.5h closure)
has the structured layer/threading/RX-flow view. `docs/BACKLOG.md`
tracks closed and deferred items.

## What's deferred to F1 (in BACKLOG.md)

- **B-9** OS Detector reliability rewrite (RX vs dispatcher race,
  sample-slot collision, premature PORT_OPEN, blocking UI). v2.0
  ships labelled EXPERIMENTAL.
- **F0.4g.2** os_detector burst-probe RX onto rx_dispatch handler.
- **B-7 follow-up** delete dead `tcp_handshake_process` / `_spoof`,
  migrate `arp_get_specific_mac` callers in `ArpSpoofingSpecificIP`.
- **F0.8** dual-build CI (admin / pentest .fap variants).
- **PCAP per-batch flush.**
- **Protocol expansion** (LLDP, mDNS, IPv6, responder family).
- **F0.0 Task 9** (manual UI) — enable GitHub Discussions in
  Settings → Features.

## What NOT to touch

- Driver files (`libraries/chip/enc28j60.{c,h}`) keep the
  GPL-2.0-or-later SPDX header. New code added to them inherits the
  license. Don't rename `ETHERCARD_*` macros — they document the
  derivation boundary.
- `enc28j60_t.mutex` (F0.5a) — every public chip function takes it.
  Static helpers (`write_register`, `set_bank_with_mask`, etc.) do
  NOT take it; they inherit from the caller.
- The remaining `rx_dispatch_pause/resume` callsite in
  `GetIPScene.c` (around DORA) — the DHCP state machine consumes
  raw packets in order; the chip mutex isn't enough. Don't remove
  this until DORA itself migrates onto an rx_dispatch handler (F1).
- `scanner_wait_for_packet`'s contract: predicate captures state
  via `pred_ctx`; rx_buffer is NOT preserved after the wait returns
  (F0.5g doc fix). Don't write callers that re-read the frame.
- Handlers run inside the dispatch mutex (F0.5d). Keep them fast,
  no register/unregister inside, no slow I/O. SnifferScene's
  ~30 ms SD write is the slowest one tolerated; document any new
  slow handler.
- `application.fam` — no SDK pin (Unleashed unlshd-087 doesn't
  support `fap_min_sdk_version`).
- ARP-spoof-to-IP feature — gated by `DEV_MODE`. F0.8 will
  introduce `PENTEST_MODE` and re-gate.

## Repo conventions

- All commit messages in English. Conventional Commits required by
  pre-commit hook. **Do not put dots in scope** — `fix(f0.5f):` is
  rejected by the parser; use `fix: F0.5f — ...` instead.
- Annotated tags `v<MAJOR>.<MINOR>-f<N>.<M>` for each sub-phase that
  warrants a checkpoint.
- Hardware-validate on a Flipper before tagging a phase final
  whenever the device is available. Test rig at
  `/tmp/eth-testrig-up.sh` / `eth-testrig-down.sh` brings up
  10.10.10.0/24 + dnsmasq + http.server + tcpdump on the laptop's
  USB ethernet adapter (default `enx000ec6be9018`).
- Don't claim files are "dead" without a fresh `grep -rn` over
  `EthernetAppDemo` confirming zero callers across the project,
  including header-only references.
- After Edit batches, **always grep / git diff before commit** to
  confirm the changes actually landed (see
  `~/.claude/projects/.../memory/feedback_verify_batched_edits.md`).

## Quick references

- Active backlog: `docs/BACKLOG.md` (B-9 active; B-1..B-8 closed).
- Architecture: `docs/ARCHITECTURE.md` (v2.0 reference).
- Decisions: `docs/DECISIONS.md` (D1 license, D2 INT pin, D3 SDK).
- Hardware: `docs/HARDWARE.md`.
- Master plan (historical): `ENC28J60_REFACTOR_PLAN.md` at the root.
- F0.x plans: `docs/superpowers/plans/`.
- Test rig scripts: `/tmp/eth-testrig-up.sh`, `down.sh`. Pcap to
  `/tmp/eth-testrig.pcap`.
- Flipper CLI debug: `/tmp/flipper-log.py` (pyserial @ 230400 on
  `/dev/ttyACM1`) and `/tmp/flipper-cmd.py`.
