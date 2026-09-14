# FLIPPER ETHERNET — Current Software Architecture

This document describes the current implementation under `EthernetAppDemo/`.
Current source is authoritative when this document and code differ.

User-visible requirements live in [`APP_BEHAVIOR.md`](APP_BEHAVIOR.md),
hardware details in [`HARDWARE.md`](HARDWARE.md), accepted decisions in
[`DECISIONS.md`](DECISIONS.md), and unresolved work in
[`BACKLOG.md`](BACKLOG.md). Dated plans and specifications under
`docs/superpowers/` and `ENC28J60_REFACTOR_PLAN.md` are historical records,
not current architecture.

## 1. Build and source layout

The repository currently builds one external application target from
`EthernetAppDemo/application.fam`:

- application ID: `ethernet_app`;
- entry point: `app_main`;
- main FAP stack: 24 KiB;
- `PENTEST_MODE=1` is defined by the manifest;
- `DEV_MODE=0` is defined in source.

`PENTEST_MODE` is not currently used to produce separate admin and pentest
source variants. The checked-in CI workflow builds one FAP. The two-artifact
design described by historical roadmap documents is not implemented.

Primary source areas are:

- `app_user.{c,h}` — application state and lifecycle;
- `scenes/` and `scenes_config/` — scene callbacks and X-macro registration;
- `modules/` — feature orchestration and network operations;
- `libraries/chip/` — ENC28J60, SPI, and RX Dispatch;
- `libraries/scanner/` — reusable next-hop and packet-wait session;
- `libraries/settings/` — versioned `flipper_format` configuration;
- `libraries/protocol_tools/` — packet construction and parsing;
- `draw_functions/` — custom views and IPv4 editor;
- `assets/` — compiled GUI images/icons.

## 2. Scene and GUI model

The scene manager is generated from `scenes_config/app_scene_config.h`.
There are 24 active scenes and two additional `DEV_MODE`-only scenes.

| Category | Active scenes |
|---|---|
| Navigation | `main_category_menu`, `main_menu`, `pentest_menu` |
| Capture | `sniffer`, `browser_pcaps`, `read_pcap` |
| ARP | `arp_actions_menu`, `arp_spoofing`, `arp_scanner_menu`, `arp_scanner`, `arp_ip_show_details` |
| Passive | `passive_discovery`, `passive_neighbor_list`, `passive_neighbor_details` |
| Network tools | `get_ip_scene`, `ports_scanner`, `os_detector`, `ping_menu_scene`, `ping_set_ip_scene`, `ping_scene` |
| Settings/about | `settings`, `settings_options_menu`, `set_address`, `about_us` |
| `DEV_MODE` only | `arp_spoofing_specific_ip_menu`, `arp_spoofing_specific_ip` |

The `App` owns reusable GUI objects rather than allocating them per scene:
`SceneManager`, `ViewDispatcher`, `Widget`, `Submenu`, `VariableItemList`,
`TextBox`, `ByteInput`, `NumberInput`, `FileBrowser`, `Loading`, and the
custom IPv4 editor. Scenes reset and repopulate these objects as needed.

Workers publish custom events through the ViewDispatcher. Whether every
repository-visible direct worker mutation of Widget, Submenu, or FuriString
is supported, and the precise ordering of queued events during scene changes,
remain platform-contract questions.

## 3. Central application ownership

`App` is allocated by `app_alloc()` and survives until `app_free()`. It owns:

- GUI records, managers, views, reusable modules, and shared strings;
- the Storage and Dialog records and one reusable `File` object;
- one `enc28j60_t` instance and its SPI/mutex/frame-buffer resources;
- scan parameters and network configuration;
- Scan Hosts results and timestamp;
- Passive live database, selected mode, and history UI state;
- PCAP path, index, packet selection, and reader/capture state;
- one application-owned feature-worker slot;
- feature stop/cancel flags and registered RX handles.

### Feature-worker contract

`App.thread_alternative` and `App.thread_owner` implement a single shared
feature-worker slot:

1. `app_thread_claim()` accepts a worker only when the slot is empty.
2. A feature sets its own stop/cancel state before shutdown.
3. `app_thread_join_and_free()` joins the claimed thread, clears the App
   pointer and owner, and frees the thread object.
4. `app_thread_shutdown()` is the application-level fallback that signals all
   feature stop flags and joins the current owner before shared resources are
   destroyed.

Only one application-owned feature worker can therefore run at a time, and a
claimed worker is joined before its thread object or shared application state
is freed. Platform-owned workers, including FileBrowser internals, are outside
this ownership slot.

