# FLIPPER ETHERNET — Ponytail Complexity Audit

## 1. Purpose and authority

This document is a static, current-source-based audit of complexity in the FLIPPER ETHERNET application. It distinguishes complexity that protects correctness, ownership, hardware, lifetime, or resource constraints from duplication and residue left by architectural evolution. Its rule is:

> No complexity removal without equivalence proof.

Current implementation truth is `EthernetAppDemo/` and `EthernetAppDemo/application.fam`. [APP_BEHAVIOR.md](APP_BEHAVIOR.md) owns intended user-visible behavior, [ARCHITECTURE.md](ARCHITECTURE.md) owns the current software model, [HARDWARE.md](HARDWARE.md) owns the controller/hardware model, [DECISIONS.md](DECISIONS.md) owns accepted decisions, and [BACKLOG.md](BACKLOG.md) owns current unresolved work and F4 identifiers. [ARCHITECTURE_EVOLUTION.md](ARCHITECTURE_EVOLUTION.md) and the historical plans/specifications explain lineage but do not override current source.

This audit is architectural readiness input, not an implementation plan or roadmap. In particular, it does not place, prioritize, or redesign historical F1 capabilities.

## 2. Audit method and classification vocabulary

Evidence came from repository-wide reference searches and static tracing of scene entry/exit, worker ownership, callbacks, packet paths, controller state, persistent state, allocation lifetimes, and compile-time gates. A symbol is called uncalled only when a fresh repository-wide search found declarations/definitions but no current call site.

Classifications used here are:

- **Essential complexity:** intrinsic protocol, hardware, concurrency, lifetime, or validation work.
- **Justified architectural complexity:** required by the current product/resource architecture.
- **Accidental complexity:** complexity that protects no identified current property.
- **Evolutionary residue:** an API, branch, or structure retained from an earlier design without a current active role.
- **Duplicated responsibility:** one concept implemented in several places without a semantic reason.
- **Split ownership:** one logical state transition distributed across multiple owners.
- **Compensating complexity:** machinery primarily needed around an unresolved defect or limitation.
- **Healthy extension seam:** clear enough in ownership, lifecycle, resources, and errors for compatible growth.
- **Fragile extension seam:** usable only while implicit or narrow constraints are preserved.
- **Redesign candidate:** unsuitable as the basis for substantial expansion.
- **Blocked by correctness:** do not substantially simplify or extend until cited findings are resolved.

“Still necessary” means the protected property is current; it does not mean the exact present implementation is the only possible implementation. SDK/HAL guarantees remain external where repository source cannot prove them.

## 3. Complexity map

| Area | Owner and lifetime | Main dependencies | Present characterization |
|---|---|---|---|
| `App` | `app_alloc()` to `app_free()` | scenes, ENC, storage, GUI, worker, scan/passive/PCAP state | Justified central owner with some split logical state |
| Scene/view layer | `SceneManager` plus reusable App views | App state, feature workers, custom events | Justified UI separation; duplicated glue and implicit callback contracts |
| Feature worker slot | `App.thread_alternative` plus `thread_alternative_owner` | scene start/exit, stop flags | Healthy ownership boundary |
| RX Dispatch | process-global `g_dispatch`, app runtime | ENC, PA14, permanent and temporary handlers | Essential primary RX boundary; fragile while direct-RX exceptions remain |
| Scanner session | stack-owned by a feature worker | borrowed App/ENC/RX state, semaphore waits | Coherent request/reply helper with accumulated cancellation/UI coupling |
| ENC28J60 driver | `App.ethernet`, app runtime | SPI, mutex, buffers, IRQ wake source | Essential hardware boundary; filter ownership is less coherent |
| Automatic responders | permanent RX registrations | current App/ENC network tuple | Justified concurrent service with local reply buffers |
| Passive family | scene/worker plus feature-scoped DB/history | RX, scanner wait, filters, storage, GUI | Good integrated feature; mixed extensibility across handler, data, filter, and persistence layers |
| Persistence | format-specific owners | Storage, shared/local `File`, strings | Legitimately specialized; some mechanical error/replace code is inconsistent |
| Sniffer | one feature worker and RX registration | filter, shared `File`, PCAP writer | Viable capture seam under present single-session constraints |
| PCAP reader/analyzer | reader scene/worker, App index/path/text | FileBrowser, shared `File`, ENC TX buffer scratch | Blocked by parser correctness; session state is split |
| Startup guards | shared helper plus feature constants | allocator statistics, thread assumptions, UI | Necessary advisory model with duplicated maintenance inputs |
| Teardown | scenes plus `app_free()` fallback | workers, RX, filters, files, GUI, ENC | Necessary layered defense; external quiescence contracts remain |

The intended high-level dependency direction is scenes → feature modules/shared services → RX/ENC/storage. Current exceptions include physical-button polling inside backends, worker-side GUI mutation, and direct controller receive in OS/DHCP paths.

## 4. App state and configuration

`App` in `app_user.h` is the application-lifetime aggregation point for GUI objects, records, `enc28j60_t`, feature state, `scan_params_t`, ARP results, passive objects, PCAP state, startup diagnostics, and the single feature-worker slot. Allocation and fallback teardown are centralized in `app_alloc()` and `app_free()` in `app_user.c`. This is healthy centralization on a memory-constrained single-instance application: it makes long-lived resources and cross-scene state explicit.

Not all co-location represents one semantic owner:

- The IPv4 tuple is split between `enc28j60_t.ip_address`/`subnet_mask`, `App.ip_gateway`/`mac_gateway`, and `App.is_dora`/`is_static_ip`. Scenes and DHCP update different subsets. This is split ownership and is blocked by F4-011 and F4-012.
- `App.enc28j60_connected` caches an earlier controller-start result while `is_link_up()` supplies live PHY state. The name encourages callers to conflate controller availability, link, and usable network configuration (F4-013).
- `scan_params_t` successfully centralizes persisted scan inputs, but `ip_ping` and `target_ip` duplicate the intended user-level Target IP concept (F4-014). `ip_start`, ranges, ports, and protocol selection are genuinely distinct.
- The custom IP editor binds directly to those arrays. It reuses one view, but editing and committing are not separate state transitions (F4-015/F4-016).
- `App.file`, `path`, and `text` are justified reusable resources while only one app feature worker is active, but their per-operation meaning is implicit. The PCAP reader additionally divides its session across `App.packet_positions`, file-static `packet_count`, `App.file`, `App.path`, `App.text`, and `ethernet->tx_buffer` scratch.

Major network-feature expansion should not add more direct writers to the existing tuple. Any simplification must preserve persistence compatibility, automatic-responder gating, DHCP/manual semantics, and feature precondition distinctions.

## 5. Scene/UI architecture

