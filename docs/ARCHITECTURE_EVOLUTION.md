# FLIPPER ETHERNET — Architecture Evolution

## 1. Purpose and authority

This document explains how the current FLIPPER ETHERNET architecture emerged. It is a durable lineage guide: use it to understand why present subsystems and invariants exist, how historical plans map to current features, and which older implementation patterns have been replaced.

It is not the current architecture specification, behavioral specification, backlog, or future roadmap. Use the repository sources according to these roles:

- Current implementation truth: `EthernetAppDemo/` source and `application.fam`.
- Intended user-visible behavior: [APP_BEHAVIOR.md](APP_BEHAVIOR.md).
- Current software structure: [ARCHITECTURE.md](ARCHITECTURE.md).
- Current controller and board model: [HARDWARE.md](HARDWARE.md).
- Accepted project decisions: [DECISIONS.md](DECISIONS.md).
- Current unresolved work: [BACKLOG.md](BACKLOG.md).
- Historical design lineage: [ENC28J60_REFACTOR_PLAN.md](../ENC28J60_REFACTOR_PLAN.md), `docs/superpowers/specs/`, `docs/superpowers/plans/`, and Git history.

Historical plans preserve intent and the constraints understood when they were written. They do not prove that current source still follows a planned design. Likewise, an unchecked historical checklist item is not automatically pending, and a similarly named current module is not automatically proof that the original capability was completed.

For any historical claim, compare the claim with current source first, then reconcile it with the current canonical documents. Classify the result as implemented, partially implemented, superseded, obsolete, historical/not currently adopted, or current candidate. Use current candidate only when current source, canonical documentation, backlog/decisions, or an explicit current project decision supports that status.

## 2. Documentary boundary and architectural eras

The repository has three documentary layers:

1. **Current canonical material** describes current behavior, architecture, hardware, decisions, and unresolved work. It should be concise and maintained with source changes.
2. **This evolution record** preserves transitions, their motivations, and the relationships between historical intent and current structure.
3. **Historical plans and specifications** preserve the design vocabulary and implementation context of their period. They remain read-only evidence, including where later source diverged.

The major architectural eras were:

| Era | Characteristic state | Durable outcome |
|---|---|---|
| Early application | File-static feature parameters, a continuously polling `app_worker`, direct receive loops, ad hoc feature threads, and largely volatile state | Established the scene-based Ethernet toolkit and its protocol implementations |
| F0 state and persistence centralization | `scan_params_t`, settings persistence, and a reusable scanner primitive | Shared feature inputs acquired explicit application lifetime and storage boundaries |
| F0 receive and controller hardening | RX Dispatch, automatic handler migration, scanner subscriptions, ENC mutex ownership, and PA14 interrupt wakeup | One primary receive path and serialized controller access replaced competing polling as the normal design |
| Lifecycle hardening | DORA moved to Get IP, the old background worker was removed, and feature-thread ownership/cancellation was made explicit | Feature work became scene-bounded under one app-owned worker slot |
| Passive Discovery growth | LLDP foundation, then CDP and EAPOL integration, shared identity, detail views, and Saved Neighbor History | Three historical roadmap capabilities became one integrated Passive family |
| Persistence and resource hardening | Last-scan history, PCAP lifecycle changes, dynamic large allocations, startup guards, and teardown work | Large resources became feature-scoped and storage artifacts gained explicit consumers |
| Audited canonical state | Source audits reconciled behavior, architecture, hardware, decisions, and backlog | Current truth is separated from historical intent and unresolved external contracts |

Older revisions of `ARCHITECTURE.md`, `BACKLOG.md`, and `SKILL.md` contain several different kinds of material. Fixed branch/HEAD/tag statements, a 14-scene inventory, settings schema v2 as current, and unverified free-heap estimates were stale snapshots. Full field inventories and finding lists duplicated material now owned by canonical documents. Commit-by-commit notes and old phase status remain historical evidence. The durable material retained here is the reasoned transition between designs.

The pre-canonical revision `26bd031` is a useful documentary boundary rather than the beginning of the architecture. Comparing it with the reconciled documents yields this preservation rule:

| Boundary material | Classification | Treatment here |
|---|---|---|
| Fixed branch, HEAD, tag, phase, release, old scene-count, schema-v2-current, and free-heap assertions | Stale snapshot | Not restored |
| Current scene/resource descriptions and complete F4 lists | Current-state duplication | Left with `ARCHITECTURE.md` and `BACKLOG.md` |
| Field-coverage tables, per-commit task logs, and debugging transcripts | Historical implementation detail | Used as evidence, not reproduced |
| F0 state/RX/controller/worker transitions and Passive/PCAP/resource evolution | Durable architectural lineage | Condensed into the transition sections below |
| Dual-build, LAN expansion, OT/ICS, drop-box, and broader protocol/analyzer aspirations | Historical roadmap / future intent | Preserved as context without adoption or placement decisions |

## 3. Application state and settings evolution

### Historical intent

F0.1 aimed to replace feature-local scan targets with one `App.scan_params` structure. F0.2 then aimed to persist the application MAC, network configuration, and scanner inputs through `settings.cfg` using Flipper Format.

### What landed and how it evolved

Commit `ca8887e` introduced `scan_params_t` and migrated feature parameters into `App`. The result was not a single semantic target: current source retains separate fields such as `ip_ping` and `target_ip`, because features edit and consume different values. This is an important divergence from the user-level shared-target intent recorded in `APP_BEHAVIOR.md`, and the current consequence belongs in `BACKLOG.md`, not in the old F0.1 checklist.

Commits `0cd7345` and `8814602` introduced and wired settings load/save. The format later evolved; current `libraries/settings/settings.c` writes schema version 3 and accepts versions 1 through 3. Current source saves from application teardown, while `APP_BEHAVIOR.md` owns the intended distinction between confirmation and BACK semantics.

### Current significance

`App` is the long-lived state boundary for shared network configuration, scan inputs, paths, views, history handles, controller access, and the feature-worker slot. Settings are one persistence channel; scan results, Passive history, and captures use separate formats and lifetimes.

## 4. Receive architecture evolution

### From polling to a dispatcher

The early `app_worker.c` loop polled `receive_packet()` approximately every millisecond and performed automatic ARP and ICMP responses. Feature code also used direct receive loops. This made receive ownership implicit: multiple contexts could compete for the controller FIFO, and scenes suspended/resumed a background thread to create temporary exclusivity.

The F0.4 design introduced `libraries/chip/rx_dispatch.*` as a single primary chip-RX loop with a handler registry. Commit `ecc17b2` added the dispatcher; `0656422` moved automatic ARP and ICMP handling into permanent registrations. Scanner packet waits subsequently moved to temporary dispatcher registrations and semaphores (`76d5e79`). Race hardening in `11bf048` established the register-before-trigger ordering and strengthened registration/dispatch synchronization.

Commit `410c092` changed the dispatcher from tight polling to PA14/ENC INT wakeup with a bounded polling fallback. This improved the wakeup mechanism without changing the ownership rule: the dispatcher remains the normal receiver.

### Current architecture and remaining exception

RX Dispatch is primary, not universal. Automatic ARP/ICMP, scanner waits, Passive Discovery, and Sniffer use dispatcher registration. Current OS detection still performs direct `receive_packet()` calls. DHCP is also a direct receiver while Get IP places the dispatcher behind `rx_dispatch_pause()`/`rx_dispatch_resume()`; the pause is bounded rather than an acknowledged quiescence handshake, so it does not prove that an in-flight receive has completed. The `DEV_MODE`-only ARP-specific-spoofing path can reach the legacy direct-receive `arp_get_specific_mac()`, while direct-receive TCP handshake/old OS helpers remain compiled but have no current caller found. These limitations and reachability distinctions are tracked by the relevant F4 items in `BACKLOG.md`.

The architectural lesson is not “direct RX never existed” or “the migration is complete.” It is: new receive consumers should begin with dispatcher registration, and any direct-RX exception must explicitly prove FIFO exclusivity, dispatcher quiescence, cancellation behavior, and interaction with permanent responders.

## 5. ENC28J60, SPI, GPIO, and IRQ evolution

### Historical controller model

Early controller code relied on shared file-static receive/bank state and did not serialize every register transaction. The F0 roadmap proposed bulk SPI, interrupt-driven receive, and a revised SRAM layout as one group, but these goals did not land as one indivisible design.

### Implemented transition

Commit `abf9790` introduced a per-controller `FuriMutex`; follow-up `9d45f31` closed missing synchronization coverage across allocation/reset/MAC/start/link paths. Receive cursor state is now in `enc28j60_t.rx_next_packet`; RX and TX frame
buffers are instance members. The earlier process-global bank cache was replaced by the per-instance `enc28j60_t.bank` cache. Bank-switch logic compares against that instance state, consults the controller bank state when a switch is needed,
and updates the per-instance cache under the ENC synchronization boundary. Later lifecycle work (`dc03227`, `f1e7e14`) hardened controller and shutdown state.

