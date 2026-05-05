# Architecture — current state (pre-refactor snapshot)

> This is the **as-is** snapshot taken at the start of F0 (2026-05-05).
> It will be rewritten in F0.4 once the RX dispatcher refactor lands.
> Read alongside `ENC28J60_REFACTOR_PLAN.md` and the roadmap design doc
> at `docs/superpowers/specs/2026-05-05-roadmap-design.md`.

## Layers as they exist today

```
EthernetAppDemo/
├── application.fam              # FAP manifest (fap_version=(1,0); no SDK pin — see D3)
├── app_user.{c,h}               # entry point, App struct, view registry, threading
├── app_worker.c                 # ethernet worker thread (auto-ARP-reply, ICMP-reply)
├── scenes/                      # GUI per feature (15 scenes)
├── scenes_config/               # X-macro scene registry
├── modules/                     # feature logic per protocol/scanner (8 modules)
├── libraries/
│   ├── chip/                    # ENC28J60 driver + SPI shim (GPL-2.0 carve-out, see D1)
│   ├── protocol_tools/          # packet craft helpers
│   ├── functions/               # endian helpers
│   └── generals/                # ethernet_general
├── draw_functions/              # custom canvas helpers
├── assets/                      # icons
└── dist/                        # built FAP (tracked in git, ~95 KB current size)
```

## Threading model

Three thread types coexist:

- **Main app thread.** Stack 30 KB declared in `application.fam:7`. Runs
  the Furi event loop, scene callbacks, and ViewDispatcher.
- **Ethernet worker thread.** Allocated in `app_user.c:105` with stack
  10 KB. Single long-lived thread that polls the ENC28J60 RX FIFO and
  dispatches auto-replies for ARP requests and ICMP echo requests.
  Communicates with scenes via custom events on the ViewDispatcher.
- **Per-scene alternative thread (`app->thread_alternative`).** Scenes
  that run heavy scans use a "suspend worker, take radio, do work,
  resume worker" pattern. See `ArpScannerScene.c:196-211`,
  `ArpspoofingScene.c:13-36`, `OsDetector.c:64-70`,
  `PortsScannerScene.c:131-144`, `SnifferScene.c:14-22`.

Total stack pressure during heavy scans: roughly 40 KB (main + worker +
alt) plus the embedded `enc28j60_t` chip struct which holds two 1518-byte
buffers (`enc28j60.h:30-31`). Heap free during operation: estimated
120-150 KB out of ~200 KB available to FAPs.

## Scene registration

X-macro at `scenes_config/app_scene_config.h:6-57`. Each scene
contributes `on_enter`, `on_event`, `on_exit` via macro expansion in
`scenes_config/app_scene_functions.c`.

Scene inventory (15 scenes):

| Scene | Purpose |
|---|---|
| `MainMenuScene` | top-level menu |
| `AboutUsScene` | about/credits |
| `SettingsScene` | MAC/IP configuration (RAM-only, no persistence) |
| `GetIPScene` | trigger DHCP DORA |
| `ArpMenuScene`, `ArpScannerScene`, `ArpspoofingScene`, `ArpSpoofingSpecificIP` | ARP family |
| `PingScene` | ICMP echo |
| `PortsScannerScene` | TCP/UDP port scanner UI |
| `OsDetector` | OS fingerprinting UI |
| `SnifferScene`, `BrowserPcapScene`, `ReadPcapsScene` | sniff + PCAP I/O |
| `TestingScene` | dev-mode placeholder, scheduled for removal in F0.6 |

## RX flow today

1. Worker polls `enc28j60_t.EPKTCNT` (`enc28j60.c:547`). No interrupt is
   used — the chip's `/INT` line is wired to Flipper pin 10 (PA14 / SWCLK)
   per D2 but is not yet leveraged in software.
2. Reads next packet into `enc28j60_t.rx_buffer[1518]` (embedded in the
   chip struct, `enc28j60.h:30-31`).
3. Parses Ethernet header. If the frame is ARP or ICMP and the local IP
   matches `ethernet->ip_address`, the worker constructs and sends the
   reply directly. Otherwise the frame is discarded.