`scenes_config/app_scene_config.h` generates 24 active scenes and two `DEV_MODE`-only scenes through X-macro tables in `scenes_config/app_scene_functions.c`. Separate scenes reflect legitimate user workflows. Shared App-owned `Widget`, `Submenu`, `VariableItemList`, `TextBox`, `ByteInput`, `NumberInput`, `FileBrowser`, loading view, and IP editor reduce resident duplication.

Repeated concepts are nevertheless visible:

- enter → reset → populate → restore selected item → switch view;
- target-edit setup and callback return in Ping, Ports, OS, Scan Hosts, and Settings;
- slot check → guard calculation → thread allocation → `app_thread_claim()` → start → event-driven join;
- worker stop-flag publication and scene-exit joins;
- result submenu/detail construction and page navigation;
- controller/link/configuration precondition screens.

The common syntax is not sufficient reason to merge scenes. Safe shared backends are narrower: an edit transaction, worker-start scaffold, consistent precondition result, and reusable bounded pagination/detail helpers. They must preserve feature-specific scene history, selected-item state, event IDs, worker cleanup order, and user-visible BACK/confirmation semantics.

Several workers call widget/submenu/string APIs directly; `about_us_thread()` also polls physical buttons and renders pages itself. Whether these cross-thread GUI operations are supported is an external SDK question under F4-029. Until resolved, current worker/UI plumbing is a fragile seam rather than a pattern to copy.

Localized accidental residue is also present: `app_scene_settings_callback()` writes the same `app_scene_settings_options_menu_option` scene state twice in one callback, with no intervening read. This does not justify merging Settings scenes; it is simply a redundant state write whose removal would require confirming that no SDK side effect is attached to repeated identical writes.

## 6. Worker ownership

`AppThreadOwner`, `app_thread_claim()`, `app_thread_is_owned()`, `app_thread_join_and_free()`, and `app_thread_shutdown()` in `app_user.c` protect a real invariant: only one application-owned feature worker occupies `App.thread_alternative`, and only the matching feature consumes its lifecycle. A failed claim frees the rejected thread. Join clears the App slot before freeing the object. Application shutdown sets feature stop flags before joining the surviving owner.

Owner tagging is a healthy architectural ownership boundary under the current
one-feature-worker model. Replacing it with pointer checks would discard the identity invariant. FileBrowser, RX Dispatch, ISR callbacks, and GUI dispatch are deliberately outside this slot and need their own contracts.

Duplication remains around the boundary. Features use differently named stop flags, different button/event sources, and different join points. A shared launch helper may reduce guard/allocation/claim boilerplate, but cancellation and cleanup cannot be made generic without encoding each feature’s unblock, RX-unregister, filter-restore, file-close, and event-publication requirements. F4-029 must be resolved before treating worker-side GUI/event behavior as a reusable contract.

## 7. RX Dispatch

`libraries/chip/rx_dispatch.c` owns a process-global `g_dispatch`, a four-kilobyte thread, an eight-slot registry, a registry mutex, PA14 interrupt wakeup, and a 100 ms fallback wait. `app_alloc()` initializes it and installs permanent automatic ARP/ICMP registrations; `app_free()` unregisters those handlers before dispatcher deinitialization.

Predicates and handlers execute while the registry mutex is held. This is deliberately complex: `rx_unregister()` taking the same mutex cannot return while a callback is still executing with a borrowed feature context. It gives stack-owned wait contexts and semaphores a quiescence boundary. It also serializes all handlers, makes slow handlers delay others, and implicitly forbids registration/unregistration from inside callbacks. That contract should be made explicit before compatible expansion; simply moving callbacks outside the mutex would require an equivalent reference/counting or epoch/quiescence proof.

Permanent and temporary registrations are a useful distinction. Scanner waits register before their trigger transmit, then unregister before stack context/semaphore destruction. Passive and Sniffer similarly bound handler lifetime to their worker/session cleanup.

RX Dispatch is primary, not universal:

- active OS detection calls `receive_packet()` directly without pausing Dispatch (F4-001/F4-002);
- DHCP calls direct receive after bounded `rx_dispatch_pause()`;
- the DEV-only specific-ARP-spoof path can reach `arp_get_specific_mac()`;
- old TCP direct-RX helpers compile but have no current caller found.

`rx_dispatch_pause()` uses a nesting counter and attempts an acknowledgement, but returns after a bounded wait even without proven quiescence (F4-004). It is compensating complexity for DHCP’s exception, not a general-purpose healthy ownership mechanism. RX Dispatch becomes a healthy extension seam only for registered consumers that keep callbacks bounded and obey context lifetime rules; it is blocked as a universal receive seam by F4-001, F4-002, and F4-004.

## 8. Scanner session

`scanner_session_t` in `libraries/scanner/scanner_session.h` is stack-owned and borrows `App`, ENC, dispatcher, local network fields, and cancellation flags. It combines:

- subnet-aware next-hop selection;
- a four-entry MAC cache;
- ARP resolution;
- register-before-trigger packet waits;
- short-lived semaphore/handler ownership;
- timeout/cancellation reporting.

These responsibilities form a coherent request/reply scan abstraction: target routing, address resolution, trigger ordering, and bounded response waiting are naturally coupled. Ping, ARP scanning/spoof support, TCP/UDP scan code, OS support, and Passive infrastructure instantiate it, although not every caller uses every operation and OS still bypasses it for part of RX.

Accumulated concerns are narrower. `scanner_session_deinit()` is a no-op despite being called by active users. The assigned `view_dispatcher` field has no current behavioral use in the session implementation. Cancellation combines caller flags with direct physical BACK polling, coupling a backend primitive to Flipper input. Header text still describes the superseded direct `arp_get_specific_mac()` cache-miss path. These are evolutionary residue or cross-layer coupling, not proof that the central abstraction should be removed.

A future scanner-like feature can reuse the session if it needs one trigger/response exchange and the present cancellation/resource model. The session should not accumulate general GUI, persistent-state, or arbitrary streaming responsibilities.

## 9. ENC28J60, SPI, buffers, and filters

`libraries/chip/enc28j60.h` makes SPI ownership, mutex, `bank`, RX cursor/release state, MAC/IP/subnet fields, and 1,518-byte RX/TX buffers per-instance. Public controller operations serialize register access with the ENC mutex. `set_bank_with_mask()` uses the instance bank cache and consults hardware when a switch is required. The mutex and per-instance bank/cursor state are essential; reverting to a file-static cache or unsynchronized bank access would revive cross-context corruption risk.

Documentation discrepancy observed during this audit: `ARCHITECTURE_EVOLUTION.md` currently says bank selection is derived from chip registers rather than relying on a cache. Current `enc28j60.c` instead compares against `enc28j60_t.bank`, reads the chip bank only when a change is needed, and then updates the per-instance cache. `ARCHITECTURE.md` also names the worker-owner field as `App.thread_owner`; the source name is `App.thread_alternative_owner`. Those canonical documents are intentionally not edited in this phase.