PA14 interrupt registration became the dispatcher wakeup source in `410c092`, with fallback polling retained. The bulk-SPI roadmap idea was not adopted because the ENC28J60 command protocol and chip-select transaction boundaries did not support the proposed blanket conversion without changing semantics.

The current controller SRAM layout is 3 KiB RX (`0x0000`–`0x0BFF`) and 1.5 KiB TX (`0x0C00`–`0x11FF`), as documented in `HARDWARE.md`. Do not revive older 6/2 KiB descriptions as current facts.

### Boundary that remains external

Repository source proves callback registration/removal order and intended teardown, but some IRQ/HAL guarantees remain external contracts. The evolution does not convert those unknowns into either confirmed safety or confirmed defects.

## 6. Scanner-session evolution

### Original primitive

The F0.3a plan introduced `scanner_session_t` as a stack-owned, borrowed-state helper. Its first purpose was subnet-aware next-hop resolution with a small cache; Ping was the proof-of-concept. The planned `scanner_wait_for_packet()` initially existed before it had a dispatcher-backed implementation.

### Migration and present role

Scanner users were migrated incrementally rather than atomically. Once RX Dispatch existed, packet waits acquired a temporary handler registration plus semaphore and enforced registration before the triggering transmit. Current scanner-session users include Ping, ARP scanning/spoof support, TCP/UDP scanning, OS support code, and Passive infrastructure. Not every consumer uses every scanner operation, and OS detection still contains a direct receive path.

The durable boundary is that a scanner session borrows `App`/ENC/network state, owns its short-lived wait synchronization when used, caches next-hop resolution for its session lifetime, and must unregister before that lifetime ends. It is a shared backend primitive, not a user-visible feature and not proof that all scanner flows have identical RX behavior.

## 7. Worker ownership and cancellation evolution

### Earlier pattern

The original application combined a long-lived `app_worker` with feature-created `thread_alternative` workers. Scenes commonly suspended the background thread, allocated a replacement, and later resumed the original. Pointer presence served as an informal ownership signal, and cancellation contracts varied by feature.

### Transition

The RX dispatcher first stripped receive responsibility from `app_worker`. DORA remained temporarily as a service stub, exactly as the F0.4a plan described. Commit `9821945` then moved DORA into the Get IP scene worker and deleted `app_worker.c`.

Later fixes hardened worker termination and scene exits across scanners, DHCP, ports, controller teardown, and other features. Commit `17ddcc6` established explicit `AppThreadOwner` claims around the single `App.thread_alternative` slot. Current helpers—`app_thread_claim()`, `app_thread_is_owned()`, `app_thread_join_and_free()`, and `app_thread_shutdown()`—make ownership visible and centralize join/free behavior.

### Current consequence

Only one app-owned feature worker is intended to occupy the slot at a time. This makes large feature stacks mutually exclusive, but it does not automatically make every callback or SDK-owned worker part of the same ownership scheme. FileBrowser, dispatcher, GUI, GPIO callbacks, and protocol handlers have separate execution/lifetime contracts.

## 8. Passive Discovery evolution

Passive Discovery is the clearest example of historical roadmap structure diverging from the current feature structure.

### 8.1 LLDP foundation

The historical roadmap listed LLDP harvest as F1.1. Commit `bb565e7` introduced the LLDP parser and a shared neighbor database foundation. Early identity was primarily MAC-oriented. Subsequent work added the Passive scenes, live worker, detail views, VLAN/PoE fields, and multicast reception.

### 8.2 CDP consolidation

The roadmap listed CDP separately as F1.2. It did not become an independent top-level worker and database. Commit `033b1d7` integrated CDP into the same Passive frame path, protocol-selection UI, and database, while making identity source-aware so the same MAC could retain distinct protocol observations.

### 8.3 EAPOL consolidation

The roadmap listed 802.1X EAPOL reconnaissance separately as F1.3. Commit `8fc6f9d` added EAPOL parsing and identity persistence to the same Passive runtime. It did not add another controller owner or protocol-owned receive loop.

### 8.4 Current integrated model

Current `passive_discovery_module.c` has one worker, one temporary RX registration, one live neighbor database, and a protocol-handler table for LLDP, CDP, and EAPOL. “Discover All” sends each received frame through the applicable predicates/handlers; protocol-specific modes filter the same shared infrastructure. `neighbor_db_find_by_source()` and the `(source MAC, protocol)` identity preserve independent observations.