Configured feature-worker stacks are currently:

| Worker | Stack |
|---|---:|
| Scan Hosts, Ping, ARP Spoof All | 10 KiB each |
| Scan Ports, OS Detector | 5 KiB each |
| Get IP, Sniffer, PCAP reader | 4 KiB each |
| Passive Discovery, About | 3 KiB each |

These workers are mutually exclusive and must not be summed as simultaneous
feature stacks.

## 4. ENC28J60 and SPI ownership

The App owns one heap-allocated `enc28j60_t`. The object owns:

- the external SPI handle;
- a per-instance `FuriMutex`;
- current register-bank and RX-ring state;
- a shared 1,518-byte RX frame buffer;
- a shared 1,518-byte TX frame buffer.

Public controller operations serialize register-bank state and SPI transfers
through the ENC mutex. The mutex covers each receive or send operation; it does
not extend the lifetime of data in the shared RX buffer after
`receive_packet()` returns.

Application allocation sets defaults, allocates/starts the ENC controller,
loads settings, applies the configured MAC, and then initializes RX Dispatch
and its permanent handlers. Application teardown stops RX Dispatch before the
ENC/SPI objects are freed. Detailed pins, filters, controller SRAM layout, and
unresolved HAL guarantees are documented in `HARDWARE.md`.

## 5. Receive architecture

### RX Dispatch

`libraries/chip/rx_dispatch.c` contains one process-global dispatcher object,
`g_dispatch`. Its 4 KiB thread is long-lived for the App session.

The ENC `/INT` signal on PA14 sets a thread flag. A 100 ms timed wait provides
a fallback wake-up. On each wake the dispatcher drains the hardware receive
queue into `ethernet->rx_buffer` and evaluates registered predicates and
handlers.

The registry has eight fixed slots. Two are normally occupied for the App
lifetime:

- automatic ARP request replies;
- automatic ICMP Echo Request replies.

Registry mutation and predicate/handler execution are serialized by the
registry mutex. An unregister operation therefore waits for an invocation
currently holding that mutex to complete. Handlers must not register or
unregister recursively and should avoid slow work while the registry is held.

Temporary scanner waits register their predicate before executing the
optional trigger/send operation. The wait context and semaphore remain alive
until the registration is removed. The frame pointer supplied to a callback
is valid only during that callback.

### Current direct-RX exceptions

RX Dispatch is the primary receive path, but it is not a universal exclusive
owner in current source.

OS Detector's TCP probe loop calls `receive_packet()` directly while RX
Dispatch remains active. There is no OS-specific dispatcher registration for
those TCP replies and no dispatcher pause around this loop. Both contexts can
consume the hardware queue and both use `ethernet->rx_buffer`. Consequently,
a frame may reach the wrong consumer and one context can replace the shared
buffer before the other finishes parsing it. This is an active architectural
defect, not the intended RX model.

DHCP also consumes raw packets directly, but Get IP surrounds the DHCP state
machine with `rx_dispatch_pause()`/`rx_dispatch_resume()`. Pause requests wake
the dispatcher and wait for an acknowledgement for a bounded period. The
function returns even if acknowledgement was not observed, so it is not a
proven hard quiescence barrier under delayed scheduling.

Legacy direct-receive helpers also remain in disabled or currently uncalled
paths. They must not be treated as active defects without proving a caller.

## 6. Transmit ownership

Foreground feature code may construct frames in the shared ENC TX buffer.
The one-feature-worker contract prevents current foreground writers from
overlapping one another. Actual controller sends also serialize through the
ENC mutex.

Automatic ARP and ICMP responders construct replies in local buffers rather
than the shared TX buffer. There is therefore no current foreground-versus-
automatic-responder TX-buffer race. Mutable ARP construction globals remain a
separate concurrency concern when automatic ARP handling and OS direct-RX ARP
handling overlap.

## 7. Scanner sessions

`scanner_session_t` is a feature-owned, stack-resident session containing:

- borrowed App, ENC, ViewDispatcher, network, and cancellation state;
- subnet-aware next-hop selection;
- a four-entry round-robin MAC cache;
- temporary RX wait state.

`scanner_resolve_next_hop()` selects the target itself when it is on-subnet and
the gateway otherwise, then resolves/caches the corresponding MAC.
`scanner_wait_for_packet()` allocates a semaphore, registers a temporary RX
predicate, performs its trigger only after registration, waits with timeout
and cancellation checks, unregisters, and then destroys the wait resources.