The controller command protocol, banked registers, RX ring, transmit area, reset/link checks, and GPIO/IRQ integration are essential hardware complexity. Blanket bulk-SPI conversion or a different SRAM split cannot be justified by line-count reduction.

Shared instance RX/TX frame buffers are justified resource complexity under one foreground feature-worker rule. TX automatic responders avoid the shared TX buffer by building callback-local frames. RX bytes returned from `receive_packet()` outlive the mutex, so competing receivers can overwrite them; this makes the OS direct-RX exception a correctness blocker (F4-002), not evidence that the buffers alone are a bad design.

Filter helpers expose imperative feature-specific mutations: Passive enables/disables multicast, Sniffer enables broad valid-CRC reception and restores the normal filter, and DHCP adjusts broadcast acceptance then restores it. Normal paths have restoration, but there is no explicit filter owner, snapshot, or nested transition contract. This is a fragile extension seam. A future simplification may centralize transition ownership, but must prove the exact normal filter, protocol acceptance, nesting/coexistence rules, failure cleanup, and restoration on every exit.

The PA14 callback-removal and runt-frame contracts remain external under F4-032/F4-034. Complexity preserving interrupt wakeup plus fallback polling must remain until those contracts are established.

## 10. Automatic responders

`app_alloc()` installs permanent ARP and ICMP predicates/handlers in `app_user.c`. They borrow the application-lifetime `App`/ENC object and current network fields, gate on configuration state, and coexist with dispatcher-active features. Permanence avoids gaps between scenes and keeps basic host behavior independent of a specific feature worker.

Local reply frames and ICMP payload storage are important concurrency/resource boundaries: responders do not borrow the foreground shared TX construction buffer. They still transmit through the mutex-protected controller path. Their stack/temporary heap peak can overlap registered feature workers because callbacks execute on the dispatcher thread.

This is justified architectural complexity. The mutable global state used by ARP reply construction is not: it becomes hazardous with the active OS receive exception (F4-003). Network-tuple mutation also needs a coherent snapshot/transition contract before adding responder types. Automatic handlers are a suitable pattern only for small, bounded, always-applicable responses—not a general feature framework.

## 11. Passive Discovery

Current Passive Discovery uses one feature worker, one scanner session, repeated temporary registered waits, one feature-scoped live database, and one handler table for LLDP, CDP, and EAPOL. `PassiveProtocolALL` applies all matching handlers; protocol-specific modes filter the same pipeline. Identity is `(source MAC, protocol)`, preserving distinct observations from one device. The scenes share live/saved list and detail presentation while `passive_history.c` owns the persistent representation.

Healthy shared layers are:

- one RX/worker/filter lifecycle for the feature family;
- a bounded protocol handler table with name, initialization, processing, cleanup, page-count, and detail-builder functions;
- one bounded live DB and source-aware identity;
- reusable live/saved list and details scenes;
- one history interface for browse/filter/decode/merge/clear.

Protocol differences are fundamental where LLDP, CDP, and EAPOL have different frame predicates, TLVs/fields, validation, identity data, and detail pages. Those branches should not be erased behind a falsely uniform parser.

The handler table is a healthy-with-constraints seam, not a complete plugin boundary. Its `run` member is never invoked by the current orchestrator; only LLDP supplies it. `lldp_module_run()` and related wrappers therefore retain an older execution model. The database and history formats explicitly enumerate protocol fields and flags, and `neighbor_t` is a fixed superset stored for every one of 32 slots. A new field can increase every record, and a new protocol currently touches enum, handler table, identity flags, DB model, history encoding/decoding/filtering, and UI. Thus runtime dispatch is extensible, but the data/persistence/filter boundary is fragile for arbitrary growth.

Fresh repository-wide reference checks also found no current callers for `passive_discovery_module_get_neighbor_count()`, `passive_discovery_module_get_neighbor()`, `neighbor_db_init()`, `neighbor_db_load()`, `neighbor_db_save()`, or `neighbor_db_find()`. Removal would still require build/link checks and confirmation that no external ABI/test consumes them. `neighbor_db`’s reserve comment refers to a four-kilobyte Passive stack while current source configures three kilobytes; this is maintenance drift, not proof that the guard result is incorrect.

Passive is an extension seam for compatible bounded link-layer observations, but expansion must not assume that its current record/history/filter model scales automatically. F4-023 and conditional F4-024 must also remain visible.

## 12. Persistence

The four persistence families are legitimately specialized:

- `libraries/settings/settings.c` uses versioned Flipper Format for mutable named configuration; writer v3, loader v1–v3.
- Scan Hosts uses compact native `last_scan.bin` for a timestamp, bounded count, and `App.ip_list` results.
- `libraries/protocol_tools/passive_history.c` uses a bounded custom version/magic/CRC/index/record model and a temporary replacement file.
- `modules/capture_module.c` produces PCAP for interoperability, while Read PCAP indexes and interprets it.

Unifying these formats would obscure their different compatibility and validation semantics. Repeated storage mechanics—open/close discipline, exact read/write checks, temporary replacement, and error propagation—could use narrow helpers, but format-specific validation must remain local.

Current inconsistency is tied to known findings: settings and last-scan create-always writes lack safe replacement or complete result handling (F4-017/F4-020); loads accept partial/incompletely validated state (F4-018/F4-019); cancellation can expose partial scan RAM state (F4-021); the scan format is native/unversioned (F4-022); Passive merge failure is ignored (F4-023); rename atomicity remains external (F4-033). These findings block treating current storage boilerplate as a model to copy.

## 13. Sniffer and PCAP

Sniffer has a coherent present lifecycle: a feature worker checks controller/link, selects promiscuous reception, creates a PCAP, registers a dispatcher handler, updates the UI, then marks stopped, unregisters, restores the filter, and closes the file. The handler writes captured frames on the dispatcher thread. The one-worker rule and file-static capture timestamp origin make one capture session at a time an explicit current assumption.

It is an extend-with-constraints capture seam. F4-008 blocks claiming reliable capture completion because write failure is ignored by the handler/counter. Worker-side GUI access and blocking storage in the dispatcher callback also depend on F4-029 and bounded-callback expectations. Significant capture growth should give session state and error propagation explicit ownership rather than add more file-static state.

The PCAP reader is not safe to extend. Its 2,000-entry `uint64_t` index is a simple bounded random-navigation structure, but it requires one 16,000-byte contiguous allocation. Reader state is split across App fields, a file-static packet count, shared `File`/strings, and `ethernet->tx_buffer` used as read scratch. `analysis_module.c` combines fixed-offset parsing with textual rendering. F4-005, F4-006, and F4-007 are correctness blockers. The index is a justified current product limit, but major analyzer growth should not build on the present parser/session boundary without redesign.