The live database became feature-scoped rather than always resident (`1cadad2`), and is bounded at 32 neighbors. Saved Neighbor History (`7e066d7`) later added offline browsing, merge, filtering, and clearing through a separate persistent representation.

### 8.5 Architectural meaning

LLDP, CDP, and EAPOL are implemented capabilities, but their original one-item-per-roadmap decomposition was superseded by one cohesive user-facing feature. Future roadmap reconciliation must evaluate the intended capability, not count scene names or unchecked F1 rows.

## 9. Receive-filter evolution

The roadmap proposed hardware pattern matching for individual passive protocols. Current hardware use is more compositional:

- The normal filter is unicast, valid CRC, pattern match, and broadcast (`UCEN | CRCEN | PMEN | BCEN`); the programmed pattern supports LLDP.
- Passive Discovery enables multicast (`MCEN`) for the feature lifetime, allowing CDP multicast and EAPOL multicast while retaining normal filtering behavior.
- Sniffer temporarily selects broad valid-CRC reception and restores the normal filter on exit.
- DHCP temporarily changes broadcast acceptance and restores the prior broadcast state.

This supersedes the idea that each Passive protocol requires a separate feature or independent hardware-pattern owner. Filter state is shared controller state, so the invariant is transition-and-restoration discipline, not a fixed filter value for every feature.

The previously suspected permanent filter-poisoning path was not supported after current source tracing: normal Passive, Sniffer, and DHCP exits contain restoration paths. That rejected hypothesis must not be revived without new current-source evidence. Questions about concurrent transition windows remain separate and belong in `BACKLOG.md` where applicable.

## 10. Persistence evolution

Persistent state arrived in separate waves and should not be treated as one monolithic database:

| Artifact | Evolution | Current role |
|---|---|---|
| `settings.cfg` | Added in F0.2; schema later advanced to v3 | Application/network/scan configuration; writer v3, loader v1–v3 |
| `last_scan.bin` | Added by `7d9cf8b` | Bounded ARP scan result history reused by Scan Hosts and target-oriented screens |
| `passive_history.bin` plus temporary replacement file | Added with Saved Neighbor History | Versioned/checksummed Passive observations keyed by MAC and protocol, with bounded merge/rewrite behavior |
| `files/*.pcap` | Capture path predated later lifecycle hardening | Sniffer output and View Packets input |

These stores have distinct formats, failure behavior, allocation patterns, and consumers. Current robustness questions are enumerated in `BACKLOG.md`; this document does not duplicate them. Historical references to `passive_discovery.bin` are not current-path authority.

## 11. Sniffer and PCAP evolution

Sniffer began as a direct receive loop with ad hoc filter and file handling. The RX migration made it a dispatcher consumer, and lifecycle fixes closed busy-loop and registration-window behavior recorded by the old backlog. Sniffer now owns a feature worker, temporary RX registration, capture file lifecycle, and temporary broad receive-filter state.

View Packets evolved separately as a FileBrowser plus PCAP indexing/reading flow. Commit `f3b79ff` stabilized navigation and file lifecycle. Commit `7a2a37a` moved the packet-offset index out of permanent `App` storage: the current 2,000-entry `uint64_t` index is a 16,000-byte contiguous allocation made only for the reader scene and released afterward.

This separation matters: Sniffer produces PCAP files but does not coexist with the PCAP-reader worker under the one-feature-worker rule. File selection is an SDK-owned interaction, while indexing and rendering use application-owned state. PCAP parsing and rendering defects remain current backlog concerns; historical stabilization commits are not proof that all malformed-input paths are safe.

## 12. Network-state evolution

Early code and plans often used “connected” as if add-on presence, physical link, configured IPv4 state, and DHCP completion were the same fact. Current analysis distinguishes at least four concepts:

1. ENC28J60/add-on availability.
2. PHY link state.
3. usable/configured network parameters.
4. acquisition/mode flags such as `is_dora` and `is_static_ip`.

Get IP owns active DHCP work; manual settings update application state; other features consume cached/shared values and perform different precondition checks. The distinctions are described in `HARDWARE.md` and `ARCHITECTURE.md`; source-visible consistency issues are tracked under the network-state F4 group in `BACKLOG.md`.

The durable lesson is to name the exact state being tested. A future feature should not reuse a generic “connected” flag without defining whether it needs hardware presence, link, an address, a gateway, or completed acquisition.

## 13. Memory and resource evolution

