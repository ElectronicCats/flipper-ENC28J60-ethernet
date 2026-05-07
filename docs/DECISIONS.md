# Architectural Decisions Log

Records of decisions taken during the refactor. Each entry: date, context,
options considered, decision, rationale, consequences.

---

## D1 — License posture of `libraries/chip/enc28j60.{c,h}`

**Date:** 2026-05-05
**Status:** DERIVATIVE_GPLV2 — Option A chosen (attribution + carve-out)

### Context

The repo `LICENSE` is MIT, Copyright (c) 2025 Electronic Cats. The driver
files at `EthernetAppDemo/libraries/chip/enc28j60.{c,h}` carry no in-file
license header. F0.0 audit was needed to confirm whether they are
original MIT-clean code or derivative of upstream code with a different
license.

### Findings (Task 2 audit, 2026-05-05)

The driver is a direct port of **EtherCard** (https://github.com/njh/EtherCard),
originally authored by Guido Socher and Jean-Claude Wippler, licensed
**GPL-2.0-or-later** (`/tmp/ethercard/LICENSE`, `src/enc28j60.cpp:1-9`
declares "Copyright: GPL V2"). EtherCard is itself based on AVRlib by
Pascal Stang.

Concrete evidence:

- `enc28j60.h:17-18` defines `ETHERCARD_SEND_PIPELINING` and
  `ETHERCARD_RETRY_LATECOLLISIONS` — distinctive EtherCard macros copied
  verbatim.
- Magic init constants in identical order at `enc28j60.c:439-449` vs
  EtherCard `src/enc28j60.cpp:389-397`: `EPMM0=0x303f`, `EPMCS=0xf7f9`,
  `PHLCON=0x476`, `MAIPG=0x0C12`, `MABBIPG=0x12`. These are EtherCard
  design choices, not chip-mandated.
- Function bodies match across `enc28j60_start`/`initialize`,
  `send_packet`/`packetSend`, `receive_packet`/`packetReceive`,
  `set_bank_with_mask`/`SetBank`, `read_register`/`readReg`,
  `write_Phy`/`writePhy`, including the silicon-rev-B7 workaround
  (`if (rev > 5) ++rev`) and the late-collision detection
  (`tsv[3] & 1<<5`). The port renames methods to snake_case, replaces
  Arduino SPI with Furi HAL SPI, and converts C++ class to C struct —
  none of which removes derivation.

`Spi_lib.{c,h}` is **original Electronic Cats code** (Furi HAL SPI),
MIT-clean. No Microchip AN833/AN1120 references found anywhere in
`libraries/chip/`.

### Options considered

- **A — Attribution + carve-out:** mark `enc28j60.{c,h}` as
  GPL-2.0-or-later (file-level), add `LICENSES/EtherCard.LICENSE` with
  the upstream license text, clarify in repo `LICENSE` and `README.md`
  that the driver is GPL-2.0 while the rest of the project remains MIT.
- **B — Relicense entire repo to GPL-2.0-or-later.**
- **C — Reimplement the driver from the Microchip ENC28J60 datasheet
  (DS39662) without consulting EtherCard source; restore full MIT.**

### Decision

**Option A — attribution + carve-out.** Selected by Sabas on 2026-05-05.

### Rationale

1. The driver is functional and debugged through several CI cycles and
   external contributor PRs. Reimplementing (Option C) introduces
   significant regression risk and delays the entire roadmap.
2. Sibling Electronic Cats apps (`flipper-MCP2515-CANBUS`,
   `flipper-rs485modbus`, `flipper-SX1262-LoRa`) are MIT. Relicensing
   this single repo to GPL-2.0 (Option B) creates an outlier and may
   conflict with downstream commercial use of the broader Electronic
   Cats ecosystem.
3. File-level license carve-outs are standard practice in firmware
   projects (RIOT-OS, NuttX, Zephyr).

### Consequences

F0.0 Task 5b applies the carve-out:

- 5-line SPDX-tagged attribution header added at the top of
  `enc28j60.c` and `enc28j60.h`.
- New file `LICENSES/EtherCard.LICENSE` with the full GPL-2.0 text from
  upstream EtherCard.
- `LICENSE` (top-level) gets a one-paragraph note referencing the
  carve-out.
- `README.md` "License" section updated to mention the dual posture.
- The `ETHERCARD_*` macro names in `enc28j60.h:17-18` are kept as-is —
  they document the derivation boundary; renaming them would weaken
  the attribution.

Downstream implication: any modification to `enc28j60.{c,h}` and any
distribution of a binary that links them must comply with GPL-2.0-or-later.
Source code release of those two files (already public on this repo) is
the standard mechanism. The MIT-licensed remainder of the project is
unaffected.

---

## D2 — INT pin GPIO selection

**Date:** 2026-05-05
**Status:** EXPOSED on Flipper external pin 10 (PA14 / SWCLK)

### Context

F0.5 plans to switch from polling `EPKTCNT` (`enc28j60.c:547`) to
interrupt-driven RX via the ENC28J60 `/INT` line. Required: the shield
must route `/INT` to a Flipper external GPIO not already claimed by
the SPI driver.

### Findings

GPIO already claimed by SPI driver (`Spi_lib.h:10-13`):

| Function | Flipper pin | STM32 GPIO | SDK macro |
|----------|-------------|------------|-----------|
| MOSI     | 2           | PA7        | `gpio_ext_pa7` |
| MISO     | 3           | PA6        | `gpio_ext_pa6` |
| CS       | 4           | PA4        | `gpio_ext_pa4` |
| SCK      | 5           | PB3        | `gpio_ext_pb3` |

`/INT` routing on the shield: confirmed by Sabas (maintainer) — wired to
**Flipper pin 10 / STM32 PA14 / SWCLK** alternate function.

### Options considered

- **A — Use the exposed pin 10 for interrupt RX.** SDK macro likely
  `gpio_swclk`; F0.5 implementer must verify in
  `firmware/targets/f7/furi_hal/furi_hal_resources.{c,h}`.
- **B — Stay on polling.** Lower throughput target, no SWD trade-off.

### Decision

**Option A — interrupt-driven RX on pin 10 (PA14 / SWCLK).**

### Rationale

The pin is exposed by the shield specifically for this purpose.
Throughput target ≥6 Mbps sustained sniff (with bulk SPI + INT) is
materially better than polling-only ~5 Mbps.

### Consequences

- F0.5 implementation: `furi_hal_gpio_init(swclk_pin,
  GpioModeInterruptFall, GpioPullUp, GpioSpeedLow)` (ENC28J60 `/INT` is
  active-low, open-drain — pull-up required), then
  `furi_hal_gpio_add_int_callback(swclk_pin, callback, ctx)`.
- The polling loop in `enc28j60.c:547` is replaced by a
  semaphore/flag set by the ISR; the new `rx_dispatch` thread (also
  F0.4 / F0.5) waits on it.
- **SWD debug attach is unavailable while the app runs** — the
  SWCLK line is repurposed as a GPIO input. Acceptable for production
  builds; for in-development debugging on this app specifically,
  developers must use UART logging rather than SWD.
- Documented in `docs/HARDWARE.md` (Task 7).

---

## D3 — SDK version pinning

**Date:** 2026-05-05
**Status:** PINNING_DEFERRED_TO_CI (manifest pin not supported by Unleashed fbt)

### Context

`application.fam` had no `fap_min_sdk_version`. CI uses Unleashed
`release` channel via `https://up.unleashedflip.com/directory.json`
(per `.github/workflows/build.yml`). Future F0 work depends on:

- `furi_hal_gpio_add_int_callback` (F0.5)
- `flipper_format_*` (F0.2)
- `furi_hal_rtc_get_timestamp` (F0.7)

### Findings

Effective SDK at audit: **Unleashed `unlshd-087`, API 87.8, Target 7**.

All 3 required APIs verified present:

- `furi_hal_gpio_add_int_callback` —
  `~/.ufbt/current/sdk_headers/f7_sdk/targets/f7/furi_hal/furi_hal_gpio.h:207`
- `flipper_format_buffered_file_alloc` (and `_file_alloc`,
  `_string_alloc`) — `~/.ufbt/current/sdk_headers/f7_sdk/lib/flipper_format/flipper_format.h:107,115,123`
- `furi_hal_rtc_get_timestamp` —
  `~/.ufbt/current/sdk_headers/f7_sdk/targets/f7/furi_hal/furi_hal_rtc.h:329`

**Critical deviation from the original plan:** the Unleashed fork of fbt
does **not** support the `fap_min_sdk_version` manifest field. Adding it
breaks build with:

```
fbt: warning: Failed parsing manifest '.../application.fam' :
     FlipperApplication.__init__() got an unexpected keyword argument
     'fap_min_sdk_version'
```

Verified: `~/.ufbt/current/scripts/fbt/appmanifest.py:35` defines
`FlipperApplication` and the field is absent. Grep across
`~/.ufbt/current/scripts/` returns zero hits for `fap_min_sdk`.

Unleashed embeds the SDK API version into the FAP header automatically
at build time (`elfmanifest.py:72`, `fbt_extapps.py:281`). Compatibility
is enforced **firmware-side at FAP load**, not by an app-declarative pin.

### Options considered

- **A — Pin via `fap_min_sdk_version` in `application.fam`.** Not
  available on Unleashed.
- **B — Pin via CI workflow** (`sdk-channel`, `sdk-index-url`,
  `ufbt-version`). Already in place at `.github/workflows/build.yml`.
- **C — Add `fap_version=(1, 0)` baseline** even though it is not a
  pin — provides a starting point for the refactor's app-version
  numbering.

### Decision

**Combination: B (CI-side pinning continues) + C (`fap_version=(1, 0)`
added to `application.fam`).** No app-declarative SDK pin.

### Rationale

The manifest pin is unavailable. Channel pinning at CI level is the
supported mechanism on Unleashed and already works correctly.

### Consequences

- `application.fam` now contains `fap_version=(1, 0)`.
- F0.0 plan amended: skip the `fap_min_sdk_version` step.
- Roadmap `docs/superpowers/specs/2026-05-05-roadmap-design.md` and
  master plan `ENC28J60_REFACTOR_PLAN.md` should note this constraint
  on next revision (the implicit assumption that "we pin SDK in the
  manifest" is wrong on Unleashed).
- If stricter pinning is wanted later, the lever is `ufbt-version` /
  `sdk-index-url` in CI, not the manifest. Out of scope for F0.0.
- Commit `401c6d4`.

---