## 14. Network-state model

Current source has at least four necessary concepts:

1. controller allocation/start availability;
2. live PHY link;
3. configured IPv4/subnet/gateway/MAC tuple;
4. acquisition/mode flags (`is_dora`, `is_static_ip`).

The complexity of having distinct concepts is essential. Their present names and ownership are not. `enc28j60_connected`, live `is_link_up()`, address fields, gateway fields, and acquisition flags are read and refreshed inconsistently. Manual settings and DHCP update the tuple in distributed stages. Automatic responders depend on flags while scanners depend on different subsets. Target state is additionally split between `ip_ping` and `target_ip`.

This is the highest-leverage simplify-first area before broad network-feature expansion. The goal is not one boolean called “connected”; it is an explicit state model and atomic transition boundary that retains link-only, configured-IP, gateway-required, multicast, and no-IP use cases. F4-011 through F4-016 define current correctness/behavior constraints.

## 15. Memory and startup guards

The 24 KiB main stack, four-kilobyte RX Dispatch stack, App/GUI/ENC objects, two 1,518-byte frame buffers, and permanent handlers are resident. Only one app-owned feature stack (three to ten kilobytes) is intended at a time. The approximately 15.5 KiB Passive DB and 16 KiB PCAP index are feature-scoped and are not intended to coexist as active resources. Automatic responder demand may overlap dispatcher-active features.

Feature-scoped allocation and one-worker mutual exclusion are healthy resource architecture. The large fixed Passive record array and monolithic PCAP index are current product constraints and contiguous-block sensitivities, not source-proven leaks or persistent fragmentation.

`libraries/functions/startup_guard.c` correctly distinguishes total free heap from largest allocation block and uses saturating calculations. It centralizes assumed eight-byte alignment, eight-byte heap header, 328-byte thread metadata, and a 1,024-byte reserve. These are API/allocator-dependent assumptions, and passing a guard reserves nothing. Later GUI, storage, string, scanner, and responder allocations can still occur (F4-028).

Maintenance complexity remains because each scene independently supplies stack/additional totals/largest block, repeats launch/error UI, and sometimes performs multi-boundary guards. Passive DB code separately duplicates allocator alignment/header assumptions. A declarative feature requirement plus common guarded-launch boundary could reduce drift while retaining both total and contiguous tests, diagnostic boundary identity, post-check allocation failure handling, and feature-specific later-allocation caveats. F4-026–F4-028 constrain that work; stack claims still require runtime high-water validation.

## 16. Teardown and relaunch

Representative cleanup deliberately follows feature-specific order: signal stop, unblock or finish waits, join the matching worker, quiesce/unregister callbacks, restore filters, close files, free feature allocations, then reset reusable GUI. `app_free()` provides fallback shutdown for any surviving feature worker, Passive objects, PCAP index, permanent registrations, dispatcher/IRQ, GUI, storage records, ENC/SPI, strings/file, and App.

Some repetition is defensive rather than accidental. Scene exit handles normal transitions promptly; app-level fallback handles abnormal or top-level termination. Cleanup cannot be collapsed by line count because callback context, filter state, and file data must remain alive until producers stop.

External questions remain: top-level Passive `on_exit()` ordering (F4-024), GUI/FileBrowser callback quiescence (F4-029/F4-031), GPIO callback removal and hardware shutdown (F4-032), and storage rename guarantees (F4-033). Relaunch safety therefore depends partly on SDK/HAL contracts even though repository-owned cleanup order is explicit.

## 17. Legacy, dormant, and reachability findings

| Path/symbol | Fresh classification | Evidence and removal prerequisite |
|---|---|---|
| `ArpSpoofingSpecificIP.c` and its two scenes | DEV-ONLY | Scene config/menu/source are gated by `DEV_MODE`; current build defines `DEV_MODE=0`. Retain unless the product decision removes the development feature. |
| UDP port scan branch | COMPILE-REACHABLE BUT USER-INACCESSIBLE / dormant | `ports_scanner_thread()` retains the UDP branch, but `protocols[]` contains only `"TCP"` and selection code is disabled. Persisted nonzero index is a separate F4-009 hazard. Removal requires a product decision plus settings compatibility analysis. |
| `arp_get_specific_mac()` | DEV/LEGACY REACHABLE | Used by the DEV-only specific spoof path and by currently uncalled TCP helpers; not part of active normal scanner resolution. Remove only with those callers/feature decisions. |
| `tcp_handshake_process()` | CURRENTLY UNCALLED / historical helper | Definition/declaration found; no current caller. Build/link and intended external API checks required before removal. |
| `tcp_handshake_process_spoof()` | CURRENTLY UNCALLED / historical helper | Same standard; contains legacy direct RX. |
| `tcp_os_detector()` | CURRENTLY UNCALLED / historical helper | Current OS module uses other paths. Same removal proof required. |
| Passive handler `.run` / `lldp_module_run()` | CURRENTLY UNCALLED / evolutionary residue | Table stores `.run`, but the orchestrator calls processing/detail hooks, not `run`. Remove only after handler ABI and all references are verified. |
| Passive module neighbor wrappers | CURRENTLY UNCALLED | `passive_discovery_module_get_neighbor_count/get_neighbor` have no current callers. Confirm no external/test contract. |
| `neighbor_db_init/load/save/find` | CURRENTLY UNCALLED / incomplete legacy API | Repository-wide search found no current callers; `load/save` are not the current Passive History mechanism. Confirm linkage/API expectations before removal. |
| RX fallback polling | ACTIVE | Not residue merely because PA14 IRQ exists; it supplies recovery/wakeup fallback and must remain until hardware/HAL guarantees justify removal. |
| DHCP direct RX and dispatcher pause | ACTIVE / compensating | Current Get IP path. Simplification is blocked by F4-004 and must preserve DHCP transaction behavior. |
| OS direct RX | ACTIVE / blocked by correctness | Current `os_scan()` path; F4-001/F4-002. It is not legacy just because dispatcher architecture exists. |

## 18. Cross-cutting duplication