The architecture accumulated several always-resident costs: the 24 KiB FAP main stack, GUI objects, `App`, ENC instance buffers, controller mutex/SPI handle, and the RX dispatcher with its 4 KiB stack and registry. Feature workers add stacks from 3 KiB through 10 KiB.

Later work reduced unnecessary coexistence rather than proving a universal free-heap figure:

- The approximately 15.5 KiB Passive neighbor database moved from static residency to Passive-family lifetime.
- The 16 KiB PCAP offset index moved to PCAP-reader lifetime.
- The app-owned feature-worker slot makes feature stacks mutually exclusive.
- Startup guards were added for resource-sensitive scene entry.

The source audits found sensitivity to total free heap and largest contiguous block, but no source proof of an active leak or persistent allocator fragmentation. Old estimates of 120–150 KiB free heap are not current facts. Allocator metadata, SDK object sizes, FileBrowser internals, IRQ contracts, and true stack high-water marks require external SDK evidence or runtime validation.

## 14. Teardown and cross-feature evolution

Early scene transitions relied on local conventions: set a flag, suspend or resume a thread, unregister if remembered, and reset a reused view. Later hardening made the intended sequence more explicit:

1. signal feature cancellation;
2. unblock waits where the feature contract requires it;
3. join and free the correctly owned worker;
4. unregister temporary RX handlers;
5. restore feature-modified controller filters/state;
6. close files and free feature-scoped allocations;
7. reset or reuse GUI objects;
8. leave long-lived application state available to the next scene.

At application exit, dispatcher/handler, GPIO IRQ, controller/SPI, storage, GUI, and application allocations have a defined source order. Whether every external callback-removal or SDK record-close operation provides the required synchronization is not invented here; those contracts remain conditional backlog questions.

Cross-feature resource design now relies on release-before-next-feature behavior. Passive DB and PCAP index are not meant to coexist as active feature resources; Sniffer and PCAP reader run sequentially; completed ARP results and persistent files intentionally survive their producers and feed later consumers.

## 15. Invariants that emerged and why

These are current invariants because they address architectural failure modes exposed during the evolution. Their precise implementation remains owned by `ARCHITECTURE.md`, `HARDWARE.md`, and source.

| Invariant | Why it emerged |
|---|---|
| One app-owned feature worker at a time | Replaced pointer-only ownership and inconsistent scene-thread replacement |
| Join/free only through the matching `AppThreadOwner` | Prevents one scene from consuming another scene's worker lifecycle |
| RX Dispatch is the primary FIFO reader | Replaced competing polling receive loops |
| Temporary RX registration must outlive every possible callback | Handlers borrow feature/session context |
| Register before the triggering transmit | Prevents fast responses from arriving in a subscription gap |
| Direct RX requires explicit dispatcher exclusion | A chip FIFO cannot safely have implicit competing consumers |
| ENC register/bank operations use the instance mutex | Replaced unsynchronized shared controller state |
| Filter mutations are feature-scoped and restored | Multiple features reuse one hardware filter register |
| Large feature allocations are scoped and mutually exclusive where designed | Protects limited heap and contiguous-block availability |
| Persistence formats have explicit version/identity boundaries | Settings, scans, Passive records, and captures evolve independently |
| Teardown stops producers before freeing borrowed resources | Threads, callbacks, registrations, and GUI events may outlive local control flow |
| SDK/HAL behavior stays unresolved until verified | Repository source cannot prove external allocator, callback, or record contracts |

## 16. Superseded implementation patterns

| Earlier pattern or plan | Current replacement | Status |
|---|---|---|
| Feature-local/file-static scan targets | `App.scan_params` fields | Implemented; shared-target semantics remain imperfect |
| No persistent settings | Versioned `settings.cfg` | Implemented and evolved to writer v3/loader v1–v3 |
| One polling `app_worker` owns automatic RX | RX dispatcher plus registered automatic handlers | Superseded |
| Suspend/resume `app_worker` around feature threads | One explicitly owned `thread_alternative` slot | Superseded |
| Scanner helpers poll the controller directly | Dispatcher-backed temporary registrations/semaphore waits | Mostly superseded; OS has a direct-RX exception |
| Mutable file-static ENC bank/receive cursor state | Per-instance bank/RX state plus controller mutex | Superseded |
| Tight RX polling | PA14 interrupt wakeup with bounded polling fallback | Superseded |
| Blanket bulk-SPI conversion | Command-appropriate SPI transactions | Historical proposal not adopted |
| Separate LLDP, CDP, and EAPOL user features | Integrated Passive Discovery modes and shared data model | Superseded feature decomposition |
| Protocol identity by MAC alone | Source/protocol-aware `(MAC, protocol)` identity | Superseded |
| Always-resident Passive database | Passive-family-lifetime allocation | Superseded |
| Always-resident PCAP offset array | Reader-scoped 16 KiB allocation | Superseded |
| Dual Admin/Pentest artifacts | One FAP compiled with `PENTEST_MODE=1` | Historical build experiment superseded |

