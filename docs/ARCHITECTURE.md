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

- **Main app thread.** Stack 24 KB (`application.fam:7`). Runs the
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
  RX ring 3 KB at `0x0000-0x0BFF`, TX 1.5 KB at `0x0C00-0x11FF`.
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

## Passive Discovery migration / anti-regression ledger

The current Passive Discovery runtime has three implemented handlers:
LLDP, CDP, and EAPOL. Discover All performs one
`scanner_wait_for_packet` call per worker iteration and invokes all
three handlers' `process_frame` callbacks sequentially for the same
received frame. A single-protocol selection invokes only its matching
handler.

Runtime ownership invariants:

- The Passive orchestrator owns the scanner session, cancellation,
  multicast enable/disable, receive wait, and worker cleanup.
- Protocol handlers do not create workers, scanner sessions, RX
  registrations, or modify ENC28J60 filter state.
- A wait unregisters its RX handler and releases its semaphore/context
  before handler cleanup, multicast disable, scanner deinit, worker
  return, and GUI-side join/resource release.
- Main, Passive worker, and RX Dispatch stacks remain 24 KB, 4 KB, and
  4 KB respectively. The dynamic neighbor DB remains 32 entries.
  EAPOL added four one-byte fields to `neighbor_t` (476 to 480 bytes).
  The field-coverage pass retains LLDP Chassis/Port subtypes and an
  IEEE 802.3 PoE-TLV presence flag; alignment makes `neighbor_t` 484
  bytes and the family-lifetime payload 15,488 bytes.

Database and UI invariants:

- LLDP observations use `(source MAC, NEIGHBOR_SOURCE_LLDP)`; CDP uses
  `(source MAC, NEIGHBOR_SOURCE_CDP)`; EAPOL uses
  `(source MAC, NEIGHBOR_SOURCE_EAPOL)`. The same MAC may therefore
  have three independent protocol observations. EAPOL entries represent
  observed 802.1X participants/endpoints, not topology neighbors.
- Starting a single-protocol scan clears only that protocol's records;
  Discover All clears all three implemented sources.
- Filtered lists and details use the same source-relative ordinal. All
  mode uses the same global occupied-entry ordinal and labels each row
  with its source; detail rendering is selected from the record source.

### Passive Discovery roadmap field coverage

Occurrence priority is `core`, `common`, or `optional`. Status describes
the complete wire -> parser -> shared storage -> module -> details path.

#### LLDP

| Field | Wire source | Roadmap | Priority | Status and representation |
|---|---|---:|---|---|
| Source MAC | Ethernet header | supporting | core | Parsed, stored, displayed as MAC. |
| Chassis ID + subtype | TLV 1 | required | core | Parsed, stored, displayed with subtype context. Textual, MAC, and IPv4 network-address forms are represented; other network-address families remain unsupported. |
| Port ID + subtype | TLV 2 | required | core | Parsed, stored, displayed with subtype context. Textual, MAC, and IPv4 network-address forms are represented. |
| TTL | TLV 3 | supporting | common | Parsed, stored, displayed in seconds. |
| Port Description | TLV 4 | no | optional | Defined by the protocol but not parsed or stored. |
| System Name | TLV 5 | required | core | Parsed, stored, displayed with continuation pages. |
| System Description | TLV 6 | supporting | common | Parsed, stored, displayed with continuation pages. |
| System Capabilities | TLV 7 | supporting | common | Supported and enabled masks are parsed/stored; details decode IEEE roles and retain raw hex when the masks are equal. |
| Management IPv4 | TLV 8, address subtype 1 | required | core | Parsed, stored, displayed. Other address subtypes are not retained. |
| Port VLAN ID | IEEE 802.1 OUI, subtype 1 | required | optional | Parser length corrected to the six-byte organizational value; PVID is stored independently and displayed as Port VLAN ID. |
| VLAN Name VID/name | IEEE 802.1 OUI, subtype 3 | required | optional | Parser now consumes VID, name length, then name; VID and name are stored separately and displayed with an unambiguous Named VLAN ID label. |
| Network Policy VLAN | LLDP-MED OUI, subtype 2 | no | optional | VLAN bit extraction corrected and displayed as Network Policy VLAN. Application type, tagged/unknown flags, priority, and DSCP are not retained. |
| Basic Power via MDI | IEEE 802.3 OUI, subtype 2 | required | optional | Device type, supported flag, pair and class are parsed/stored; UI distinguishes TLV presence from MDI support. Enabled/pair-control and 802.3at/bt extensions remain unimplemented. |
| Extended Power via MDI | LLDP-MED OUI, subtype 4 | required | optional | Seven-byte value is accepted; device type, source, priority and 0.1-W power value are stored and displayed textually. |
| Requested/allocated PoE extensions | IEEE 802.3at/bt extensions | no | optional | Storage members exist but the wire fields are not parsed. |