| Repeated concept | Assessment |
|---|---|
| Target editing | One shared concept with multiple feature destinations; good abstraction candidate only as a transactional editor preserving distinct target semantics until F4-014 is resolved. |
| Worker startup | Repeated mechanical sequence; common launch scaffold is plausible, while stop/join/cleanup policies remain feature-specific. |
| Cancellation | Common intent but different unblock/cleanup needs; a cancel-token interface could replace backend button polling without erasing feature semantics. |
| ARP resolution and registered packet waiting | Correctly centralized in scanner session for active migrated paths; remaining direct helpers are reachability-specific residue/exceptions. |
| Link/config checks | Duplicated and semantically inconsistent due to split network state; centralize meanings, not merely UI strings. |
| IP/MAC formatting | Repeated low-risk presentation mechanics; bounded formatting helpers could reduce minor duplication. |
| Result menus/details | Similar shell, different record lifetime and fields; share bounded navigation/render primitives, not one generic result model. |
| Storage open/write/replace | Mechanical duplication exists; validation/version/format policies are intentionally distinct. |
| Filter transition/restore | One shared hardware concept currently distributed across features; needs explicit ownership semantics before centralization. |
| Pagination | Passive has a coherent page-count/build contract; About and PCAP have different input/session rules. Reuse only the navigation primitive. |
| Guard formulas | One concept partly centralized, while feature constants and Passive allocation assumptions remain distributed. |

## 19. Complexity that must be preserved

| Complexity | Failure mode it prevents |
|---|---|
| `AppThreadOwner` identity checks | A scene joining/freeing another feature’s worker |
| Cancel/unblock followed by join-before-free | Worker use-after-free of App, GUI, file, semaphore, or controller state |
| RX registry locking and unregister quiescence | Callback use of destroyed stack/session context |
| Register-before-trigger | Losing a fast response in the registration gap |
| ENC mutex and per-instance bank/RX cursor state | Interleaved register-bank or controller-state corruption |
| Callback-local responder frames | Foreground/automatic responder TX construction overwrite |
| Shared RX/TX buffer coexistence restrictions | Simultaneous parsing/construction over one instance buffer |
| Filter restoration on all exits | Later features inheriting promiscuous/multicast/broadcast state |
| Parser length/bounds validation | Out-of-bounds access on network or stored input |
| Persistence magic/version/CRC/record boundaries where present | Misinterpreting incompatible, partial, or corrupted state |
| Total-free and largest-block checks as separate guard dimensions | Treating aggregate free bytes as proof of a contiguous allocation |
| Feature-scoped large allocation and one-worker rule | Summing mutually exclusive stacks/large feature state in constrained heap |
| Feature-specific cancellation/cleanup order | Producer or callback surviving the resource it borrows |
| Application fallback teardown | Resource survival on non-normal scene exit |
| IRQ fallback wakeup and external-contract caution | Assuming unproven interrupt delivery/removal semantics |

## 20. Simplification candidates

### SC-01 — Network/configuration state transitions

- **Current complexity:** one logical network configuration is split across ENC fields, App gateway/MAC/flags, cached start state, live link reads, and distributed DHCP/manual writers.
- **Historical/architectural reason:** features and persistence accumulated incrementally around the controller object.
- **Property currently protected:** distinct hardware, link, configuration, acquisition, and gateway concepts.
- **Why it may no longer be necessary:** distributed mutation is not required to preserve those distinctions and produces F4-011–F4-013.
- **Direction:** one explicit network-state owner and transactional updates, while retaining typed preconditions rather than one “connected” flag.
- **Equivalence proof:** all readers/writers, persisted schema compatibility, DHCP cancellation, manual configuration, auto responders, scanner routing, and link-only features behave equivalently or more strongly.
- **Dependencies:** F4-011, F4-012, F4-013; related F4-016.
- **Risk:** disabling valid link-only modes or creating stale responder/scanner state.
- **Benefit:** clearer ownership and safer future preconditions.
- **Confidence:** high.

### SC-02 — Transactional target/address editing

- **Current complexity:** scenes bind the custom IP/ByteInput views directly to live App/controller arrays and implement callbacks separately.
- **Reason:** a reusable editor was added without a separate draft/commit layer.
- **Protected property:** each scene edits its intended field and returns through its own history.
- **Why unnecessary:** direct live mutation is not required for view reuse.
- **Direction:** reusable draft/validate/commit/cancel transaction around existing editors.
- **Equivalence proof:** OK/BACK semantics, initial values, callback ordering, scene state, settings persistence timing, and MAC SDK behavior.
- **Dependencies:** F4-014–F4-016 and external F4-030.
- **Risk:** committing the wrong target or changing navigation/persistence behavior.
- **Benefit:** one confirmation contract and less callback duplication.
- **Confidence:** high for IP; medium for MAC until SDK contract is known.

### SC-03 — Declarative guarded worker launch

- **Current complexity:** scenes repeat slot check, requirement calculation, guard UI, allocation, claim, start, retry, and cleanup setup.
- **Reason:** guards and owner tags were added after feature workers already existed.
- **Protected property:** one owner, both heap dimensions, allocation-failure handling, feature-specific stack sizing.
- **Why unnecessary:** the mechanical launch sequence is common even though worker cleanup is not.
- **Direction:** a narrow launch helper driven by explicit per-feature requirements and callbacks; leave stop/join/resource teardown feature-owned.
- **Equivalence proof:** exact NeedT/NeedB formula, diagnostic boundary, check-to-use limitations, rejected-thread free, retry flow, and each feature’s later allocations remain visible.
- **Dependencies:** F4-026–F4-029.
- **Risk:** hiding omitted allocations or weakening owner/cancellation rules.
- **Benefit:** lower maintenance drift and smaller repeated scene glue.
- **Confidence:** medium-high.

### SC-04 — Scanner cancellation boundary and no-op API residue

- **Current complexity:** scanner session combines caller flags with physical BACK polling; retains unused dispatcher state and a no-op `deinit()` contract.
- **Reason:** migrated polling code and staged future-subscription plans accumulated in the helper.
- **Protected property:** responsive cancellation and explicit end-of-session call sites.
- **Why unnecessary:** backend input polling and meaningless cleanup need not be part of request/reply networking.
- **Direction:** one caller-owned cancellation contract; either give `deinit()` real owned state or remove it and unused fields after proof.
- **Equivalence proof:** every active caller cancels waits promptly, unregisters before context destruction, preserves BACK behavior, and no ABI/test relies on the API.
- **Dependencies:** F4-029 and all scanner caller reachability.
- **Risk:** uncancellable waits or premature stack-context destruction.
- **Benefit:** clearer backend boundary and smaller API surface.
- **Confidence:** medium.

### SC-05 — Direct-RX/pause exception reduction

- **Current complexity:** primary registered RX coexists with active OS direct RX and paused-dispatch DHCP direct RX.
- **Reason:** incremental migration from feature-owned receive loops.
- **Protected property:** existing OS/DHCP protocol transactions.
- **Why unnecessary:** multiple FIFO ownership models are not inherently required.
- **Direction:** converge compatible receive consumers on one explicit ownership model; do not remove pause until direct receive is gone or a real barrier exists.
- **Equivalence proof:** packet ordering, timeouts, cancellation, DHCP broadcast/filter transitions, OS multi-probe behavior, permanent responder coexistence, and callback lifetime.
- **Dependencies:** blocked by F4-001, F4-002, F4-004; F4-003 may disappear with the harmful overlap.
- **Risk:** lost replies, dual consumers, deadlock, or transaction regressions.
- **Benefit:** one auditable RX contract.
- **Confidence:** high as a direction, not an implementation choice.