## 17. Surviving architectural debt

The current backlog, not this lineage record, owns severity and resolution state. The following debt is mentioned only to connect it to the architecture that produced it:

- RX/concurrency (`F4-001`–`F4-004`): dispatcher callback lifetime, OS direct receive, and bounded pause behavior remain the main exceptions to the intended receive boundary.
- PCAP/parsing (`F4-005`–`F4-010`): file parsing, indexing, and rendering evolved incrementally and retain source-proven correctness/robustness work.
- Network state (`F4-011`–`F4-014`): historical conflation of presence, link, addressing, and acquisition still affects feature preconditions and state refresh.
- Input/persistence semantics (`F4-015`–`F4-016`) and storage robustness (`F4-017`–`F4-023`): persistent state was added in layers, while confirmation and malformed-file contracts require further work.
- Memory/resource guards (`F4-026`–`F4-028`): feature scoping improved peak demand, but guard formulas and later SDK allocations do not form a complete allocator contract.
- External contracts (`F4-024`, `F4-029`–`F4-034`): Passive replacement durability, SDK allocation behavior, record/thread/GPIO semantics, and related guarantees cannot be settled from application source alone.

The source-visible OS sample-index and premature `PORT_OPEN` observations are useful future audit targets but were not assigned F4 finding identifiers. They should not be silently promoted into formal findings by this document.

## 18. Rejected or narrowed historical hypotheses

Historical debugging notes are hypotheses until current source proves them. The audit history narrowed or rejected several recurring claims:

- **Permanent ENC filter poisoning:** not supported by current normal Passive, Sniffer, and DHCP restoration paths. Concurrent transition questions are distinct.
- **A general dangling feature worker:** not supported across the current owner-tagged feature paths as a blanket explanation; individual cancellation paths must still be audited on their own evidence.
- **An app-owned feature worker surviving `app_free()`:** not supported by the current `app_thread_shutdown()`-before-resource-release ordering.
- **A foreground/automatic-responder TX-buffer race:** not supported by current responder construction, which uses callback-local response storage rather than borrowing the feature's shared TX frame.
- **A scanner callback use-after-free after normal unregister:** not supported as the earlier broad hypothesis; current registration locking and teardown ordering narrow the remaining concern to specifically documented callback/quiescence boundaries.
- **Active application memory leak:** not proven from current source ownership paths.
- **Persistent heap fragmentation as a root cause:** not proven; current source establishes sensitivity to live allocation layout and contiguous blocks only.
- **Feature stack overflow:** not proven from configured sizes and source-local objects; high-water validation remains the appropriate evidence boundary.
- **All RX consumers use RX Dispatch:** false; OS detection remains a direct-RX exception.
- **Scanner-session migration is all-or-nothing complete:** too broad; the primitive is widely used, but receive behavior varies by operation and OS retains direct receive.
- **The firmware SDK is deterministically pinned:** false for current CI; the Unleashed `release` selection is floating.
- **Two distributable FAP variants are current:** false; current `application.fam` describes one FAP with `PENTEST_MODE=1`.
- **Historical 6/2 KiB ENC SRAM partition is current:** false; current code/documentation use 3 KiB RX and 1.5 KiB TX.

Rejection is evidence-relative, not permanent dogma. A claim may be reopened only with new current-source, authoritative SDK/HAL, or reproducible runtime evidence.

## 19. Historical roadmap intent that is not current architecture

The master plan and roadmap specification describe a broader product direction: mDNS/DHCP tracking, expanded scan modes, topology/export features, IPv6 responders, MITM and stress capabilities, OT/ICS work, drop-box operation, and dual Admin/Pentest distribution. Those documents remain valuable for the problem statements and dependencies they captured.

Except where current source, canonical documentation, or an explicit current project decision says otherwise, these remain historical roadmap items that are not currently adopted. Historical presence alone does not make them current candidates or commitments. Items explicitly re-adopted by current project evidence may be treated as current candidates. This report makes no placement decision about whether a future capability should extend Passive Discovery, Sniffer, a scanner backend, an existing scene, or a new user-visible feature. It also does not adopt the old F1/F2/F3 ordering. The present build has 24 active scenes plus two `DEV_MODE`-only ARP Spoofing Specific IP scenes; that inventory describes the current UI, not the size or placement of a future roadmap.

