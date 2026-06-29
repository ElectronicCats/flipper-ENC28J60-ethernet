# Architecture — v2.0

> Current state of the app at the v2.0 release tag. Replaces the
> F0-start "pre-refactor snapshot" that lived here through Phase 0.
> Read alongside `docs/DECISIONS.md`, `docs/HARDWARE.md`,
> `docs/BACKLOG.md`.

## Layers

```
EthernetAppDemo/
├── application.fam              # FAP manifest (fap_version=(1,0); no SDK pin — see D3)
├── app_user.{c,h}               # entry point, App struct, view registry,
│                                # auto-ARP / auto-ICMP handler registration
├── scenes/                      # GUI per feature (14 scenes; TestingScene removed in F0.6)
├── scenes_config/               # X-macro scene registry
├── modules/                     # feature logic per protocol/scanner
│                                # (arp / dhcp / tcp / udp / icmp / capture /
│                                #  analysis / ping / os_detector)
├── libraries/
│   ├── chip/                    # ENC28J60 driver + Spi_lib + rx_dispatch
│   │                            # (GPL-2.0 carve-out on enc28j60.{c,h}, see D1)
│   ├── scanner/                 # scanner_session_t primitive (subnet-aware
│   │                            # next-hop resolve + cache + cancellable wait
│   │                            # + send-then-wait race protection)
│   ├── settings/                # flipper_format-backed persistence (schema v2)
│   ├── protocol_tools/          # packet craft helpers
│   ├── functions/               # endian helpers
│   └── generals/                # ethernet_general
├── draw_functions/              # custom canvas helpers
└── assets/                      # icons
```

`dist/` (the built FAP) is **not** tracked since F0.5f. Distribution
is via GitHub Releases or the upcoming F0.8 CI workflow.

## Threading model

Three thread types coexist:

- **Main app thread.** Stack 30 KB (`application.fam:7`). Runs the
  Furi event loop, scene callbacks, and ViewDispatcher.
- **`rx_dispatch` thread.** Stack 4 KB (`libraries/chip/rx_dispatch.c`).
  Single long-lived thread woken by the ENC28J60 `/INT` line on
  PA14 (F0.5c). On wake it drains every queued packet via
  `receive_packet`, then walks a `(predicate, handler, ctx)`
  registry and invokes any matching handler. Handlers run inside the
  dispatch mutex to keep `rx_unregister` correct against in-flight
  invocations (F0.5d). `app_alloc` registers two static handlers:
  `auto_arp` (replies to ARP requests) and `auto_icmp` (replies to
  ICMP echo requests, F0.5g — match limited to ECHO_REQUEST).
- **Per-scene alternative thread (`app->thread_alternative`).** Scenes
  that run scans or DORA spawn one alt thread, drive it from the
  main thread via custom view-dispatcher events, and join in
  `on_exit` (with cancel signalling for DORA, F0.5f).

The pre-refactor "Ethernet worker thread" / `app_worker.c` /
`flag_dhcp_dora` signalling pattern is gone (F0.4e). The
"suspend worker, take radio" pattern across scanner scenes is gone
(F0.4a — replaced by handler registry; F0.5a — chip mutex makes the
remaining pause/resume callsites unnecessary; F0.4f — last
arp_get_specific_mac path migrated).

Total stack pressure during a scan: ≈ 40 KB (main + rx_dispatch + alt)
plus the embedded `enc28j60_t` chip struct holding two 1518-byte
buffers (`enc28j60.h:42-43`). Heap free during operation: ≈ 120-150
KB out of ~200 KB available to FAPs.

## Scene registration

X-macro at `scenes_config/app_scene_config.h`. Each scene contributes
`on_enter`, `on_event`, `on_exit` via macro expansion in
`scenes_config/app_scene_functions.c`.

Scene inventory (14 scenes):

| Scene | Purpose |
|---|---|
| `MainMenuScene` | top-level menu |
| `AboutUsScene` | about/credits |
| `SettingsScene` | MAC/IP configuration (persisted via `settings.cfg` since F0.2) |
| `GetIPScene` | DHCP DORA, runs in alt thread, cancellable from Back (F0.5f) |
| `ArpMenuScene`, `ArpScannerScene`, `ArpspoofingScene`, `ArpSpoofingSpecificIP` | ARP family |
| `PingScene` | ICMP echo, migrated to scanner_wait_for_packet (F0.4g.1) |
| `PortsScannerScene` | TCP/UDP port scanner UI |
| `OsDetector` | OS fingerprinting UI — labelled EXPERIMENTAL since F0.5h, see B-9 |
| `SnifferScene`, `BrowserPcapScene`, `ReadPcapsScene` | sniff + PCAP I/O |

## RX flow

1. ENC28J60 `/INT` (PA14 / SWCLK / Flipper external pin 10) drives a
   falling-edge GPIO ISR. The ISR sets `RX_FLAG_INT` on the dispatcher
   thread and returns — no chip I/O, no mutex acquire (F0.5c).
2. The `rx_dispatch` thread blocks on `furi_thread_flags_wait` with a
   100 ms fallback timeout (errata DS80349 safety net). On wakeup it
   drains all queued packets via `receive_packet` until that returns 0.
3. Each public chip function takes the per-instance `enc28j60_t.mutex`
   so bank state and SPI transactions can't be torn between threads
   (F0.5a — closes B-6).
4. For each drained frame the dispatcher walks the slot registry under
   the dispatch mutex (F0.5d) and invokes handlers whose predicate
   returns true. Handlers must be fast — see the contract docstring
   in `rx_dispatch.h`.