### SC-06 — Receive-filter transition ownership

- **Current complexity:** feature-specific helpers mutate shared filter bits and encode restoration conventions implicitly.
- **Reason:** Passive, DHCP, and Sniffer acquired distinct reception needs over time.
- **Protected property:** protocol acceptance during a feature and restoration afterward.
- **Why unnecessary:** each feature need not independently encode the shared-state ownership convention.
- **Direction:** explicit scoped transition/restore ownership or equivalent state contract.
- **Equivalence proof:** exact bit patterns, programmed patterns, nested/concurrent operations, failures, cancellation, reset/start, and normal baseline restoration.
- **Dependencies:** F4-032/F4-034 external hardware contracts; no current poisoning finding.
- **Risk:** silent packet loss or promiscuous reception persisting.
- **Benefit:** safer compatible RX feature growth.
- **Confidence:** medium.

### SC-07 — Passive obsolete execution/wrapper surface

- **Current complexity:** handler `.run`, LLDP run wrappers, module neighbor wrappers, and old neighbor DB APIs coexist with the current orchestrator/history design.
- **Reason:** LLDP-first implementation evolved into a consolidated Passive pipeline.
- **Protected property:** possible internal/API compatibility only; no active caller was found.
- **Why unnecessary:** current flow uses registered frames, handler `process_frame`, DB update, and details hooks.
- **Direction:** remove or clearly isolate verified uncalled compatibility surface.
- **Equivalence proof:** complete build/link/reference checks for all configurations, tests/external API confirmation, and no historical path re-enabled by flags.
- **Dependencies:** independent of F4 runtime findings.
- **Risk:** breaking a non-obvious build/test or future intended hook.
- **Benefit:** smaller extension contract and less misleading code.
- **Confidence:** high on current uncalled status; medium on removal authorization.

### SC-08 — Passive data/persistence extension boundary

- **Current complexity:** fixed superset `neighbor_t` and protocol switches span DB, history, filtering, serialization, and UI.
- **Reason:** three related protocols were consolidated while retaining bounded predictable storage.
- **Protected property:** bounded memory, source-aware identity, offline compatibility, and protocol-specific details.
- **Why it may become unnecessary:** extending every record and switch for every compatible observation will amplify cost and coupling.
- **Direction:** before substantial expansion, define a stable common identity/index plus bounded protocol-owned payload/version contract; retain present limits unless resource analysis proves changes.
- **Equivalence proof:** existing LLDP/CDP/EAPOL identity, 32-record bound, history compatibility/CRC, filtering, detail pages, and allocation coexistence.
- **Dependencies:** F4-023, F4-024, F4-026, F4-033.
- **Risk:** history incompatibility, unbounded memory, or merged identities.
- **Benefit:** controlled protocol growth without enlarging every record.
- **Confidence:** medium; redesign only if growth demands it.

### SC-09 — Narrow storage mechanics

- **Current complexity:** formats repeat portions of File open/read/write/close/error/replace handling with inconsistent results.
- **Reason:** independent persistence features with different formats and eras.
- **Protected property:** specialized schema, validation, and compatibility rules.
- **Why unnecessary:** exact I/O and replacement mechanics need not be reinvented.
- **Direction:** small mechanical helpers, never a universal serialized-object layer.
- **Equivalence proof:** byte-for-byte formats, open modes, partial-I/O behavior, crash/replacement semantics, user error propagation, and shared/local File lifetimes.
- **Dependencies:** F4-017–F4-023 and external F4-033.
- **Risk:** flattening distinct safety policies or changing compatibility.
- **Benefit:** consistent error handling and less boilerplate.
- **Confidence:** medium-high after correctness fixes define desired behavior.

### SC-10 — PCAP reader session/parser boundary

- **Current complexity:** reader session state is split among App, a file-static count, controller scratch, strings/File, index, parsing, and rendering.
- **Reason:** browsing, indexing, analysis, and resource hardening evolved separately.
- **Protected property:** bounded random packet navigation and reuse of scarce buffers.
- **Why unnecessary:** split ownership and parser/render coupling are not required for those properties.
- **Direction:** redesign around one bounded reader session and validated parse representation before adding analyzers.
- **Equivalence proof:** all PCAP header/record bounds, 2,000-entry navigation behavior, 16 KiB lifecycle, file ownership, text output, cancellation, and malformed-input handling.
- **Dependencies:** blocked by F4-005, F4-006, F4-007; related F4-026–F4-028/F4-031.
- **Risk:** memory-safety regression, file leak, or navigation incompatibility.
- **Benefit:** clear ownership and safe analyzer growth.
- **Confidence:** high that redesign precedes major expansion.

### SC-11 — Repeated scene rendering/navigation glue

- **Current complexity:** feature scenes repeatedly rebuild menus/status/details and sometimes manually refresh or invoke scene-entry behavior.
- **Reason:** user workflows were added independently around shared SDK views.
- **Protected property:** distinct labels, selection, history, and feature events.
- **Why unnecessary:** bounded presentation/navigation primitives can be shared without merging features.
- **Direction:** share only stable helpers for status pages, result navigation, and bounded page progression.
- **Equivalence proof:** selected-item retention, BACK stack, callback context, dynamic text lifetime, reset timing, and worker/UI synchronization.
- **Dependencies:** F4-029/F4-031; F4-015 for editors.
- **Risk:** stale callbacks/text or broken scene history.
- **Benefit:** lower UI glue duplication.
- **Confidence:** medium.

### SC-12 — Dormant and legacy networking paths

- **Current complexity:** dormant UDP selection, DEV-only spoofing, old TCP direct-RX functions, and legacy ARP receive helper remain in the build/source tree.
- **Reason:** historical capability work and incremental RX migration.
- **Protected property:** potential development mode and unadopted compatibility paths.
- **Why unnecessary:** verified uncalled paths enlarge the RX surface and mislead maintainers.
- **Direction:** make product decisions first, then remove only paths proven unreachable in every supported configuration or migrate retained paths to current ownership contracts.
- **Equivalence proof:** compile-time variants, menu reachability, settings values, callers/function pointers, tests, and intended future use.
- **Dependencies:** F4-001–F4-004, F4-009/F4-010.
- **Risk:** silently deleting a supported development capability or leaving a persisted state without a consumer.
- **Benefit:** smaller attack/reasoning surface.
- **Confidence:** high on classifications, conditional on product decision.

## 21. Extension-seam assessment