Before proposing roadmap work, determine:

1. the capability the historical item intended;
2. whether current source already implements it wholly or partly;
3. whether a current feature superseded its old decomposition;
4. the closest current user-facing and backend owners;
5. RX ownership, worker, filter, persistent-state, memory, and teardown consequences;
6. dependencies on active `BACKLOG.md` items and current invariants.

## 20. Subsystem evolution matrix

“Primary evidence” names representative source paths or commits, not an exhaustive history.

| Subsystem / capability | Historical model | Motivation / problem | Major transition | Current architecture | Emerged invariant | Remaining debt / exception | Primary evidence |
|---|---|---|---|---|---|---|---|
| App configuration ownership | Feature-local/file-static inputs | Remove collisions and make cross-scene state explicit | Centralized in `App.scan_params` | Separate feature fields shared through `App` | `App` owns application-lifetime mutable configuration | Ping and general target semantics remain split | `ca8887e`; `app_user.h` |
| Settings | Volatile defaults | Preserve MAC, network, and scanner inputs | F0.2 load/save, then schema additions | Writer v3; loader v1–v3; application-lifecycle save | Version and validate each persisted schema | Confirmation/BACK semantics diverge from intended behavior | `0cd7345`, `8814602`; `libraries/settings/settings.c` |
| Scanner session | Repeated ARP resolution and polling waits | Share next-hop resolution, cache, cancellation, and wait logic | Ping POC, incremental migrations, dispatcher waits | Borrowed session with cache and temporary RX wait resources | Register before trigger; unregister before session context expires | Use does not imply every feature RX path is dispatched | `581ba2a`, `76d5e79`; `libraries/scanner/` |
| RX architecture | Background and feature loops compete for FIFO | Establish one normal receive owner | Dispatcher plus handler registry | Dispatcher-primary frame delivery | New ordinary consumers register with dispatcher | OS and DHCP remain direct-RX exceptions; pause is bounded | `ecc17b2`; `libraries/chip/rx_dispatch.c`; F4-001–004 |
| ENC/SPI ownership | File-static bank/cursor state and unguarded access | Prevent cross-context controller-state races | Instance mutex/state hardening | Shared ENC instance with serialized public operations | Controller/bank/RX state belongs to the instance synchronization boundary | Some HAL/SPI teardown guarantees are external | `abf9790`, `9d45f31`, `dc03227`; `libraries/chip/enc28j60.*` |
| Worker ownership | Background worker plus ad hoc alternative workers | Make scene cancellation and cleanup deterministic | Remove `app_worker`; add owner-tagged worker slot | One claimed app-owned feature worker | Matching owner joins before borrowed state is released | SDK workers and dispatcher have separate contracts | `9821945`, `17ddcc6`; `app_user.c` |
| Automatic responders | ARP/ICMP replies in polling worker | Keep background replies after removing that receiver | Permanent dispatcher handlers | Dispatcher callbacks parse and transmit replies | Callback inputs/resources stay valid through handler lifetime | Callback/teardown contracts require exact quiescence analysis | `0656422`; `app_user.c` |
| Passive Discovery | Separate protocol roadmap items | Share capture, UI, database, and filter lifetime | LLDP base grew into multi-protocol orchestration | One Passive family with modes and Discover All | Passive protocols do not own independent workers or filters | Shared callback/filter boundaries retain F4 dependencies | `passive_discovery_module.c`; `PassiveDiscoveryScene.c` |
| LLDP | Standalone F1.1 capability | Harvest switch identity/topology | Parser, DB, worker, and views landed | Passive protocol mode and shared DB | Protocol handler writes protocol-keyed records | Hardware pattern is LLDP-specific, not a generic EtherType engine | `bb565e7`; `modules/lldp_module.c` |
| CDP | Standalone F1.2 capability | Add Cisco discovery observations | Added to one-frame Passive dispatch | Passive mode with source-aware records | A MAC may retain an independent CDP observation | Admission relies on feature multicast state | `033b1d7`; `modules/cdp_module.c` |
| EAPOL | Standalone F1.3 capability | Expose 802.1X/EAP identity | Added without protocol-owned RX | Passive mode with identity persisted in shared model | Protocol-specific detail coexists behind common identity/storage APIs | Admission combines normal unicast and Passive multicast behavior | `8fc6f9d`; `modules/eapol_module.c` |
| Passive History | Live observations only | Retain and browse normalized neighbors offline | Versioned bounded history added | Separate history object/file merged from live DB | Persistence identity is MAC plus protocol | Atomic-replacement durability depends on external storage contract | `7e066d7`; `passive_history.*`; F4-024 |
| Receive filters | Per-protocol pattern proposals | Admit discovery/capture traffic without permanent broad receive | Shared multicast/promiscuous/broadcast transitions | Normal baseline plus feature-scoped mutations | Restore controller filter state at feature exit | Transition coexistence must respect shared hardware state | `enc28j60.c`; `HARDWARE.md` |
| Sniffer | Direct polling capture | Integrate capture with shared RX and lifecycle | Dispatcher subscription and lifecycle fixes | Worker, temporary handler, broad filter, PCAP writer | Registration/file/filter lifetimes are one feature transaction | Capture output still feeds parser safety debt | `SnifferScene.c`; `capture_module.c` |
| PCAP reader | Fragile navigation and resident indexing | Bound reader state and recover memory outside the scene | File fixes; index made on demand | FileBrowser then reader-scoped 16 KiB offset index | Index/file/string ownership ends with reader scene | Parser/index bounds and malformed input remain active debt | `f3b79ff`, `7a2a37a`; `ReadPcapsScene.c` |
| Network state | “Connected” conflates several facts | Make feature preconditions and acquisition state explicit | State spread across ENC, App, and scan configuration | Presence, PHY link, network config, and acquisition are distinct | Test the exact network condition a feature requires | Refresh/cached-state consistency remains F4-011–014 | `app_user.h`; `GetIPScene.c`; `HARDWARE.md` |
| Memory/startup guards | Generic free-heap assumptions | Account for large stacks/blocks and feature coexistence | Feature scoping and preflight guards | Total-free and largest-block checks before selected starts | Do not sum mutually exclusive workers; distinguish total from contiguous need | Guard formulas omit some later/SDK allocations | `startup_guard.c`; F4-026–028 |
| Teardown/relaunch | Scene-local cleanup conventions | Prevent producers/callbacks from outliving borrowed resources | Central joins, unregisters, filter/file cleanup, ordered app free | Explicit feature and application teardown boundaries | Stop and quiesce producers before freeing consumers/state | GPIO/SDK record/callback guarantees remain external | `app_thread_shutdown()`; `app_free()`; F4-029–034 |
| Build model | Planned dual Admin/Pentest artifacts | Separate capability profiles and distribution | Dual-build experiment, then single-build stabilization | One FAP, `PENTEST_MODE=1`, 24 KiB main stack | Build claims follow `application.fam`, not old plans | Unleashed `release` CI channel floats | `67d0a9d`, `4029913`, `41d6f03`; `application.fam` |

