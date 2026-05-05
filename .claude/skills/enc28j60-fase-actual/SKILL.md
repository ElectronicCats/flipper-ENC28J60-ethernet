---
name: enc28j60-fase-actual
description: Active context for the ENC28J60 refactor. Consult before any commit, file creation, or direction change. Updated at the start of every sub-phase.
---

# Current phase: F0.0 — Bootstrap (closing)

## What is done

- v2.0-f0.0 (closing):
  - Branch `refactor/phase-0` created from main.
  - uFBT 0.2.6 + Unleashed `unlshd-087` SDK (API 87.8, Target 7) verified.
  - Build green. FAP baseline: 95544 bytes.
  - **D1 resolved:** driver `enc28j60.{c,h}` is DERIVATIVE_GPLV2 of EtherCard upstream. **Option A applied** — file-level GPL-2.0-or-later carve-out with SPDX headers, `LICENSES/EtherCard.LICENSE` added, `LICENSE` and `README.md` updated. Rest of the project remains MIT.
  - **D2 resolved:** ENC28J60 `/INT` is wired to Flipper external pin 10 (PA14 / SWCLK). F0.5 will use interrupt-driven RX. SWD debug attach unavailable during runtime (acceptable trade-off).
  - **D3 resolved with deviation:** Unleashed fbt does NOT support `fap_min_sdk_version`. SDK pinning lives in CI workflow only. `fap_version=(1, 0)` added to `application.fam` as baseline. All 3 required APIs verified present (`furi_hal_gpio_add_int_callback`, `flipper_format_*`, `furi_hal_rtc_get_timestamp`).
  - `docs/DECISIONS.md`, `docs/ARCHITECTURE.md` v0, `docs/HARDWARE.md` written.
  - GitHub issue templates added (`bug_report`, `feature_request`, `hardware_compat`, `config.yml`).
  - GitHub Discussions enablement pending user manual action (gh CLI not available locally).

## Where we are

F0.0 closing. About to enter F0.1 (centralize `scan_params` into App).

## What NOT to touch

- Runtime code in `EthernetAppDemo/modules/`, `scenes/`, `libraries/chip/` — code refactors start in F0.1, not F0.0.
- `application.fam` — already at the F0.0 target shape (`fap_version=(1, 0)` only).
- The `main` branch — work continues on `refactor/phase-0`.
- ARP-spoof-to-IP feature — gated by current `DEV_MODE`, leave as-is until F0.8 introduces `PENTEST_MODE` flag.
- The `ETHERCARD_*` macro names in `enc28j60.h:17-18` — they document the derivation boundary; renaming weakens the GPL-2.0 attribution.

## Exit criteria for F0.0

- [x] branch `refactor/phase-0` created
- [x] uFBT build verified green untouched
- [x] D1 (license) recorded, Option A applied
- [x] D2 (INT pin) recorded
- [x] D3 (SDK pin) recorded — manifest pin not available; CI-side only
- [x] `docs/ARCHITECTURE.md` v0 committed
- [x] `docs/HARDWARE.md` committed
- [x] `docs/DECISIONS.md` committed
- [x] `LICENSES/EtherCard.LICENSE` + SPDX headers + LICENSE/README updates committed
- [x] `.github/ISSUE_TEMPLATE/` committed (bug, feature, hardware + config.yml)
- [ ] GitHub Discussions enabled (pending user manual action via UI)
- [ ] tag `v2.0-f0.0` created (pending Task 11)

## Extra rules during F0

- All commit messages in English.
- Each sub-phase ends with one annotated tag `vMAJOR.MINOR-fN.M`.
- Do not promote ARP-spoof-to-IP into the standard menu until F0.8 introduces `PENTEST_MODE`. Leave the existing `DEV_MODE` gate as-is.
- Do not delete files claiming they are "dead" without a fresh `grep -rn` confirming zero callers.
- After every `ufbt` build, run `git checkout -- EthernetAppDemo/dist/` to discard tracked-but-rebuilt artifacts before commit, unless the task explicitly requires committing the new `.fap` (only at release tags).
- The two driver files (`enc28j60.c`, `enc28j60.h`) are GPL-2.0-or-later — modifications to them must preserve their SPDX header. Any new code that you write into them is also under GPL-2.0.

## Quick references

- Master plan: `ENC28J60_REFACTOR_PLAN.md` (root).
- Roadmap design: `docs/superpowers/specs/2026-05-05-roadmap-design.md`.
- F0.0 task plan: `docs/superpowers/plans/2026-05-05-f0-0-bootstrap.md`.
- D1/D2/D3 details: `docs/DECISIONS.md`.
- Architecture: `docs/ARCHITECTURE.md`.
- Hardware: `docs/HARDWARE.md`.
- F1.x findings (raw): `/tmp/f0-0-d{1,2,3}.md` — temp, will be cleaned at end of F0.0.