| Subsystem | Assessment | Why |
|---|---|---|
| `App` general state | SIMPLIFY FIRST | Central lifetime is sound, but network/target/session concepts are overloaded or split. |
| Scenes/UI | EXTEND WITH CURRENT CONSTRAINTS | Separate workflows and reusable views are valid; edit and cross-thread contracts need care. |
| `AppThreadOwner` | HEALTHY EXTENSION SEAM | Explicit single-slot identity and join/free boundary; retain feature-specific cancellation. |
| RX registered consumers | BLOCKED BY CORRECTNESS as a universal seam; otherwise FRAGILE | Strong callback lifetime, but direct RX and implicit callback-under-lock rules remain. |
| `scanner_session` | EXTEND WITH CURRENT CONSTRAINTS | Coherent bounded request/reply responsibilities; do not add UI/storage/streaming duties. |
| ENC driver operations | HEALTHY EXTENSION SEAM | Clear instance/mutex/SPI boundary, subject to hardware contracts. |
| Shared ENC buffers | NOT AN EXTENSION SEAM | Resource implementation detail with strict coexistence assumptions. |
| Receive filters | SIMPLIFY FIRST / FRAGILE | Shared hardware state lacks explicit scoped ownership. |
| Automatic responders | EXTEND WITH CURRENT CONSTRAINTS | Suitable only for small bounded permanent behavior using local frames and stable state. |
| Passive handler/runtime | EXTEND WITH CURRENT CONSTRAINTS | Good common RX/worker/dispatch boundary; data/history/filter growth is coupled. |
| Passive DB/history model | REDESIGN BEFORE MAJOR EXPANSION | Fixed superset and protocol switches amplify per-protocol state and compatibility work. |
| Settings | FIX CORRECTNESS FIRST | Format specialization is valid; transaction/validation semantics are not a reusable model yet. |
| Last-scan persistence | FIX CORRECTNESS FIRST | Native format and partial I/O/state handling constrain reuse. |
| Passive History | EXTEND WITH CURRENT CONSTRAINTS | Strongest bounded format; merge/error/rename contracts remain. |
| Sniffer | EXTEND WITH CURRENT CONSTRAINTS | Coherent one-session lifecycle; failure propagation and callback/GUI constraints remain. |
| PCAP parser/analyzer | REDESIGN BEFORE MAJOR EXPANSION / BLOCKED BY CORRECTNESS | F4-005–F4-007 and split session/render state. |
| Network-state model | SIMPLIFY FIRST / FIX CORRECTNESS FIRST | Concepts are necessary but writers/representations are split. |
| Startup guards | EXTEND WITH CURRENT CONSTRAINTS | Both heap dimensions are correct concepts; assumptions and per-feature formulas can drift. |
| Teardown/relaunch | EXTERNAL CONTRACT MUST BE RESOLVED FIRST for affected edges | Repository ordering is defensive; SDK/HAL quiescence remains conditional. |

## 22. Pre-expansion readiness map

This is architectural readiness, not roadmap priority.

- **Safe to extend now:** owner-tag enumeration/helpers for another mutually exclusive app worker; ENC operations through the existing mutex/instance boundary; bounded protocol-format helpers that retain validation.
- **Extend with current constraints:** scanner request/reply uses; separate scenes using main-thread callbacks; Sniffer’s single-session capture; Passive runtime for closely compatible bounded observations; Passive History within present identity/version/resource assumptions; startup guards that retain total-free and largest-block dimensions.
- **Simplify first:** network-state mutation/meaning; target edit transactions; filter transition ownership; repeated guarded-worker startup; scanner backend input coupling; major additions to the fixed Passive record model.
- **Fix correctness first:** OS/RX convergence (F4-001–F4-004), PCAP parser/indexing (F4-005–F4-007), protocol selection (F4-009), network tuple/target/editor behavior (F4-011–F4-016), and persistence mechanics before copying them (F4-017–F4-023).
- **Redesign before major expansion:** generic PCAP analyzer/session; Passive DB/history representation if expansion would substantially add protocol-owned data; any general service built on the current mixed network-state booleans.
- **External contract must be resolved first:** worker GUI/event/stop semantics (F4-029), ByteInput cancellation (F4-030), FileBrowser callback quiescence (F4-031), GPIO/IRQ teardown (F4-032), temporary-file replacement guarantees (F4-033), and short-frame delivery (F4-034).

## 23. Relationship to current F4 findings

| Complexity area | Relationship |
|---|---|
| RX direct exceptions/pause | Caused by/compensates for migration limits; blocked by F4-001, F4-002, F4-004; F4-003 is overlap-dependent |
| PCAP split parser/session | Blocked by F4-005–F4-007; capture error path by F4-008 |
| Dormant UDP/protocol selector | F4-009 is active input safety; F4-010 describes dormant semantic debt |
| Network/config state | Current split ownership manifests as F4-011–F4-014 |
| Editor/persistence timing | Current coupling manifests as F4-015/F4-016; MAC detail awaits F4-030 |
| Storage mechanics | Simplification should follow desired semantics from F4-017–F4-023/F4-033 |
| Passive teardown/history | F4-023 and conditional F4-024 constrain consolidation |
| ARP Spoof BACK behavior | F4-025 is feature semantics; do not genericize cancellation until corrected |
| Memory/guards | F4-026–F4-028 describe resource and model limits, not leaks |
| Worker/GUI/event boundary | Extension depends on F4-029/F4-031 |
| ENC IRQ/frame boundary | Extension depends on F4-032/F4-034 |
| Owner-tagged worker and ENC mutex | Independent preserved complexity; no current F4 defect requires their removal |
| Passive uncalled wrappers | Independent evolutionary residue; not a correctness finding |

No new F4 identifier is created here. The audit found no new source-proven active memory leak, persistent allocator fragmentation, or stack overflow.

## 24. Complexity audit matrix