## 21. Lineage diagram

```text
Historical baseline
  file-static targets + polling app_worker + direct feature RX
          |
          +--> F0.1 shared App.scan_params
          |       `--> F0.2 versioned settings persistence
          |
          +--> F0.3 scanner_session
          |       `--> cached next-hop resolution
          |       `--> temporary dispatcher-backed packet waits
          |
          +--> F0.4 RX Dispatch
          |       +--> permanent ARP/ICMP handlers
          |       +--> scanner / Passive / Sniffer registrations
          |       `--> OS direct-RX exception remains
          |
          +--> F0.5-era controller hardening
          |       +--> ENC instance mutex/state
          |       +--> PA14 IRQ wake + polling fallback
          |       `--> feature-scoped filter transitions
          |
          +--> worker/lifecycle hardening
          |       +--> DORA moves to Get IP; app_worker removed
          |       `--> AppThreadOwner + centralized join/free
          |
          +--> Passive lineage
          |       LLDP foundation
          |          `--> CDP + EAPOL shared dispatch/data model
          |                 `--> Discover All + source-aware identity
          |                        `--> Saved Neighbor History
          |
          +--> storage/resource hardening
          |       +--> last_scan.bin
          |       +--> PCAP lifecycle + on-demand 16 KiB index
          |       +--> Passive-family-lifetime database
          |       `--> startup/resource guards
          |
          `--> current audited architecture
                  dispatcher-primary RX, one app-owned feature worker,
                  serialized ENC access, explicit persistence/resource
                  boundaries, and documented unresolved contracts
```

This lineage explains the current shape; it does not freeze it. When source changes an architectural boundary, update the current canonical owner first, then update this document only if the reasoned historical relationship also changed.
