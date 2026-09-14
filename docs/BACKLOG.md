# FLIPPER ETHERNET — Current Backlog

This file is the canonical list of current unresolved work. Current source is
the implementation ground truth, [`APP_BEHAVIOR.md`](APP_BEHAVIOR.md) defines
intended user-visible behavior, and [`ARCHITECTURE.md`](ARCHITECTURE.md)
describes the implementation and invariants that changes must preserve.

Finding IDs beginning with `F4-` are stable audit identifiers. Their priority
is audit priority based on reachability, consequence, and architectural scope;
it is not an approved implementation order or roadmap.

Historical plans and specifications are not automatically active backlog.

## 1. Active current work

### RX and concurrency

#### F4-001 — OS Detector directly competes with RX Dispatch — P0

OS Detector's active TCP probe loop calls `receive_packet()` while the
long-lived dispatcher also drains the same ENC queue. OS has no temporary
dispatcher registration for those replies and does not pause Dispatch.
Frames can be consumed by the wrong context.

#### F4-002 — OS Detector and RX Dispatch share the RX buffer — P0

Both receive paths write `ethernet->rx_buffer`. The ENC mutex serializes an
individual receive but not subsequent parsing, so an overlapping receive can
replace the frame while the other context parses it.

#### F4-003 — ARP reply construction uses concurrent mutable globals — P1

Automatic ARP handling and the OS direct-RX ARP path can interleave updates to
`my_mac`, `my_ip`, `mac_dest`, and `IP_SRC_REQUESTED`, producing a reply built
from mixed state. Its harmful active overlap depends on the OS RX exception.

#### F4-004 — Dispatcher pause is not a guaranteed quiescence barrier — P1

`rx_dispatch_pause()` performs a bounded acknowledgement wait and returns even
if acknowledgement was not observed. DHCP then begins direct RX. A collision
requires the explicit delayed-acknowledgement timing condition; none is claimed
to have been observed at runtime.

### PCAP, packet parsing, and protocol selection

#### F4-005 — PCAP analysis payload destination overflow — P0

The unknown-Ethernet analysis path can copy a 1,504-byte payload from a maximum
1,518-byte frame into a 1,500-byte stack array.

#### F4-006 — Generic packet analysis lacks complete length validation — P0

Generic Ethernet/ARP/IPv4/TCP/UDP analysis contains fixed-offset access,
unsigned length-subtraction, and trusted TCP data-offset paths that can read or
copy outside validated captured data. This does not apply equally to the more
strongly bounded DHCP, CDP, EAPOL, inner LLDP, or Passive History decoders.

#### F4-007 — PCAP records are indexed without complete structural validation — P1

The reader can commit an index entry before proving a complete packet header,
uses `orig_len` for traversal while record data is described by `incl_len`, and
does not fully validate global-header fields, length relationships, or remaining
file bytes. Malformed records can reach generic analysis.

#### F4-008 — Sniffer does not propagate capture-write failure — P1

The capture helper reports write failure, but the Sniffer handler ignores the
result and advances its displayed packet count.

#### F4-009 — Persisted protocol index can index past the UI table — P0

Settings accepts `scan_protocols_index` without bounds validation. The active
Ports UI has one protocol label (`TCP`) and indexes that table directly.

#### F4-010 / B-4 — UDP scan route is latent and incomplete — P3

The worker retains a UDP branch, but the active UI exposes only TCP. The UDP
predicate does not distinguish closed ports from no response. Treat this as
architectural debt in a currently dormant route, not as failure of the active
TCP UI.

### Network-state consistency

#### F4-011 — Manual IP can create a mixed network tuple — P1

Manual IP confirmation replaces the IPv4 address and acquisition flags while
retaining the previous subnet, gateway IP, and gateway MAC.

#### F4-012 — DHCP cancellation can split tuple and acquisition flags — P1

Cancellation after an ACK can occur after IP/subnet/gateway fields are copied
but before `is_dora` and `is_static_ip` are finalized. This is a timing-dependent
state transition.