5. Scenes that need to pair a send with a single matching reply use
   `scanner_wait_for_packet` (`libraries/scanner/scanner_session.c`),
   which registers a per-call predicate and an optional `trigger_fn`
   that fires after registration but before the wait — closing the
   send→register race (F0.5d). DORA in `GetIPScene` still uses
   `rx_dispatch_pause/resume` because the DHCP state machine consumes
   raw packets in order; that's the only remaining pause/resume call
   site.

## Driver layer

`libraries/chip/`:

- `enc28j60.{c,h}` — port of EtherCard's ENC28J60 driver
  (snake_case, Furi HAL SPI). GPL-2.0-or-later carve-out, D1.
  Per-instance `FuriMutex* mutex` serializes register access (F0.5a).
  RX ring 6 KB at `0x0000-0x17FF`, TX 2 KB at `0x1800-0x1FFF`.
- `Spi_lib.{c,h}` — Electronic Cats Furi HAL SPI shim, MIT.
  Bus pins 2/3/4/5 = PA7/PA6/PA4/PB3.
- `rx_dispatch.{c,h}` — single-thread RX dispatcher, INT-driven,
  handler registry. See "RX flow" above.

## Scanner primitive

`libraries/scanner/scanner_session.{c,h}`:

- `scanner_session_t` — per-scene context. Holds the chip handle,
  borrowed pointers into `App` (gateway IP/MAC, subnet mask), and a
  4-entry round-robin MAC cache.
- `scanner_resolve_next_hop` — subnet-aware ARP resolve with cache.
  Builds the ARP request and arms `scanner_wait_for_packet` with a
  match predicate; trigger sends the request after registration
  (F0.5d closes the race).
- `scanner_wait_for_packet` — registers a predicate, optionally fires
  a trigger, blocks on a semaphore until match or timeout. Predicate
  captures state via `pred_ctx`; the rx_buffer is no longer guaranteed
  to hold the matched frame after wake (F0.5g doc fix). Honors the
  back button as cancel.

## Storage

- **PCAP files:** `/ext/apps_data/ethernet/files/pcap_DD_MM_YYYY_N.pcap`.
  Live-write during capture; per-batch flush still scheduled for F1.
  Reader hardened in F0.5e: `pcap_get_specific_packet` clamps record
  `orig_len` to the caller's buffer; `pcap_scan` uses `storage_file_seek`
  instead of reading into a fixed buffer.
- **Settings:** `settings.cfg` via `flipper_format`. Schema v2 (F0.5f):
  MAC, IP, `is_static_ip`, `is_dora`, gateway IP, gateway MAC,
  scan_params block. v1 files load with v2 fields defaulting.

## Resolved during F0

- **F0.1** — three colliding `target_ip[4]` declarations centralized
  into `App.scan_params`.
- **F0.2** — settings persistence via `flipper_format`.
- **F0.3a** — scanner-session primitive replaces ~80-100 lines of
  per-scene boilerplate.
- **F0.4a / F0.4e / F0.4f** — RX dispatcher replaces the worker-thread
  + suspend pattern; `app_worker.c` deleted; last `arp_get_specific_mac`
  callsite migrated for the scanner path.
- **F0.4g.1** — PingScene migrated off direct `receive_packet`.
- **F0.5a** — chip-level `FuriMutex` (B-6 closed).
- **F0.5b** — skipped intentionally; chip protocol forces a CS toggle
  per command, so a "bulk SPI" rewrite had no real win.
- **F0.5c** — INT-pin–driven RX dispatcher (D2 landed).
- **F0.5d / wave2 / e / f / g / h** — race fixes, defensive
  bounds checks, settings v2, DORA cancellation, doc cleanups,
  OS Detector quick-wins + EXPERIMENTAL label.
- **F0.6** — `ofp_tseq` debug-IP removal, `TestingScene` removal,
  dead `flipper_process_dora` removed, production printf hex-dump
  silenced.
- **F0.7** — eight bugs from the audit (PCAP timestamps, `is_duplicated_ip`
  underflow, `tcp_send_xmas_probe` always false, MAC/subnet length
  mismatch, TCP fall-through fix, `pcap_scan` overflow, pre-DORA
  auto-reply IP conflict, MainMenu logo blocking).

## Deferred to F1

- **B-9** — OS Detector reliability rewrite (direct RX vs dispatcher,
  sample-slot collision, premature PORT_OPEN before ACK validation,
  blocking UI). v2.0 ships with the feature labelled EXPERIMENTAL.
- **F0.4g.2 (open)** — migrate the remaining direct `receive_packet`
  callers in `os_detector_module` (burst probe). PingScene done in
  F0.4g.1.
- **B-7 follow-up** — `arp_get_specific_mac` still has callers in
  `tcp_module` (only inside dead-code `tcp_handshake_process` /
  `_spoof`) and `ArpSpoofingSpecificIP`. Cleanup task: delete the
  dead handshake helpers, migrate ArpSpoofing.
- **F0.8** — dual-build CI (admin / pentest .fap variants).
- **F1 protocol craft expansion** — LLDP, mDNS, IPv6, responder
  family.
- **PCAP per-batch flush** — currently relies on the FAT cache.

## See also

- `docs/DECISIONS.md` — D1/D2/D3 (license, INT pin, SDK).
- `docs/HARDWARE.md` — pinmap, INT wiring.
- `docs/BACKLOG.md` — open bugs (B-9 active; B-1..B-8 closed).