#### CDP

| Field | Wire source | Roadmap | Priority | Status and representation |
|---|---|---:|---|---|
| Cisco destination | Ethernet destination `01:00:0C:CC:CC:CC` | required | core | Validated for every accepted frame; not persisted because it is framing, not neighbor state. |
| Source MAC | Ethernet header | supporting | core | Parsed, stored, displayed. |
| Version | CDP header | supporting | common | Parsed and validated (1/2), but not stored or displayed. |
| TTL | CDP header | supporting | common | Parsed, stored, displayed in seconds. |
| Device ID | TLV `0x0001` | supporting | core | Required for acceptance, stored in `name`, displayed with continuation pages. |
| Address / Management Address | TLV `0x0002` / `0x0016` | supporting | core | Bounded address-record parser retains the first NLPID IPv4 address; displayed as Management IPv4. Other protocols/addresses are not retained. |
| Port ID | TLV `0x0003` | supporting | core | Parsed, stored, displayed with continuation pages. |
| Capabilities | TLV `0x0004` | supporting | common | Parsed as 32 bits; low 16 bits are stored. Details decode Cisco roles and retain raw hex. High capability bits remain a shared-model limitation. |
| Software Version | TLV `0x0005` | supporting | common | Parsed, packed into shared description storage, then displayed on continuation pages. It receives a 77-character baseline share and borrows unused Platform capacity; very long combined values remain bounded by the shared field. |
| Platform | TLV `0x0006` | supporting | common | Parsed, packed into shared description storage, then displayed on continuation pages. It receives a 47-character baseline share and borrows unused Software capacity; very long combined values remain bounded by the shared field. |
| Native VLAN / Duplex | TLV constants `0x000A` / `0x000B` | no | optional | Defined but not parsed, stored, or displayed. |

#### EAPOL / EAP

| Field | Wire source | Roadmap | Priority | Status and representation |
|---|---|---:|---|---|
| Source MAC | Ethernet header | supporting | core | Parsed, stored, displayed. |
| EAPOL version | EAPOL header | supporting | common | Versions 1-3 are validated, stored, and displayed numerically. |
| Packet type | EAPOL header | Start required | core | EAP-Packet, Start, Logoff, and Key are parsed/stored and displayed by name; numeric fallback is retained for unknown stored values. |
| EAP code | EAP header | Identity supporting | common | Request, Response, Success, and Failure are parsed/stored and displayed by name. Success/Failure correctly have no Type byte. |
| EAP type | EAP Request/Response | Identity required | common | Parsed/stored for Request/Response and displayed by method name; unknown methods display `Unknown (n)`. |
| Response/Identity text | EAP type 1 data | required | core | Bounded printable-ASCII text is stored in `name`, preserved across later no-identity frames, and displayed with continuation pages. |
| EAP identifier | EAP header | no | optional | Present on wire but not parsed or persisted. |
| EAPOL-Key body | EAPOL type 3 | no | optional | Bounds/classification only; key material is intentionally never persisted or displayed. |

### Details UI contract

The former scene inserted an overview at scene page 0, passed `page - 1`
to the protocol renderer, and nevertheless wrapped using only the handler
page count. That made the overview a special entry-only page and made the
last declared handler page unreachable. Details are now strictly zero-based:
scene page N is handler page N, and forward/reverse navigation both wrap over
the exact handler-reported count.

Page order (a long value can add same-header continuation pages):

- LLDP: System Name; Source MAC/Management IPv4; Port ID; Chassis ID;
  TTL; Capabilities; System Description; Port VLAN ID/Named VLAN ID;
  VLAN Name; Network Policy VLAN; PoE/Power Class; MED Power and role.
- CDP: Device ID; Source MAC/Management IPv4; Port ID; TTL;
  Capabilities; Platform; Software Version.
- EAPOL: Identity; Source MAC/Packet Type; EAPOL Version/EAP Code;
  EAP Type.

Short pages keep the two header/value groups used elsewhere in the app.
Long values use a 120-pixel-wide Widget text box with the active proportional
`FontSecondary` (HaxrCorp 4089, seven-pixel height and eleven-pixel normal
leading). The helper conservatively paginates at 45 UTF-8-safe glyph
sequences: three 120-pixel lines divided by the font's eight-pixel maximum
glyph box. The Widget then performs final pixel-accurate, word-aware wrapping.
Fields received by LLDP are byte strings, but this font build contains the
95 printable ASCII glyphs; CDP and EAP Identity already sanitize to printable
ASCII. Non-ASCII LLDP rendering therefore remains a known limitation.

Human-readable conversions are deliberately display-only:

- LLDP capability bits: Other, Repeater, Bridge, WLAN AP, Router,
  Telephone, DOCSIS, Station, C-VLAN, S-VLAN, and TPMR.
- CDP capability bits: Router, Transparent Bridge, Source-Route Bridge,
  Switch, Host, IGMP, Repeater, Phone, Remote, CVTA, and TPMR.
- EAPOL packet types, EAP codes, and known EAP methods use names; unknown
  stored enum values include their number.
- VLAN IDs, TTL, EAPOL version, MAC/IP addresses, and capability raw masks
  remain numeric where the number is operationally useful.

### Roadmap receive-filter audit

- LLDP matches the active PMEN pattern: offset zero, mask bytes 12-13,
  checksum `0x7733` for EtherType `0x88CC`. UCEN, CRCEN, PMEN and BCEN
  remain enabled by default.
- CDP is IEEE 802.3 length + LLC/SNAP, so Ethernet bytes 12-13 are a length,
  not a fixed CDP EtherType. The roadmap's "same pattern-match" wording is
  technically inaccurate for the current two-byte EtherType pattern. CDP is
  functionally admitted by the orchestrator's temporary MCEN because its
  destination is Cisco multicast.
- EAPOL has EtherType `0x888E`, but PMEN remains programmed only for LLDP.
  Multicast EAPOL is functionally admitted by temporary MCEN and valid
  unicast EAPOL by UCEN. The roadmap-specific EAPOL PMEN optimization is not
  implemented.

No ENC28J60 filter, ERXFCON, PMEN pattern, or centralized multicast behavior
was changed in the field-coverage pass. Remaining roadmap work is a separate
filter design capable of admitting both EtherTypes without regressing normal
unicast/broadcast reception. Field-by-field deterministic frame -> parser ->
stored value -> physical pixel validation is also intentionally deferred.

CDP migration record:

- Reused from the historical implementation: Cisco destination MAC,
  IEEE 802.3 LLC/SNAP envelope, Device ID, Address, Port ID,
  Capabilities, Software Version, Platform, TTL, and version behavior.
- Rejected historical behavior: Address-TLV fixed offsets, accepting a
  packet after malformed/truncated TLVs, parsing past the IEEE 802.3
  declared payload, omitting checksum validation, MAC-only DB upsert,
  protocol-owned multicast changes, per-handler waits, and verbose
  logging while RX Dispatch synchronization is held.
- Current parser validates the standard one's-complement CDP checksum,
  CDP versions 1/2, every TLV header/length/boundary, and every declared
  Address record. It accepts IPv4 only from an NLPID `0xCC` record and
  also recognizes the Management Address TLV's identical record format.
- Known limitations: untagged IEEE 802.3 frames only; standard checksum
  behavior only; no VLAN-tag envelope; only the first supported IPv4
  address is retained; text is sanitized/truncated to shared-model
  capacity; the 32-bit CDP capability word is parsed but only its low
  16 bits are persisted because that is the existing displayed field.
  CDP version is validated but not persisted.

EAPOL migration record:

- EAPOL recognition is for untagged Ethernet-II EtherType `0x888E`.
  The parser deliberately does not require a destination address: the
  standard PAE group `01:80:C2:00:00:03` and valid unicast exchanges are
  both observations. Existing UCEN plus the Passive orchestrator's MCEN
  enable admits those paths without changing ENC28J60 filter masks.
- The EAPOL header and declared body must fit the captured frame. Versions
  1-3 and packet types EAP-Packet, Start, Logoff, and Key are supported.
  Start/Logoff require a zero-length body. Key requires a descriptor byte,
  but its body stays opaque: no key material, packet snapshot, credential,
  or per-frame heap allocation is retained or logged.
- Embedded EAP length is independently checked (`>= 4` and within the
  EAPOL body). Request/Response require the Type byte. Success/Failure are
  valid with their protocol-defined four-byte EAP packet and never read a
  Type byte. Response/Identity text is copied only from within EAP length,
  sanitized, bounded, and stored in the existing `name` field.
- Updating `(MAC, NEIGHBOR_SOURCE_EAPOL)` preserves an already learned
  non-empty identity whenever a later Start, Logoff, Key, or other EAP
  packet has no new identity. EAPOL never merges fields into LLDP or CDP.
- EAPOL has no worker, scanner session, RX registration, filter lifecycle,
  persistent protocol buffer, or protocol-specific database. The shared
  Passive worker owns receive/cancel/cleanup and dispatches one frame
  sequentially to LLDP, CDP, and EAPOL in Discover All.
- Known limitations: untagged Ethernet-II frames only; EAPOL versions 1-3
  and packet types 0-3 only; EAPOL-Key is classification/bounds-only; only
  Response/Identity text is retained, truncated to the shared 63-character
  name capacity. EAP identifier and raw authentication/key data are not
  persisted.

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