#### F4-013 — Controller availability cache can become stale — P1

`enc28j60_connected` represents a prior controller-start result rather than
current PHY link or usable network state. Feature paths do not consistently
refresh or invalidate it.

#### F4-014 — Ping and Ports/OS use different target fields — P2

Ping uses `scan_params.ip_ping`; Ports and OS use `scan_params.target_ip`.
This contradicts the intended shared Target IP behavior.

### Input and persistence semantics

#### F4-015 — Custom IPv4 editor mutates state before confirmation — P2

UP/DOWN edits write directly into the bound App field and BACK has no rollback.
Affected paths include Scan Hosts start IP, Ping target, Ports target, OS
target, and Settings manual IP. Later application teardown can persist the
unconfirmed value.

#### F4-016 — Confirmation does not update settings storage immediately — P2

The user contract requires an explicit OK/Set/Save/Enter action to commit the
value and update `settings.cfg`. Current source commits RAM but normally writes
the file only during `app_free()`.

### Storage robustness

#### F4-017 — Settings save can destroy the last valid copy — P1

`settings.cfg` is opened with create-always and written sequentially. A partial
write failure can leave a truncated file and is not surfaced to the user.

#### F4-018 — Settings load is partial and incompletely range-validated — P2

Earlier fields can remain applied when later fields are absent or malformed,
and scan numeric fields are not all semantically validated. F4-009 is one
concrete downstream consequence.

#### F4-019 — Short last-scan body can expose incomplete results — P1

`last_scan.bin` accepts timestamp/count before proving the full result body was
read. The one-byte count cannot exceed the 255-entry App array, but accepted
entries can still contain stale or zero data.

#### F4-020 — Last-scan rewrite does not validate all writes — P1

The create-always writer can replace the last valid scan with an incomplete
file after an I/O failure.

#### F4-021 — Cancelled Scan Hosts state can remain visible — P2

Cancellation leaves partial RAM results. If restoration from the prior scan
file fails, later host-selection screens can expose those partial results.

#### F4-022 — Last-scan format is native and unversioned — P3

The format depends on native layout and has no explicit compatibility version.
This is architectural portability debt, not an array-overflow finding.

#### F4-023 — Passive history merge failure is ignored — P2

Normal Passive scene cleanup invokes the merge but ignores failure, so new
observations may be lost silently when the live database is released.

### User-visible behavior

#### F4-025 — BACK stops active ARP Spoof All — P2

Current BACK handling stops the active operation, while the intended behavior
reserves stop for the explicit center-button STOP action.

### Memory, resource, and startup-guard work

#### F4-026 — Large contiguous and dynamic resource demands — P2

The PCAP reader requires a 16,000-byte contiguous index payload and Passive
requires an approximately 15.5 KiB contiguous neighbor database. Ports result
menus and SDK-backed GUI/storage/string objects add data-dependent demand.
These are resource constraints, not memory leaks.

#### F4-027 — Stack headroom requires runtime high-water validation — P1

The 4 KiB RX Dispatch stack has substantial automatic-ICMP call-stack demand;
the 5 KiB OS worker and 4 KiB PCAP reader also have large nested locals. Source
does not prove a stack overflow.

#### F4-028 — Startup guards are incomplete preflight models — P2

Guard formulas depend partly on SDK allocator/thread metadata assumptions and
do not include every later GUI, storage, string, scanner, or automatic-handler
allocation. Passing a guard does not reserve memory.

### OS Detector concerns awaiting bounded classification

The following B-9 source patterns remain visible but do not yet have formal
`F4-` classifications:

- multiple replies in an attempt can overwrite sample arrays indexed by the
  attempt number rather than a response counter;
- a port can be marked `PORT_OPEN` before the received ACK is validated.

The earlier B-9 claim that the UI immediately joins and blocks after starting
the worker is closed; the current scene starts asynchronously and joins on its
completion/exit paths.