4. Scenes that need raw frames suspend the worker (`furi_thread_suspend`)
   and take exclusive ownership of the radio for the duration of the
   scene. On exit they resume the worker.

This will be replaced in F0.4 by a single `rx_dispatch` thread with a
list of registrable handlers.

## Driver layer

`libraries/chip/`:
- `enc28j60.c`, `enc28j60.h` — port of EtherCard's ENC28J60 driver
  (snake_case-converted, Furi HAL SPI swapped in). Carries a
  GPL-2.0-or-later carve-out (D1). Pre-refactor RX/TX layout: RX
  3 KB at `0x0000-0x0BFF`, TX 1.5 KB at `0x0C00-0x11FF`. Remaining
  3.5 KB of on-chip SRAM is unused.
- `Spi_lib.c`, `Spi_lib.h` — original Electronic Cats Furi HAL SPI shim,
  MIT-clean. Bus claims pins 2/3/4/5 (PA7/PA6/PA4/PB3) per
  `Spi_lib.h:10-13`.
- `log_user.h` — debug switches (currently `DEBUG_MESSAGE` and
  `SHOW_PACKETS_RECEIVED` are forced on in tree, scheduled to be flipped
  off in F0.6).

## Protocol craft

`libraries/protocol_tools/` (read in F0 audit; not yet enumerated in
this snapshot — see master plan §3 for the post-refactor target):

- Header builders/parsers for Ethernet, IPv4, ARP, ICMP, TCP, UDP, DHCP.
- Pure functions over byte buffers.
- This is the right shape; F1+ extends with `lldp.{c,h}`, `ipv6.{c,h}`,
  and OT protocols.

## Storage

- **PCAP files:** `/ext/apps_data/ethernet/files/pcap_DD_MM_YYYY_N.pcap`
  (`SnifferScene.c:70-92`, path constant in `app_user.h:48`). Live-write
  during capture is **not** flushed on a per-batch basis today —
  scheduled for F1.22.
- **Settings:** **none** — every value (MAC, IP, target IP, port range,
  hostname) resets on app exit. F0.2 introduces `settings.cfg` via
  `flipper_format`.

## Known issues at snapshot time

To be resolved during F0:

- F0.1 — three colliding `target_ip[4]` declarations
  (`OsDetector.c:4`, `ArpSpoofingSpecificIP.c:9`,
  `PortsScannerScene.c:10`); one is non-`static` so it leaks as a global
  symbol.
- F0.2 — no settings persistence.
- F0.3 — scanner scenes duplicate ~80-100 lines each of "resolve target
  → poll RX with timeout → mutate Submenu from worker thread" boilerplate.
- F0.4 — "suspend worker, take radio" pattern blocks composition of
  features.
- F0.5 — SPI per-byte acquire/release bottleneck in
  `read_buffer`/`write_buffer`; INT pin polled despite being routed.
- F0.6 — `ofp_tseq` (`os_detector_module.c:517-585`) ignores its
  argument and uses hardcoded debug IPs in production; `TestingScene`
  registered but unreachable; `flipper_process_dora` (no-hostname
  variant) is dead code; production printf hex-dump of every received
  frame in `enc28j60.c:24`.
- F0.7 — eight bugs: PCAP timestamps (`capture_module.c:42-48`),
  `is_duplicated_ip` underflow (`arp_module.c:323`),
  `tcp_send_xmas_probe` always returns false (`tcp_module.c:817`),
  `subnet_mask` written into a 6-byte MAC buffer
  (`dhcp_protocol.c:232,392`), missing `break` between TCP handshake
  cases (`tcp_module.c:349,437,467,583`), `pcap_scan` overflow on
  `packet_positions[2000]` (`ReadPcapsScene.c:5`,
  `capture_module.c:195`), worker auto-reply with default IP before
  DORA, 1 s blocking logo delay on every entry to MainMenu
  (`MainMenuScene.c:39`).

## Where this doc goes next

- F0.4 rewrites the "RX flow today" and "Threading model" sections to
  describe the dispatcher.
- F1 expands "Protocol craft" with the new harvesters (LLDP, mDNS,
  responder family).
- A user-facing `docs/ROADMAP.md` is created at the v2.0 release.