Scan Hosts, Ping, Ports, and parts of OS Detector use scanner sessions. Use of
a scanner session does not imply that every receive operation in that feature
is dispatcher-owned; OS Detector is the current counterexample.

## 8. Network-state model

Network state is distributed across the ENC object and `App`:

| State | Location/meaning |
|---|---|
| MAC, IPv4, subnet mask | `enc28j60_t` active controller/application values |
| Gateway IPv4/MAC | `App.ip_gateway`, `App.mac_gateway` |
| Acquisition flags | `App.is_dora`, `App.is_static_ip` |
| Controller-start cache | `App.enc28j60_connected` |
| Live physical link | Read from the ENC PHY when a feature asks |
| Ping target | `App.scan_params.ip_ping` |
| Ports/OS target | `App.scan_params.target_ip` |
| Port/range/protocol | Other `App.scan_params` fields |

Controller-start success, current PHY link, and usable network configuration
are different states. `enc28j60_connected` is a cached controller-start result,
not a live link indication. It is not consistently invalidated or refreshed,
which can produce stale user-visible availability decisions.

Current state transitions also have known consistency defects:

- manual IP confirmation replaces the IP and changes acquisition flags while
  retaining the previous subnet, gateway IP, and gateway MAC;
- cancellation after a DHCP ACK can occur after tuple fields are copied but
  before acquisition flags are finalized;
- Ping and Ports/OS store separate targets despite the intended shared Target
  IP behavior in `APP_BEHAVIOR.md`.

## 9. Settings and configuration

`libraries/settings/settings.c` uses `flipper_format` at
`/ext/apps_data/ethernet/settings.cfg`.

- The writer emits schema version 3.
- The loader accepts versions 1, 2, and 3.
- Loading overlays parsed values onto initialized defaults.
- Normal source serialization occurs in `app_free()`.

Current persisted values are:

- MAC address, IPv4 address, `is_static_ip`, and a normalized `is_dora`;
- gateway IPv4, gateway MAC, and subnet mask;
- target IP, target port, port range, and protocol index;
- Ping IP, Scan Hosts start IP, and Scan Hosts range.

The user-level contract requires explicit confirmation to commit and persist
an edit. Current custom IPv4 editors mutate bound App fields before
confirmation, and physical settings serialization normally waits until
application teardown. Those differences are documented in `APP_BEHAVIOR.md`
and `BACKLOG.md`.

Settings load/save is not transactional. A create-always rewrite can leave a
partial file after an I/O failure, and loading can retain an earlier subset of
successfully parsed fields when later fields are absent or malformed. Numeric
scan fields are not all semantically range-checked.

## 10. Persistent data

| Artifact | Current architecture |
|---|---|
| `settings.cfg` | Textual/versioned `flipper_format`; schemas 1–3 accepted, schema 3 written; non-transactional rewrite |
| `last_scan.bin` | Latest Scan Hosts timestamp/count/result snapshot; native unversioned binary layout |
| `passive_history.bin` | Versioned, bounded, checksummed Passive History |
| `passive_history.tmp` | Temporary output used while rewriting Passive History |
| `files/*.pcap` | Captures written by Sniffer and read by the PCAP browser/reader |

`App.file` is reused sequentially at application level. A feature closes it
before another active flow uses it. SDK-internal File or FileBrowser ownership
remains platform-defined.

`last_scan.bin` validates its header but does not prove that the full result
body was read before exposing its accepted count. Its writer does not
transactionally replace the prior file or verify every write.

Passive History is the strongest repository-owned persistence boundary. It
checks magic/version/count/body limits and CRC before committing decoded
records, and it writes through a temporary file. Atomic replacement semantics
for the final rename remain filesystem-dependent.

## 11. Passive Discovery

Passive Discovery has one 3 KiB feature worker and one live `neighbor_db_t`
with 32 entries. Entries are keyed by `(source MAC, protocol source)`, so the
same MAC may have distinct LLDP, CDP, and EAPOL observations.

The worker owns one scanner session, one temporary RX wait at a time,
cancellation, multicast-filter enable/disable, and protocol dispatch. Discover
All sends each received frame sequentially through LLDP, CDP, and EAPOL
handlers; a single-protocol mode invokes only its selected handler. Protocol
handlers do not own workers, RX registrations, or filter transitions.