## 2. Conditional and external-contract questions

These items must remain conditional until the named platform contract is
established.

| ID | Question |
|---|---|
| F4-024 | Does normal top-level SceneManager/ViewDispatcher shutdown invoke Passive `on_exit()` before `app_free()` fallback cleanup? |
| F4-029 | Are direct worker GUI/string mutations, custom-event publication/ordering, `volatile` stop flags, and physical BACK polling sufficient under the current SDK/runtime contracts? |
| F4-030 | Does ByteInput mutate the supplied MAC buffer before SAVE, and what does BACK guarantee? |
| F4-031 | When are FileBrowser worker callbacks guaranteed quiescent during scene teardown? |
| F4-032 | Does GPIO callback removal quiesce an in-flight ISR, and must controller RX/EIE be explicitly disabled before wrapper teardown? |
| F4-033 | What atomic replacement/crash-consistency guarantees apply when Passive History renames its temporary file? |
| F4-034 | Can the active ENC/filter/driver path deliver a short/runt frame to application wrappers that inspect fixed Ethernet offsets? |

The allocator-dependent portion of F4-028 also remains conditional on the
current SDK allocator and thread-object contracts.

## 3. Product and resource limits

The following are bounded current capabilities, not defects by themselves:

- 32 Passive live/history records;
- 2,000 PCAP index entries;
- eight RX registry slots, normally including two permanent handlers;
- 255 Scan Hosts result entries;
- 1,518-byte application frame buffers;
- one application-owned feature worker at a time;
- a 16,000-byte contiguous PCAP index design;
- an approximately 15.5 KiB contiguous Passive DB design.

## 4. Audit priority summary

Current P0 findings are:

- F4-001 — OS dual RX ownership;
- F4-002 — shared RX-buffer overwrite;
- F4-005 — PCAP analysis stack overwrite;
- F4-006 — generic parser memory-safety boundaries;
- F4-009 — persisted protocol-index out-of-bounds access.

This priority identifies high-confidence safety/correctness blockers. It is not
an approved fix order and does not authorize implementation.

## 5. Closed and implemented history

| ID | Closed work | Current contract |
|---|---|---|
| B-1 | DHCP offer timeout increased | Current DHCP wait accommodates the earlier server collision-check delay |
| B-2 | Sniffer link-wait busy loop removed | Current capture runs in a feature worker |
| B-3 | Pre-capture worker-suspension dead window removed | RX Dispatch remains available during setup |
| B-5 | ARP-scan loading freeze tied to old polling | Scanner waits use registered RX/semaphore flow |
| B-6 | File-static ENC bank race | Bank state is per-instance and public chip operations use the ENC mutex |
| B-7 (primary) | Scanner next-hop direct-RX path | `scanner_resolve_next_hop()` uses registered receive waiting; remaining legacy callers are disabled or currently uncalled |
| B-8 | DORA-only `app_worker.c` stub | File/thread removed; Get IP owns its feature worker |
| B-9 (UI portion) | OS scene immediately blocking on worker join | Current scene starts asynchronously and joins on finish/exit |

## 6. Findings not supported as current defects

Do not re-add the following without new current-source evidence:

- a general dangling feature-worker pointer or worker surviving `app_free()`;
- normal Passive or Sniffer filter poisoning;
- a foreground/automatic-responder shared-TX-buffer race;
- scanner callback-context use-after-free after unregister;
- an active source-proven memory leak;
- persistent allocator fragmentation;
- `last_scan.bin` count overflowing the 255-entry App array;
- corruption on every ordinary DHCP failure;
- a claim that all protocol parsers are equally unsafe.

## 7. Documentation item resolved

F4-035 is resolved at the documentation level: current Saved Neighbor History
uses `apps_data/ethernet/passive_history.bin`, with
`passive_history.tmp` used during replacement. The behavioral contract is
history persistence until the user clears it, rather than a required filename.