| Area | Current complexity | Reason | Property protected | Classification | Necessary? | Direction | Equivalence proof | F4 | Readiness | Risk | Confidence |
|---|---|---|---|---|---|---|---|---|---|---|---|
| App shared state | One large lifetime aggregate | Single-instance/resource reuse | Explicit long-lived ownership | Justified + split | Central owner yes | Narrow logical state owners | All consumers/lifetimes | 011–016 | Simplify first | High | High |
| `scan_params` | Persisted multi-feature inputs | Replaced file statics | Cross-scene state | Justified + duplicate target | Mostly | Unify only intended target semantics | UI/storage/transitions | 009,014–016 | Fix/simplify | High | High |
| Network fields | ENC/App/flags/cache/live link | Incremental features | Distinct network concepts | Split ownership | Concepts yes | Transactional typed state | DHCP/manual/responders/scanners | 011–013 | Fix first | High | High |
| Scene/UI glue | 24+2 scenes, reused views, repeated callbacks | Distinct workflows | Navigation and bounded UI state | Justified + duplicated | Partly | Share narrow primitives | Scene history/callback lifetime | 015,029–031 | Constrained | Medium | High |
| `AppThreadOwner` | Tagged single feature slot | Lifecycle hardening | Correct worker identity | Healthy seam | Yes | Preserve | Claim/join/shutdown invariants | 029 | Safe/constrained | High | High |
| RX registry | 8 locked slots, callback under mutex | Multi-consumer RX | Context lifetime/quiescence | Essential + fragile seam | Yes | Explicit contract or equivalent scheme | Unregister blocks callbacks | 001–004 | Fix first universally | Critical | High |
| RX pause/resume | Nested count, bounded ack | DHCP direct RX | Attempted FIFO exclusivity | Compensating | Until migration | Converge ownership | DHCP timing/filter/cancel | 004 | Fix first | Critical | High |
| `scanner_session` | Routing/cache/ARP/wait/cancel | Scanner consolidation | Ordered bounded exchanges | Healthy with residue | Core yes | Narrow cancel/API cleanup | All callers and lifetime | 001–004,029 | Constrained | High | High |
| ENC mutex/state | Instance bank/cursor and serialized I/O | Hardware concurrency | Correct register/bank state | Essential | Yes | Preserve | Hardware transaction equivalence | 032,034 | Safe with contracts | Critical | High |
| Shared RX/TX buffers | One pair per ENC instance | RAM constraint | Bounded resource use | Justified | Yes currently | Preserve coexistence or prove replacement | No overlapping parse/build | 002,026 | Not a seam | Critical | High |
| Auto ARP/ICMP | Permanent registered callbacks/local replies | Cross-scene host behavior | Availability and TX isolation | Justified | Yes | Keep bounded/local | State snapshot and callback bounds | 003,027,029 | Constrained | High | High |
| Filter transitions | Imperative shared register changes | Different feature capture needs | Accept/restore frames | Fragile split ownership | Semantics yes | Scoped ownership | Exact bits/nesting/all exits | 032,034 | Simplify first | Critical | High |
| Passive handler model | Table plus protocol hooks | Consolidated protocols | Shared lifecycle, distinct parse/render | Healthy with residue | Core yes | Remove uncalled `.run` only after proof | All modes/ABI | 023,024 | Constrained | Medium | High |
| Passive DB/model | 32 fixed superset records | Bounded RAM/shared identity | Predictable memory and identity | Justified + fragile growth | Current yes | Redesign only for major growth | History/identity/resource parity | 023,024,026 | Redesign for major growth | High | High |
| Passive History | Versioned CRC/index/temp rewrite | Offline bounded history | Compatibility/integrity | Essential specialization | Yes | Share mechanics only | Format/error/rename parity | 023,033 | Constrained | High | High |
| Settings | Flipper Format v1–v3 load/v3 write | Named evolving configuration | Compatibility | Specialized + unsafe mechanics | Yes | Transactional validation | Schema and UI semantics | 009,011,016–018 | Fix first | High | High |
| Last scan | Native timestamp/count/results | Simple ARP history | Cross-feature saved targets | Specialized debt | Current yes | Harden before reuse | Existing file/navigation semantics | 019–022 | Fix first | High | High |
| Sniffer | Worker, filter, RX handler, file, UI | Dispatcher migration | Session cleanup/capture | Viable fragile seam | Yes | Explicit session/error state | Write/filter/handler/UI parity | 008,029 | Constrained | High | High |
| PCAP indexing | 2,000 offsets/16 KiB block | Random navigation | Bounded lookup | Justified product limit | Current yes | Reassess with reader redesign | Navigation/resource equivalence | 007,026–028 | Redesign major growth | High | High |
| Packet analyzer | Fixed-offset parse plus text render | Incremental protocol display | User-readable decoding | Redesign candidate | Capability yes | Validated parse/render boundary | All bounds/output behavior | 005–007 | Blocked | Critical | High |
| Startup guards | SDK assumptions + total/max formulas | Resource failures | Preflight diagnostics | Justified + duplicated inputs | Yes | Declarative requirements | Both dimensions/failure paths | 026–028 | Constrained | High | High |
| Teardown/relaunch | Scene cleanup plus App fallback | Multiple producers/borrowers | No surviving callbacks/resources | Essential + defensive | Yes | Consolidate only proven mechanics | Ordering/quiescence/all exits | 024,029–033 | External edges | Critical | High |
| Dormant UDP | Compiled branch, hidden selector | Incomplete historical capability | Possible retained path | Dormant residue | Product decision | Remove or finish later, not here | Flags/settings/callers | 009,010 | Not safe as active | Medium | High |
| Legacy direct RX | DEV/helper/OS/DHCP variants | Incremental RX migration | Existing transactions/dev mode | Mixed active and residue | Active paths yes | Reachability-specific convergence/removal | FIFO/timing/all configs | 001–004 | Fix first | Critical | High |

## 25. Inputs for Phase 9

When reconciling a historical capability, Phase 9 should ask which present seam can own it without reviving superseded assumptions. It should use these facts:

- Reuse `AppThreadOwner` for mutually exclusive app feature workers; do not introduce pointer-only lifecycle ownership.
- Begin receive design with registered RX and register-before-trigger. Do not copy OS direct RX or treat bounded pause as proven quiescence.
- Reuse scanner session only for bounded request/reply work; do not turn it into a generic UI/storage/streaming service.
- Use ENC operations through the instance mutex and respect shared-buffer lifetimes; do not revive a global bank cache or blanket SPI transformation.
- Treat receive filters as shared controller state needing explicit scoped ownership before multiple new filter-changing capabilities accumulate.
- Passive’s worker/handler/list/detail shell is promising for compatible bounded observation, but its fixed record/history/filter model is not automatically the owner for any future protocol.
- Sniffer is a constrained single-session capture base; the current generic PCAP analyzer is not a safe analysis base until F4-005–F4-007 are resolved and its session boundary is redesigned.
- Model precise network prerequisites—hardware, link, IPv4, gateway, multicast, or none—rather than copying `enc28j60_connected`/`is_dora` checks.
- Keep persistence semantics format-specific and reuse only proven I/O mechanics.
- Preserve feature-scoped allocation, one-worker coexistence, total-free versus largest-block guard semantics, and automatic-responder overlap in every resource model.
- Resolve or explicitly condition work on F4 correctness and SDK/HAL contracts before declaring an extension seam safe.
- Do not infer roadmap placement from historical scene/module names. Phase 8 supplies readiness constraints, not feature placement or priority.

With these constraints, the repository is analytically ready for Phase 9 to ask: given a historical capability, which current architectural seam can safely own it?