Normal scene cleanup stops/joins the worker, unregisters receive state,
restores multicast filtering, merges live observations into saved history, and
then releases the live database. Merge/write failure is currently ignored by
scene logic. `app_free()` has fallback worker/database cleanup but does not
itself perform the normal history merge. Whether normal top-level dispatcher
shutdown always invokes the scene's exit callback first is an unresolved SDK
contract.

## 12. Sniffer and PCAP reader

The active flow is:

```text
Sniffer RX handler
  -> capture File under apps_data/ethernet/files
  -> optional direct open or FileBrowser selection
  -> PCAP reader
  -> offset index
  -> selected packet analysis and TextBox rendering
```

Sniffer normally enables promiscuous reception, registers its capture handler,
writes PCAP records, then unregisters the handler before closing the File and
restoring the baseline filter. Capture write failure is not fully propagated
to its displayed packet count.

The reader allocates a `uint64_t[2000]` packet-position index: a 16,000-byte
contiguous payload. It caps the displayed/indexed capture at 2,000 packets.
Current record scanning does not completely validate PCAP version, link type,
snaplen, record lengths, or remaining file boundaries, and generic packet
analysis contains active length/arithmetic safety defects. It must not be
described as fully hardened.

## 13. Memory and resource model

Always-resident source-visible resources include the 24 KiB main stack, App
and GUI objects, ENC object and two 1,518-byte buffers, ENC mutex/SPI handle,
the 4 KiB RX Dispatch stack, registry mutex, and two permanent handlers.
Exact SDK object sizes and allocator overhead are external.

Important feature allocations include:

- PCAP reader index: 16,000 contiguous payload bytes;
- Passive neighbor DB: 32 × `sizeof(neighbor_t)`, currently approximately
  15.5 KiB contiguous payload;
- data-dependent Submenu, Widget, FileBrowser, File, and FuriString storage;
- feature-worker stack and thread metadata;
- temporary scanner semaphore/registration state;
- automatic ICMP reply payload and call-stack demand on RX Dispatch.

Passive DB and the PCAP index do not coexist under current navigation and the
one-feature-worker model. Mutually exclusive feature-worker stacks must also
not be summed.

Source does not prove an active memory leak or persistent allocator
fragmentation. The architecture is sensitive to total free heap and to the
largest available contiguous block. RX automatic ICMP, OS Detector, and PCAP
reader stacks require runtime high-water validation before any overflow claim.

## 14. Startup guards

Several feature entry points call `startup_guard_check()` before allocating a
worker or another large resource. Guards compare two different constraints:

- total free memory;
- largest contiguous allocatable block.

Their formulas include source-maintained assumptions for alignment, allocator
headers, thread metadata, semaphores, and reserve space. Those exact values
depend on the active SDK/allocator contract. A successful guard does not
reserve memory, and later GUI, string, storage, scanner, or automatic-handler
allocations may still occur.

Guards are therefore advisory preflight checks, not complete proofs that all
feature allocations will succeed.

## 15. Application teardown

Normal application shutdown follows this source-visible order:

1. signal and join the active feature worker;
2. perform feature-owned fallback cleanup where required;
3. save current settings;
4. unregister permanent automatic RX handlers;
5. remove the GPIO callback and stop/join RX Dispatch;
6. free GUI modules, managers, shared File/strings, ENC/SPI, and records.

RX Dispatch is joined before the ENC object and SPI handle are freed. A later
application allocation resets and starts the controller again before permanent
RX handlers become active.

The following remain external contract questions:

- whether top-level scene shutdown always invokes the active scene's
  `on_exit()` before `app_free()` fallback logic;
- cross-thread GUI mutation and custom-event publication/queue ordering;
- FileBrowser callback/worker quiescence;
- GPIO interrupt callback-removal quiescence;
- whether controller RX/interrupt enables require additional explicit shutdown;
- whether the active hardware path excludes short/runt frames before outer
  application wrappers inspect fixed Ethernet offsets.

## 16. Current architectural risks and limits

The active work list is maintained in `BACKLOG.md`. Major areas are:

- OS Detector receive ownership and shared RX-buffer stability;
- PCAP structure and generic packet-parser safety;
- network tuple, connectivity-cache, and shared-target consistency;
- editor confirmation/cancellation and persistence robustness;
- large contiguous allocations, stack headroom, and guard coverage;
- unresolved GUI, storage, IRQ, and frame-ingress platform contracts.

Bounded capacities such as 32 Passive records, 2,000 indexed PCAP packets,
eight RX registry slots, 255 Scan Hosts results, and 1,518-byte frame buffers
are product/resource limits rather than defects by themselves.
