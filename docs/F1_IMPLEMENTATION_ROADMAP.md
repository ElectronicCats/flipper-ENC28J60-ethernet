# FLIPPER ETHERNET — F1 Implementation Roadmap

## 1. Purpose and authority

The historical F0 phase is closed and is not reopened by this roadmap.

Capabilities and architectural mechanisms delivered or evolved from F0 are
treated as existing current architecture. This roadmap does not reimplement F0.1-F0.9.

When a future F1 capability depends on unresolved current correctness or
architectural debt in an existing F0-derived subsystem, that debt is represented
only as a prerequisite of the affected F1 work.

A prerequisite must not reimplement, restore, or redesign historical F0 work
unless current source evidence and an active current finding demonstrate that
the change is required.

This document reconstructs the historical F1 capability set against the current FLIPPER ETHERNET source and defines the proposed implementation roadmap. Historical identifiers `F1.1` through `F1.27` preserve lineage; roadmap foundations, waves, and `F1.x-A` implementation subphases define current execution structure.

Authority is, in order: current `EthernetAppDemo/` source and `application.fam`; [APP_BEHAVIOR.md](APP_BEHAVIOR.md); [ARCHITECTURE.md](ARCHITECTURE.md) and [HARDWARE.md](HARDWARE.md); [DECISIONS.md](DECISIONS.md) and [BACKLOG.md](BACKLOG.md); [ARCHITECTURE_EVOLUTION.md](ARCHITECTURE_EVOLUTION.md); [COMPLEXITY_AUDIT.md](COMPLEXITY_AUDIT.md); repository skill guidance; then the historical specification and master plan. This roadmap proposes future behavior. It does not change current behavior until a reviewed implementation subphase lands and canonical documentation is updated.

The historical F1 capability universe is adopted as the set to reconsider, not as an instruction to restore its UI, build model, order, or implementation assumptions. This document does not adopt F2, F3, OT, or drop-box work.

## 2. Historical F1 source model

The detailed source is `docs/superpowers/specs/2026-05-05-roadmap-design.md` §4. It defines exactly 27 consecutive items, `F1.1`–`F1.27`, with title, aggression class, effort, and a short requirement. `ENC28J60_REFACTOR_PLAN.md` §6 points to that table, repeats all 27 IDs in a proposed nine-group order, and assumes one PR per historical feature.

The sources agree on item identity and titles. They do not provide per-item acceptance criteria beyond the shared historical outcome “plug 60s → inventory + topology + PCAP + report.” The master plan is a summary rather than a second independent detailed definition.

Historical assumptions now superseded or invalid include:

- two admin/pentest FAP artifacts; current source builds one FAP with `PENTEST_MODE=1`;
- one scene/PR per item; current architecture already consolidates three items in Passive Discovery;
- a universal completed RX migration; OS and DHCP remain direct-RX exceptions;
- 6 KiB RX / 2 KiB TX controller SRAM; current layout is 3 KiB / 1.5 KiB;
- blanket bulk-SPI and old heap/stack estimates;
- “10/100” reporting in F1.23; ENC28J60 is 10BASE-T only;
- protocol-specific PMEN assumptions for CDP/EAPOL; current Passive reception uses the normal LLDP pattern plus scoped multicast acceptance.

These differences change implementation placement, not historical identifiers.

## 3. Reconciliation methodology

Each item has two independent decisions:

1. **Historical status:** IMPLEMENTED, PARTIALLY IMPLEMENTED, SUPERSEDED, OBSOLETE, HISTORICAL / NOT PREVIOUSLY ADOPTED, or CURRENT CANDIDATE.
2. **Current disposition:** NO NEW IMPLEMENTATION WORK, HARDEN / COMPLETE CURRENT IMPLEMENTATION, EXTEND EXISTING FEATURE, ADD MODE / SUBMODE, EXTEND SHARED BACKEND, NEW SHARED BACKEND, NEW STANDALONE USER FEATURE, MERGE / CONSOLIDATE, INFRASTRUCTURE PREREQUISITE, DEFER — BLOCKED, or DO NOT ADOPT.

The inventory records the durable placement disposition and a separate prerequisite/blocker gate. Any item naming an applicable unmet PR-*/F4/external prerequisite is also **DEFER — BLOCKED for immediate feature implementation** until that exact gate is satisfied. This preserves the intended owner without pretending the item can start now.

Placement starts from capability semantics, exact network prerequisites, lifetime, and data identity—not historical filenames. Every future item must use registered RX or bounded `scanner_session` exchange unless it proves another exclusive model; use at most one owner-tagged app worker; state filter ownership/restoration; bound data and allocations; and preserve cancellation, unregister, join, and teardown ordering.

Protocol requirements not established by repository evidence are marked **EXTERNAL STANDARD / RESEARCH REQUIRED**. SDK/HAL/filesystem guarantees are not invented.

## 4. Current architecture and readiness constraints

Strong boundaries that future work should reuse are `AppThreadOwner`, the ENC instance/mutex operation boundary, and bounded registered RX callbacks. Scanner session, separate scenes, Sniffer capture, Passive runtime/history, and startup guards are usable with their documented constraints.

The following must not become growth foundations without prerequisite work:

- OS/DHCP receive exceptions and implicit RX callback rules;
- split network state, target editing, and protocol-index validation;
- feature-local receive-filter transitions;
- worker-side GUI/cancellation uncertainty;
- unsafe persistence mechanics;
- the current PCAP reader/analyzer;
- substantial expansion of fixed `neighbor_t`/Passive History.

Resource rules remain: 24 KiB main stack; 4 KiB RX Dispatch stack; one app-owned feature worker by default; shared 1,518-byte ENC RX/TX buffers; permanent responders may overlap a feature; Passive DB and PCAP index remain feature-scoped; total free memory and largest contiguous block are separate; startup guards are advisory. No leak, permanent fragmentation, or stack overflow is assumed.

## 5. Historical F1 inventory

### 5.1 Source-extracted capability inventory

The specification supplies no item-specific acceptance tests, explicit optional/stretch split, or detailed inputs/outputs. All rows inherit the phase-wide end-to-end demo criterion. “None stated” below means the source was silent; it is not a current rejection.

| ID | Exact historical title | Aggr/effort | Stated input | Stated output/behavior | Stated dependency or implementation assumption | Optional/stretch | Source |
|---|---|---|---|---|---|---|---|
| F1.1 | LLDP passive harvest | A1/S | Received LLDP | Chassis, port, name, management, VLAN, PoE | PMEN EtherType `0x88CC` | None stated | Spec §4; plan §F1 group 1 |
| F1.2 | CDP passive harvest | A1/S | Cisco multicast CDP | Passive CDP harvest | `01:00:0C:CC:CC:CC`; “same pattern-match” | None stated | Spec §4; plan §F1 group 1 |
| F1.3 | 802.1X EAPOL recon | A1/S | EtherType `0x888E` | EAPOL-Start and EAP identity | Pattern match | None stated | Spec §4; plan §F1 group 1 |
| F1.4 | Native-VLAN / mgmt-VLAN sniffer | A1/S | Observed 802.1Q traffic | List all seen VLAN IDs | Passive capture | None stated | Spec §4; plan group 8 |
| F1.5 | mDNS/Bonjour passive harvest | A1/S | UDP/5353 DNS-SD | Enumerate named device/service classes | `_services._dns-sd._udp.local` decoding | None stated | Spec §4; plan group 2 |
| F1.6 | DHCP option-55 fingerprint | A1/S | DHCP option 55 | Known OS mapping | Existing DHCP module | None stated | Spec §4; plan §F1 group 2 |
| F1.7 | DHCP client tracker continuo | A1/S | Seen DHCP DISCOVERs | Long-running client log | Passive observation | None stated | Spec §4; plan §F1 group 2 |
| F1.8 | SSDP / UPnP discovery (M-SEARCH) | A2/S | M-SEARCH/replies | Routers/TVs/NAS/IoT enumeration | Active multicast discovery | None stated | Spec §4; plan group 8 |
| F1.9 | Traceroute (ICMP TTL + UDP variants) | A2/S | Target and TTL probes | Hop path | Reuse `protocol_tools/icmp.c` | Two probe variants required | Spec §4; plan group 3 |
| F1.10 | Reverse DNS sweep | A2/S | Every ARP-scan IP | PTR names | ARP scan result set | None stated | Spec §4; plan §F1 group 3 |
| F1.11 | Wake-on-LAN sender | A2/S | Target, broadcast/directed choice | Magic packet sent | UDP magic packet | None stated | Spec §4; plan §F1 group 3 |
| F1.12 | WoL passive capture | A1/S | Observed WoL traffic | Captured/detected magic packet | ENC MPEN | None stated | Spec §4; plan §F1 group 3 |
| F1.13 | Banner grab TCP (HTTP/SSH/SMTP/FTP) | A2/S | Target TCP services | Greeting/banner log | Connect then read | Four named protocols | Spec §4; plan group 4 |
| F1.14 | Payload-aware UDP service scan | A2/M | DNS/SNMP/NTP/NetBIOS/mDNS/SSDP probes | Service-specific responses | Replace generic `udp_port_scan` | All named profiles comprise scope | Spec §4; plan §F1 group 4 |
| F1.15 | SYN scan (half-open) | A2/S | TCP target/range | Open-port result | Replace/contrast historical connect scan | None stated | Spec §4; plan §F1 group 4 |
| F1.16 | FIN/NULL/Xmas scans (fix Xmas roto) | A2/S | TCP target/range and flag mode | OS-sensitive scan results | Fix historical Xmas sender | Three named modes | Spec §4; plan §F1 group 4 |
| F1.17 | SNMP community brute (public/private/wordlist) | A2/S | Target and candidate communities | Working community result | UDP/161; local dictionary | Built-ins plus wordlist | Spec §4; plan group 5 |
| F1.18 | SNMP v1/v2c walk (ifTable, MAC table, IP-MIB) | A2/M | Agent/community/MIB requests | Switch/topology data | SNMP v1/v2c | Three named MIB families | Spec §4; plan §F1 group 5 |
| F1.19 | Top-talkers / heat-map en pantalla | A1/M | Live traffic | On-screen bar chart | Custom Widget canvas | None stated | Spec §4; plan group 6 |
| F1.20 | Per-host packet/byte counters time-series | A1/S | Live RX frames | Host counters over time | RX Dispatch | None stated | Spec §4; plan §F1 group 6 |
| F1.21 | Asset inventory CSV/JSON export | A1/S | IP/MAC/name/OS/service findings | SD CSV/JSON | Cross-feature inventory | Both formats named | Spec §4; plan §F1 group 6 |
| F1.22 | PCAP live-write con BPF-lite filter | A1/M | Broadcast/IP/port filter and live frames | Filtered PCAP | Pre-capture filter screen | Three filter categories | Spec §4; plan group 7 |
| F1.23 | Link integrity diagnostics | A1/S | PHY/link registers | Up/down, 10/100, duplex, errata data | ENC PHY registers | None stated | Spec §4; plan group 8 |
| F1.24 | Stealth mode (TX disable) | A1/S | User mode selection | TX-disabled passive operation | `PHCON2.TXDIS` | None stated | Spec §4; plan §F1 group 8 |
| F1.25 | Passive OS fingerprint (p0f-style) | A1/M | Observed traffic | Passive OS inference | Complements active detector | None stated | Spec §4; plan group 9 |
| F1.26 | On-device PCAP analyzer | A1/M | Stored PCAP | Filters, FTP/HTTP credential-related and TLS metadata | Improve existing viewer | Named analysis types | Spec §4; plan group 7 |
| F1.27 | PCAP-to-report auto-summary | A1/M | Stored PCAP | Hosts/services/credentials/DNS summary | PCAP analysis | Named report fields | Spec §4; plan §F1 group 7 |

### 5.2 Current reconciliation inventory

| ID | Historical capability | Original intent | Historical status | Current implementation | Current disposition | Current owner | Major blockers | Wave |
|---|---|---|---|---|---|---|---|---|
| F1.1 | LLDP passive harvest | Decode chassis/port/name/management/VLAN/PoE | IMPLEMENTED / decomposition SUPERSEDED | Passive LLDP mode, DB/details/history | NO NEW IMPLEMENTATION WORK | Passive Discovery | Existing F4-023/024 are independent hardening | Current |
| F1.2 | CDP passive harvest | Observe Cisco CDP multicast | IMPLEMENTED / decomposition SUPERSEDED | Passive CDP mode, DB/details/history | NO NEW IMPLEMENTATION WORK | Passive Discovery | Existing F4-023/024 | Current |
| F1.3 | 802.1X EAPOL recon | Detect EAPOL-Start and EAP identity | IMPLEMENTED / decomposition SUPERSEDED | Passive EAPOL mode, DB/details/history | NO NEW IMPLEMENTATION WORK | Passive Discovery | Existing F4-023/024 | Current |
| F1.4 | Native-/management-VLAN sniffer | List observed 802.1Q VLAN IDs | PARTIALLY IMPLEMENTED | LLDP VLAN fields only; no traffic-wide VLAN inventory | MERGE / CONSOLIDATE | New Traffic Monitor, VLAN mode | PR-01/03/04, F4-034 | 3 |
| F1.5 | mDNS/Bonjour passive harvest | Discover DNS-SD services and products | NOT PREVIOUSLY ADOPTED | No mDNS/DNS-SD implementation | MERGE / CONSOLIDATE | New Service Discovery, mDNS mode | PR-01/03/04; standards | 2 |
| F1.6 | DHCP option-55 fingerprint | Map client parameter-request lists to OS families | NOT PREVIOUSLY ADOPTED | DHCP client/parser exists; no passive option-55 fingerprint | MERGE / CONSOLIDATE | Traffic Monitor, DHCP mode | PR-01/03/04; signature provenance | 3 |
| F1.7 | DHCP client tracker continuous | Long-running DISCOVER client log | NOT PREVIOUSLY ADOPTED | PCAP analyzer labels DHCP; no tracker | MERGE / CONSOLIDATE | Traffic Monitor, DHCP mode | PR-01/03/04/05 | 3 |
| F1.8 | SSDP/UPnP discovery | Send M-SEARCH and enumerate devices | NOT PREVIOUSLY ADOPTED | None | MERGE / CONSOLIDATE | Service Discovery, SSDP mode | PR-01/02/03/04; standards | 2 |
| F1.9 | Traceroute ICMP/UDP | Produce ordered hop path | NOT PREVIOUSLY ADOPTED | Ping/ICMP and scanner primitives only | ADD MODE / SUBMODE | Ping Host, Trace Route mode | PR-01/02/03; standards | 2 |
| F1.10 | Reverse DNS sweep | PTR-enrich ARP-scan hosts | NOT PREVIOUSLY ADOPTED | Scan Hosts history exists; no DNS | EXTEND EXISTING FEATURE | Scan Hosts via Service Discovery backend | PR-01/02/03/05 | 2 |
| F1.11 | Wake-on-LAN sender | Broadcast/directed magic packet | NOT PREVIOUSLY ADOPTED | Packet tools only; no WoL | EXTEND EXISTING FEATURE | Scan Hosts host action plus direct target | PR-02/03; standards | 2 |
| F1.12 | WoL passive capture | Detect magic packets, historically via MPEN | NOT PREVIOUSLY ADOPTED | `ERXFCON_MPEN` constant only | MERGE / CONSOLIDATE | Traffic Monitor, WoL mode | PR-01/03/04 | 3 |
| F1.13 | Banner grab TCP | Connect/read bounded HTTP/SSH/SMTP/FTP greeting | PARTIALLY IMPLEMENTED | TCP builders and uncalled legacy handshake; no banner flow | ADD MODE / SUBMODE | Ports Scanner, Service mode | PR-01/02/03; S3; standards | 2 |
| F1.14 | Payload-aware UDP service scan | Protocol probes replacing generic UDP scan | PARTIALLY IMPLEMENTED / CURRENT CANDIDATE | Dormant generic UDP branch lacks service semantics | HARDEN / COMPLETE CURRENT IMPLEMENTATION | Ports Scanner via Service Probe backend | F4-009/010; PR-01/02/03; S2 | 2 |
| F1.15 | SYN scan (half-open) | SYN/SYN-ACK open-port scan | PARTIALLY IMPLEMENTED | Active TCP Ports scan sends SYN and records SYN-ACK | HARDEN / COMPLETE CURRENT IMPLEMENTATION | Ports Scanner | PR-01/02/03, F4-029; standards | 2 |
| F1.16 | FIN/NULL/Xmas scans | TCP flag-scan modes and classifiers | PARTIALLY IMPLEMENTED | Probe builders used by OS Detector, not port-scan modes | ADD MODE / SUBMODE | Ports Scanner | PR-01/02/03; S3; standards | 2 |
| F1.17 | SNMP community brute | Test bounded community candidates | NOT PREVIOUSLY ADOPTED | None | MERGE / CONSOLIDATE | New SNMP Tools, Community Audit mode | PR-01/02/03/05; S6; standards | 4 |
| F1.18 | SNMP v1/v2c walk | Read bounded ifTable/MAC/IP-MIB results | NOT PREVIOUSLY ADOPTED | None | MERGE / CONSOLIDATE | SNMP Tools, Walk mode | Same plus bounded data model | 4 |
| F1.19 | Top-talkers/heat-map | Live ranked traffic visualization | NOT PREVIOUSLY ADOPTED | Sniffer has only aggregate packet count | MERGE / CONSOLIDATE | Traffic Monitor, Top Talkers mode | PR-01/03/04; S1 | 3 |
| F1.20 | Per-host counters/time-series | Host packet/byte metrics over RX Dispatch | NOT PREVIOUSLY ADOPTED | None | NEW SHARED BACKEND | Traffic Monitor observation engine | PR-01/03/04; bounded memory | 1/3 |
| F1.21 | Asset inventory CSV/JSON export | Export IP/MAC/name/OS/services | NOT PREVIOUSLY ADOPTED | Results are dispersed; no export | NEW STANDALONE USER FEATURE | Asset Inventory backed by S4 | PR-05; producer schemas; storage | 4 |
| F1.22 | PCAP live-write with BPF-lite filter | Filtered capture by broadcast/IP/port | PARTIALLY IMPLEMENTED | Sniffer writes PCAP but has no prefilter UI | ADD MODE / SUBMODE | Packet Sniffer | PR-03/04/06, F4-008/029 | 3 |
| F1.23 | Link integrity diagnostics | Link/duplex/error diagnostics; historical 10/100 | PARTIALLY IMPLEMENTED; 10/100 clause OBSOLETE | Live link test only | NEW STANDALONE USER FEATURE | Administration: Link Diagnostics | PR-02/04; PHY/errata research | 2 |
| F1.24 | Stealth mode (TX disable) | Scoped receive-only capture via `PHCON2.TXDIS` | NOT PREVIOUSLY ADOPTED | Register bit defined, no API/mode | ADD MODE / SUBMODE | Packet Sniffer, Stealth Capture | PR-03/04; HAL/controller contract | 3 |
| F1.25 | Passive OS fingerprint | p0f-style traffic-derived OS hints | NOT PREVIOUSLY ADOPTED | Active OS Detector only | MERGE / CONSOLIDATE | Traffic Monitor, Passive OS mode | PR-01/03/04; S1; signature research | 3 |
| F1.26 | On-device PCAP analyzer | Filters plus bounded protocol/TLS metadata analysis | PARTIALLY IMPLEMENTED | Viewer/index/generic analyzer exist but are unsafe | HARDEN / COMPLETE CURRENT IMPLEMENTATION | View Packets | PR-06, F4-005–007/026–031 | 5 |
| F1.27 | PCAP-to-report summary | Summarize hosts/services/DNS and approved metadata | NOT PREVIOUSLY ADOPTED | None | EXTEND EXISTING FEATURE | View Packets via S4/S5 | PR-05/06; F1.26 | 5 |

No complete historical item is rejected. The obsolete F1.23 “10/100” subrequirement is explicitly not adopted because the controller is 10BASE-T only. Historical implementation assumptions such as dual artifacts and independent LLDP/CDP/EAPOL scenes are likewise not adopted.

## 6. Prerequisite gates

PR-* identifiers are current roadmap prerequisite gates, not invented F1 identifiers and not a continuation or reopening of historical F0.

| ID | Foundation | Scope and completion gate | Unblocks |
|---|---|---|---|
| PR-01 | Unified RX ownership and callback contract | Resolve F4-001–004; define bounded callback/reentrancy rules; OS and DHCP no longer rely on unsafe competing FIFO ownership; static and physical transition tests pass | All new RX consumers, especially F1.4–10, F1.12–20, F1.22, F1.25 |
| PR-02 | Typed transactional network/target configuration | Resolve F4-009 and F4-011–016; explicit controller/link/IP/gateway prerequisites; validated protocol selection; edit commit/cancel; persistence timing documented | All active probes and diagnostics |
| PR-03 | Worker, cancellation, GUI-event, and guard launch contract | Resolve/establish F4-027–029 and relevant F4-031 contracts; main-thread UI mutation; owner-tagged start/cancel/unblock/join; total/max-block guards retained | Every new worker-backed feature/mode |
| PR-04 | Scoped receive-filter and PHY-mode ownership | One owner/snapshot/restore contract for multicast, promiscuous, broadcast, pattern/magic, and TX-disable state; resolve relevant F4-032/034 external contracts | Traffic Monitor, Service Discovery multicast, filtered/stealth capture, link diagnostics |
| PR-05 | Reliable bounded persistence/export mechanics | Define exact I/O, versioning, replacement, failure propagation, and bounded record rules; resolve dependent F4-017–024 and F4-033 before new persistent stores | DHCP history, inventory/export, reports |
| PR-06 | Reliable capture-session prerequisite | Resolve F4-008 for live capture; define explicit capture-session ownership, exact write/error propagation, counter/error semantics, file close behavior, and capture cleanup without expanding the offline reader/analyzer | F1.22, F1.24 |
| PR-07 | Safe PCAP reader/analyzer prerequisite | Resolve F4-005–007; validate PCAP global/record headers and lengths before indexing or parsing; separate validated packet representation from rendering; define explicit reader/analyzer session ownership; re-evaluate the 16 KiB index | F1.26, F1.27 |

PR-* identifiers are dependency gates, not a mandatory implementation phase.

A PR-* gate is evaluated only when required by an approved F1 subphase. Unrelated PR-* gates do not block that subphase.

For a particular dependent subphase, a PR-* gate is classified as:

- **NOT EVALUATED** — its current state has not yet been checked for that work.
- **SATISFIED** — current source already provides the required contract.
- **PARTIALLY SATISFIED** — only bounded prerequisite work is required.
- **BLOCKING** — required prerequisite work must be completed before the dependent F1 subphase.
- **NOT APPLICABLE** — the gate does not apply to that subphase.

Naming a PR-* gate does not by itself authorize or require code changes. Current source must first be inspected to determine whether prerequisite work is actually necessary.

When prerequisite correction is required, it must be planned and reviewed as bounded work of its own and must not be hidden inside the dependent F1 implementation diff. Any later PR-xx-A/PR-xx-B decomposition is derived from current source when that gate becomes blocking; this roadmap does not predefine those correction subphases.

## 7. Cross-F1 shared infrastructure

| ID | Shared infrastructure | Consumers | Boundary |
|---|---|---|---|
| S1 | Bounded live observation engine | F1.4, F1.6, F1.7, F1.12, F1.19, F1.20, F1.25 | One worker/registration, classifier/aggregator hooks, bounded feature-scoped records; not `neighbor_t` and not PCAP parsing |
| S2 | Service discovery/probe registry | F1.5, F1.8, F1.10, F1.14; DNS portions of F1.27 | Bounded protocol profiles, request/reply correlation, normalized service results; not a general daemon |
| S3 | TCP probe/session backend | F1.13, F1.15, F1.16 | Registered response matching, flag classification, optional bounded stream greeting; no legacy direct RX |
| S4 | Asset catalog/report model | F1.21, F1.27; outputs from Scan Hosts, Ports, OS, S1/S2/S6 | Explicit provenance and identity merge, bounded feature-scoped data, versioned export; not an expansion of `App.ip_list` |
| S5 | Validated PCAP packet representation | F1.26, F1.27 | Parser-owned bounds-checked representation separated from UI text |
| S6 | SNMP codec/session | F1.17, F1.18; SNMP profile in F1.14 | Bounded BER/SNMP encode/decode and request correlation; wordlist/walk policies remain feature-owned |

### 7.1 Shared-backend bootstrap rule

S1–S6 describe shared architectural responsibilities, not a mandatory infrastructure implementation phase.

Shared backends are instantiated just-in-time by their first approved consumer. Do not implement S1–S6 speculatively as a complete infrastructure wave.

The first approved F1 subphase that requires an S* backend establishes only the minimum reusable contract required by the currently known consumers while preserving the cross-F1 responsibilities defined in this roadmap.

Subsequent consumers reuse or deliberately extend that shared backend rather than create parallel feature-local implementations.

If current source already satisfies part or all of an S* responsibility, reuse that implementation instead of recreating it.

### 7.2 Adoption and placement rationale

| Modern capability family | Why it belongs in the current roadmap | Why this placement is correct now |
|---|---|---|
| Existing Passive Discovery | It already delivers the original LLDP/CDP/EAPOL reconnaissance value | One shared link-observation lifecycle is proven; reopening three features would add duplication |
| Traffic Monitor / S1 | Seven historical items require bounded aggregation over a live frame stream | One scoped registered consumer prevents seven workers/filters and avoids forcing event/counter data into `neighbor_t` |
| Service Discovery / S2 | mDNS, SSDP, PTR, and UDP service profiles all need safe name/message parsing and normalized service findings | Shared codecs/results prevent duplicated DNS/multicast/request correlation while keeping active and passive modes explicit |
| Ping/Ports/Scan Hosts extensions | Traceroute, WoL, banners, TCP flags, UDP services, and rDNS share existing target/result workflows | Modes/actions preserve user context and avoid unjustified top-level scenes |
| SNMP Tools / S6 | Community audit and walk share one authenticated target/session and codec | Their credential/rate/table lifecycle is too distinct for generic Ports UI but should not be duplicated across two features |
| Link Diagnostics | Hardware-only inspection remains useful without IP or packet ownership | A small standalone workflow avoids conflating link, controller, and DHCP state |
| Asset Inventory / S4 | The phase-level inventory/export goal needs structured cross-feature provenance | A dedicated browse/export owner prevents permanent growth of each producer and avoids scraping presentation strings |
| Sniffer/View Packets evolution | Filtered capture and safe offline analysis/reporting are explicit historical goals and extend existing user workflows | Sniffer owns live capture; S5/View Packets owns offline parsing; keeping them separate preserves resource/lifetime rules |

## 8. Dependency graph

Legend: `-->` hard dependency; `-.->` soft/reuse dependency; `[C]` correctness blocker; `[E]` external contract; `[S]` shared infrastructure.

```text
current source / active F4 / external contracts
                    |
                    v
          applicable PR-* gate
                    |
                    v
       required S* responsibility
             (if applicable)
                    |
                    v
             F1.x subphase


[C] F4-001..004 --> PR-01 RX ownership
[C] F4-009,011..016 --> PR-02 network/config
[E] F4-029,031 --> PR-03 worker/UI/cancel/guards
[E] F4-032,034 --> PR-04 filter/PHY ownership
[C/E] F4-017..024,033 --> PR-05 storage/export
[C] F4-008 --> PR-06 capture session
[C] F4-005..007 --> PR-07 PCAP reader/analyzer

PR-01 + PR-03 + PR-04 --> [S] S1 observation responsibility
    S1 --> F1.4, F1.6, F1.7, F1.12, F1.19, F1.20, F1.25

PR-01 + PR-02 + PR-03 --> [S] S2 service-probe responsibility
    S2 --> F1.5, F1.8, F1.10, F1.14

PR-01 + PR-02 + PR-03 --> [S] S3 TCP-probe responsibility
    S3 --> F1.13, F1.15, F1.16

S2 --> [S] S6 SNMP responsibility --> F1.17, F1.18
Scan Hosts/Ports/OS + S1 + S2 + S6 -.-> [S] S4 --> F1.21

PR-07 --> [S] S5 --> F1.26 --> F1.27
S5 + S4 --> F1.27

PR-02 + scanner_session --> F1.9, F1.11
PR-02 + PR-04 + PHY research --> F1.23
PR-03 + PR-04 + PR-06 --> F1.22, F1.24
```

Dependencies are evaluated per execution chain. There is no requirement to complete every PR-* gate or every S* responsibility before beginning unrelated F1 work. The selected subphase's hard dependencies take precedence over wave number.

## 9. Recommended implementation waves

### Current — reconciled, no new feature work

F1.1, F1.2, and F1.3 remain owned by Passive Discovery. Existing F4 hardening is handled through the backlog rather than reopening historical feature decomposition.

### Prerequisite layer — evaluated per execution chain

PR-01 through PR-07 are prerequisite gates, not a global implementation wave. Evaluate only the gates required by the selected F1 subphase. A satisfied or non-applicable gate requires no implementation work; a blocking gate is corrected separately before that dependent subphase proceeds.

### Shared-backend layer — instantiated by approved consumers

S1–S6 are shared architectural responsibilities instantiated just-in-time by approved F1 consumers. They are not implemented speculatively as a complete infrastructure wave. The first consumer establishes the minimum reusable contract required by the known dependent capabilities; later consumers reuse or deliberately extend that contract.

### Wave 2 — active diagnostics and service discovery

F1.5, F1.8–F1.11, F1.13–F1.16, and F1.23. These reuse typed network state, scanner request/reply, S2/S3, or the ENC driver. They do not require long-running broad capture.

### Wave 3 — live observation and capture modes

F1.4, F1.6, F1.7, F1.12, F1.19, F1.20, F1.22, F1.24, and F1.25. They share S1 or Sniffer and therefore wait for explicit filter ownership, worker/callback contracts, and bounded feature-scoped records.

### Wave 4 — SNMP and consolidated inventory

F1.17, F1.18, and F1.21. SNMP follows the common service transport/result contracts; Asset Inventory follows stable structured producers and reliable export rather than scraping UI strings.

### Wave 5 — validated stored-capture analysis and reporting

F1.26 then F1.27. They wait for PR-06/S5 because current PCAP parsing is a correctness blocker and redesign-first seam. Report generation then reuses S4 rather than inventing a second asset model.

This order is dependency-driven. Historical numeric order would interleave passive filters, active probes, new persistence, SNMP, live analytics, and unsafe PCAP expansion before their shared contracts exist.

Implementation waves are planning/readiness bands, not global completion barriers.

An F1 subphase may begin as soon as its own hard dependencies are satisfied. Completion of unrelated PR-* gates, S* responsibilities, or earlier-wave capabilities is not required.

A later-wave capability whose hard dependencies are satisfied is not blocked merely because unrelated earlier-wave work remains incomplete.

## 10. Dossier conventions

Each dossier has 22 numbered fields. Subphase tables use these compressed columns:

- **Prereq/areas:** applicable PR-* gates, required S* responsibilities, and likely current/new source areas.
- **Invariant/no shortcut:** ownership or behavior to preserve and prohibited architecture.
- **Resource:** stack/heap/registry/file effects; exact SDK sizes remain unresolved.
- **Acceptance/validation:** source/build/physical/malformed/cancel/transition checks without CLI logging.
- **Docs/gate:** canonical documents to update and the condition for accepting that subphase.

## 11. F1.1 — LLDP passive harvest

1. **Historical intent:** Required chassis ID, port ID, system name, management address, VLAN, and PoE TLVs. PMEN EtherType matching was an implementation assumption, not the user capability.
2. **Historical reconciliation status:** IMPLEMENTED CAPABILITY / SUPERSEDED FEATURE DECOMPOSITION. `lldp_parse()` and `lldp_fill_neighbor()` populate the required fields; the Passive LLDP mode displays and saves them.
3. **Current roadmap disposition:** NO NEW IMPLEMENTATION WORK. Do not recreate an LLDP-only worker or scene.
4. **Current capability mapping:** `lldp.c`, `lldp_module.c`, Passive handler table, `neighbor_db`, three Passive scenes, and Passive History.
5. **Capability gap:** No material historical field gap found. Existing presentation/history failure concerns are current backlog work, not missing F1.1.
6. **Architectural placement:** Existing Passive Discovery protocol mode and shared `(source MAC, protocol)` observation model.
7. **RX model:** Existing bounded Passive registered observation; no direct RX.
8. **TX model:** None.
9. **Worker/lifecycle:** Existing single 3 KiB Passive worker; scene stop, unregister, filter restore, merge, join, release.
10. **Filter impact:** Existing scoped multicast addition plus normal baseline PMEN. The historical dedicated PMEN formulation is superseded.
11. **Network prerequisites:** ENC available, PHY link, multicast reception; no configured IPv4 or gateway.
12. **Data model/identity:** LLDP observation keyed by source MAC plus LLDP source type.
13. **Persistence/export:** Existing versioned/checksummed Passive History.
14. **UI/scene:** Existing Passive LLDP/Discover All list and details; no new UI.
15. **Resource model:** Existing ~15.5 KiB feature-scoped DB, one RX slot per wait, Passive worker, bounded 32 records.
16. **Correctness/backlog:** F4-023/024/026/028/033 constrain existing hardening but do not reopen capability status.
17. **Phase 8 dependency:** Passive runtime is EXTEND WITH CURRENT CONSTRAINTS; no extension is proposed here.
18. **External research/contracts:** Standards research only if later changing decoder coverage; filesystem/scene contracts remain current F4 items.
19. **Implementation subphases:** None. Historical reconciliation is the completion record.
20. **Acceptance:** Current source continues to decode, display, save, reopen, filter, clear, cancel, restore filters, and release resources under existing canonical behavior.
21. **Regression surface:** Passive CDP/EAPOL coexistence, history compatibility, filter baseline, resource guards.
22. **Summary:** Status IMPLEMENTED/SUPERSEDED; disposition NO NEW WORK; owner Passive Discovery; wave Current; confidence high.

## 12. F1.2 — CDP passive harvest

1. **Historical intent:** Observe Cisco CDP at `01:00:0C:CC:CC:CC`; “same pattern-match” was historical implementation guidance.
2. **Historical reconciliation status:** IMPLEMENTED CAPABILITY / SUPERSEDED FEATURE DECOMPOSITION. Current CDP validates LLC/SNAP/checksum/TLVs and produces source-aware Passive observations.
3. **Current roadmap disposition:** NO NEW IMPLEMENTATION WORK.
4. **Current capability mapping:** `cdp.c`, `cdp_module.c`, Passive handler table, DB/details/history.
5. **Capability gap:** None material to the brief historical requirement. Native-VLAN/duplex enum values not currently decoded belong only if separately required and researched.
6. **Architectural placement:** Existing Passive CDP mode, not a standalone feature.
7. **RX model:** Existing Passive registered observation.
8. **TX model:** None.
9. **Worker/lifecycle:** Existing Passive worker and teardown.
10. **Filter impact:** Scoped multicast (`MCEN`); historical PMEN-only assumption superseded.
11. **Network prerequisites:** ENC and link; no IPv4/gateway.
12. **Data model/identity:** Source MAC plus CDP protocol; device/port/platform/software/management/capability metadata.
13. **Persistence/export:** Existing Passive History.
14. **UI/scene:** Existing CDP mode and detail pages.
15. **Resource model:** Shares current Passive bound; no independent worker or DB.
16. **Correctness/backlog:** F4-023/024/026/028/033 as existing constraints.
17. **Phase 8 dependency:** Existing compatible Passive observation seam; fixed data model is not expanded.
18. **External research/contracts:** Only for future TLV scope changes.
19. **Implementation subphases:** None.
20. **Acceptance:** Current CDP mode remains bounded, source-aware, cancellable, persistent, and filter-restoring.
21. **Regression surface:** LLDP/EAPOL shared paths and history compatibility.
22. **Summary:** IMPLEMENTED/SUPERSEDED; NO NEW WORK; Passive Discovery; Current; high confidence.

## 13. F1.3 — 802.1X EAPOL reconnaissance

1. **Historical intent:** Detect EAPOL-Start and EAP identity; EtherType pattern matching was the historical implementation assumption.
2. **Historical reconciliation status:** IMPLEMENTED CAPABILITY / SUPERSEDED FEATURE DECOMPOSITION. Current parser recognizes EAPOL types, EAP codes/types, and bounded identity metadata without retaining key material.
3. **Current roadmap disposition:** NO NEW IMPLEMENTATION WORK.
4. **Current capability mapping:** `eapol.c`, `eapol_module.c`, Passive handler/DB/details/history.
5. **Capability gap:** No material gap found for Start/identity observation.
6. **Architectural placement:** Existing Passive EAPOL mode.
7. **RX model:** Existing Passive registered observation.
8. **TX model:** None.
9. **Worker/lifecycle:** Existing Passive worker and cleanup.
10. **Filter impact:** Scoped multicast, not a separate EAPOL filter owner.
11. **Network prerequisites:** ENC/link/multicast; no IP.
12. **Data model/identity:** Source MAC plus EAPOL protocol; bounded identity/version/type/code metadata.
13. **Persistence/export:** Existing Passive History.
14. **UI/scene:** Existing mode and details.
15. **Resource model:** Shares Passive DB/worker/slot limits.
16. **Correctness/backlog:** Existing Passive F4 constraints only.
17. **Phase 8 dependency:** Compatible current Passive observation; no data-model expansion.
18. **External research/contracts:** Required only before broadening EAP/EAPOL interpretation; no credential/key collection is implied.
19. **Implementation subphases:** None.
20. **Acceptance:** Start, Logoff, Key metadata, EAP identity and method observations remain bounded and history-compatible.
21. **Regression surface:** Other Passive protocols and shared history.
22. **Summary:** IMPLEMENTED/SUPERSEDED; NO NEW WORK; Passive Discovery; Current; high confidence.

## 14. F1.4 — Native-VLAN / management-VLAN sniffer

1. **Historical intent:** Passively decode 802.1Q tags and list every VLAN ID observed. LLDP-specific VLAN fields were not sufficient.
2. **Historical reconciliation status:** PARTIALLY IMPLEMENTED. LLDP stores PVID/named/network-policy VLAN; no traffic-wide 802.1Q inventory exists.
3. **Current roadmap disposition:** MERGE / CONSOLIDATE into the new Traffic Monitor as a VLAN mode.
4. **Current mapping:** LLDP VLAN parser/detail code is reusable evidence; Sniffer can receive broad traffic; no generic bounded VLAN aggregator/UI exists.
5. **Gap:** Validated single/double tag decoding as scoped, bounded unique VLAN counters, tagged/untagged statistics, and result UI.
6. **Placement:** S1 classifier/aggregator with Traffic Monitor mode. It does not belong in fixed `neighbor_t` because identity is VLAN observation, not neighbor.
7. **RX:** One S1 RX registration; bounded callback extracts validated metadata and defers UI.
8. **TX:** None.
9. **Worker/lifecycle:** One Traffic Monitor `AppThreadOwner`; stop → unregister/quiesce → join → free records.
10. **Filter:** Broad/multicast needs derive from observation requirements and use PR-04 scoped ownership/restoration.
11. **Network prerequisites:** ENC and link only; no IPv4/gateway.
12. **Data/identity:** VLAN ID plus tag depth/optional outer-inner relationship; bounded counters, not a Passive neighbor.
13. **Persistence:** Session-only initially; explicit export may later feed S4. No automatic file is required by historical intent.
14. **UI:** New Traffic Monitor top-level workflow, VLAN mode, bounded ranked/list result view.
15. **Resources:** One worker/slot; bounded table sized only after source-level memory model; no coexistence with another app worker; responders may overlap.
16. **Backlog:** PR-01/03/04; F4-027–029/032/034.
17. **Phase 8:** S1 is NEW SHARED BACKEND built on registered RX; Passive DB growth is REDESIGN-FIRST and intentionally avoided.
18. **External:** IEEE 802.1Q/QinQ scope and runt-frame delivery require research/contract confirmation.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.4-A | Freeze tag scope and bounded VLAN result contract | PR-01/03/04; protocol tools/S1 design | Validate lengths; do not reuse `neighbor_t` | Source-sized table budget | Parser vectors for tagged/untagged/truncated frames | Architecture/behavior proposal approved |
| F1.4-B | Add S1 VLAN classifier/aggregator | S1 backend, RX Dispatch | Callback bounded; no direct RX/UI mutation | One slot, feature table | Build, malformed frames, cancel/repeat/filter restore | Source and resource review pass |
| F1.4-C | Expose VLAN mode and results | Traffic Monitor scenes | One owner-tagged worker; BACK semantics explicit | GUI data bounded | Physical start/stop/BACK and cross-feature tests | APP_BEHAVIOR/ARCHITECTURE updated; accepted |

20. **Acceptance:** Observed tags are counted accurately within documented bounds; malformed frames are rejected; cancellation and filter restoration are repeatable.
21. **Regression:** Sniffer/Passive filters, RX callback latency, automatic responders, feature-worker ownership.
22. **Summary:** PARTIAL; MERGED Traffic Monitor mode; PR-01/03/04; Wave 3; medium-high confidence.

## 15. F1.5 — mDNS/Bonjour passive harvest

1. **Historical intent:** Passively decode UDP/5353 DNS-SD service enumeration and identify printers, AirPlay/AirDrop, Chromecast, and similar services.
2. **Historical reconciliation status:** HISTORICAL / NOT PREVIOUSLY ADOPTED. No current DNS/mDNS code exists.
3. **Current disposition:** MERGE / CONSOLIDATE as the mDNS mode of a new Service Discovery feature sharing S2.
4. **Current mapping:** No parser/builder. RX Dispatch and filter lifecycle are reusable; Passive neighbor/history model is not.
5. **Gap:** Bounded DNS name decoding with compression safety, PTR/SRV/TXT/A/AAAA correlation, service identity, passive collection, presentation.
6. **Placement:** S2 service-result model and Service Discovery scene family. Not Passive Discovery because identity/lifetime are service instances/records rather than link neighbors.
7. **RX:** Registered multicast UDP consumer; passive mode sends nothing.
8. **TX:** None for F1.5. Any later active browse is a separate reviewed extension.
9. **Worker/lifecycle:** One Service Discovery worker; unregister/quiesce before result destruction.
10. **Filter:** Multicast ownership through PR-04, restored on every exit.
11. **Network prerequisites:** ENC/link and multicast; passive harvest does not require configured IPv4 for observation, though address interpretation may.
12. **Data/identity:** `(service instance, service type, interface/source)` with bounded related host/address/TXT metadata.
13. **Persistence:** Session-only initially; S4 export later. No implicit Passive History expansion.
14. **UI:** New Service Discovery workflow with mDNS mode and bounded service/detail lists.
15. **Resources:** One worker/slot and bounded DNS records/name storage; maximum records and largest block must be fixed before UI.
16. **Backlog:** PR-01/03/04; F4-027–029/032/034.
17. **Phase 8:** Registered RX is constrained; new service model avoids redesign-first Passive DB and blocked PCAP analyzer.
18. **External:** RFC 6762/6763 and product-label mapping provenance — EXTERNAL STANDARD / RESEARCH REQUIRED.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.5-A | Define bounded DNS-SD record/identity and parser vectors | PR-01; new protocol tools/S2 | Compression loop/bounds safety; no UI strings as data | Fixed record/name maxima | Valid, compressed, cyclic, truncated vectors | Protocol/data contract approved |
| F1.5-B | Implement passive S2 mDNS collection | PR-03/04; RX/filter | Bounded callback; unregister before free | One slot + bounded service store | Build, multicast capture, malformed/cancel/filter tests | Backend gate passes |
| F1.5-C | Add Service Discovery mDNS UI | Existing scene conventions | Main-thread UI; one worker | Bounded menus/details | Physical browse/start/stop/repeat | Behavior/architecture docs updated |

20. **Acceptance:** Required service classes are derived from validated DNS-SD records, duplicates are stable, bounds/errors are visible, no TX occurs in passive mode.
21. **Regression:** Multicast filtering, Passive, Sniffer, RX slot use, memory headroom.
22. **Summary:** NOT ADOPTED historically; now MERGED into Service Discovery; Wave 2; high placement confidence, medium protocol-detail confidence pending research.

## 16. F1.6 — DHCP option-55 fingerprint

1. **Historical intent:** Passively map DHCP Parameter Request List ordering/content to known OS families using the existing DHCP foundation.
2. **Status:** NOT PREVIOUSLY ADOPTED. Current DHCP code is a DORA client and PCAP analyzer labels message type; no option-55 observation/fingerprint database exists.
3. **Disposition:** MERGE / CONSOLIDATE into Traffic Monitor’s DHCP mode, sharing S1 with F1.7.
4. **Mapping:** Reuse bounded Ethernet/IPv4/UDP/DHCP framing knowledge, not the direct-RX DORA transaction.
5. **Gap:** Passive DHCP decoder for client identity/option 55, normalized signature representation, provenance-aware mapping, confidence/unknown output.
6. **Placement:** S1 DHCP classifier plus bounded client observation store. Do not add responsibility to Get IP.
7. **RX:** Registered observation; never DHCP direct receive/pause.
8. **TX:** None.
9. **Worker:** Traffic Monitor worker and lifecycle.
10. **Filter:** Broadcast/multicast requirements through PR-04; restore baseline.
11. **Network:** ENC/link; no configured local IP or gateway.
12. **Data/identity:** Client MAC/client identifier plus option-55 sequence and last-seen/count; OS family is an attributed inference.
13. **Persistence:** Session-only at first; explicit asset export later. Signature table may be read-only data, not mutable settings.
14. **UI:** DHCP mode detail under Traffic Monitor; show evidence/confidence, not definitive OS truth.
15. **Resources:** Bounded client/signature tables; one S1 slot/worker; size budget before implementation.
16. **Backlog:** PR-01/03/04 and F4-027–029/034.
17. **Phase 8:** Uses new bounded observation engine; avoids direct RX and Passive fixed model.
18. **External:** DHCP option syntax and legally/provenance-suitable fingerprint corpus require research.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.6-A | Define decoder, identity, and fingerprint provenance | S1 contract; protocol tools | Bounds first; unknown remains unknown | Fixed option/signature maxima | Valid/malformed DHCP vectors | Research/data review approved |
| F1.6-B | Implement classifier and inference | PR-01/03/04; S1 | Registered RX only; no DORA coupling | Bounded per-client records | Build, captures, cancellation/filter restore | Backend gate passes |
| F1.6-C | Add DHCP fingerprint detail UI | Traffic Monitor scenes | Main-thread render; qualified labels | Bounded detail strings | Physical observation/repeat/no-link | Canonical docs updated |

20. **Acceptance:** Valid PRLs map reproducibly; unknown/truncated data is safe; no DHCP state mutation occurs.
21. **Regression:** Get IP/DORA, auto responders, Traffic Monitor co-modes, filter state.
22. **Summary:** New Traffic Monitor capability; Wave 3; medium confidence pending signature research.

## 17. F1.7 — DHCP client tracker continuous

1. **Historical intent:** Maintain a long-running log of observed DHCP DISCOVER clients.
2. **Status:** NOT PREVIOUSLY ADOPTED.
3. **Disposition:** MERGE / CONSOLIDATE with F1.6 in Traffic Monitor’s DHCP mode.
4. **Mapping:** Current analyzer can recognize DISCOVER but has no live client log or structured history.
5. **Gap:** Bounded client-event aggregation, elapsed/last-seen/count presentation, long-run resource behavior, optional explicit export.
6. **Placement:** S1 DHCP records shared with F1.6. A mode setting selects client tracking/detail, not a separate worker.
7. **RX:** Registered S1 observation.
8. **TX:** None.
9. **Worker:** Traffic Monitor worker; cancellation and UI refresh use PR-03.
10. **Filter:** Broadcast acceptance under PR-04.
11. **Network:** ENC/link only.
12. **Data/identity:** Stable client MAC/client identifier with bounded event counters and timestamps; define eviction deterministically.
13. **Persistence:** No automatic cross-relaunch history is required. Explicit export may feed S4 after PR-05.
14. **UI:** Same DHCP mode as F1.6, with client list/details.
15. **Resources:** Bounded ring/map; no unbounded “continuous” log or per-packet allocation.
16. **Backlog:** PR-01/03/04; PR-05 only for export; F4-027–029.
17. **Phase 8:** New bounded engine avoids long-lived App growth and persistence duplication.
18. **External:** DHCP identity/privacy semantics and timebase behavior require specification review.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.7-A | Add bounded DHCP client aggregation to S1 | F1.6-A/S1 | Deterministic bound/eviction; no append-only heap | Fixed client/event budget | Synthetic long-run and duplicate tests | Resource contract approved |
| F1.7-B | Add client tracking UI and optional export hook | PR-03; later S4 | No worker-side GUI; no silent persistence | Bounded menus/export buffer | Hours-equivalent replay, stop/reopen | Behavior docs and gate pass |

20. **Acceptance:** Repeated DISCOVERs update stable clients without unbounded growth; stop/restart clears or retains only as explicitly documented.
21. **Regression:** F1.6 inference, DHCP/DORA, memory guards, Sniffer/Passive transitions.
22. **Summary:** New consolidated DHCP monitor behavior; Wave 3; high architecture confidence.

## 18. F1.8 — SSDP / UPnP discovery (M-SEARCH)

1. **Historical intent:** Actively send SSDP M-SEARCH and enumerate routers, TVs, NAS, and IoT services.
2. **Status:** NOT PREVIOUSLY ADOPTED.
3. **Disposition:** MERGE / CONSOLIDATE as SSDP mode of Service Discovery using S2.
4. **Mapping:** UDP framing/scanner primitives exist; no SSDP builder/parser/result model.
5. **Gap:** Standards-compliant request, registered response correlation, bounded HTTP-like header parsing, deduplication, detail UI.
6. **Placement:** S2 protocol profile and shared Service Discovery scenes; not Ports Scanner because the workflow is multicast service enumeration.
7. **RX:** Register predicate before M-SEARCH; collect bounded multiple replies for a timed window.
8. **TX:** Feature worker builds request in shared ENC TX buffer and sends under ENC mutex; no callback-local TX needed.
9. **Worker:** One Service Discovery owner; cancel wakes/slices collection, unregisters, joins.
10. **Filter:** Multicast reception/restore through PR-04.
11. **Network:** ENC/link/configured IPv4 and multicast route on local segment; no gateway required for link-local SSDP.
12. **Data/identity:** USN/location/service type/source tuple with bounded headers.
13. **Persistence:** Session-only; S4 export later.
14. **UI:** Service Discovery SSDP mode; progress, result list, details.
15. **Resources:** One worker/slot, bounded response table/header storage, scanner-like timeout; guard total/largest block.
16. **Backlog:** PR-01/02/03/04; F4-011–016/027–029/032/034.
17. **Phase 8:** S2 extends scanner concepts without making scanner session a streaming service.
18. **External:** SSDP/UPnP specifications and interoperability response limits require research.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.8-A | Define SSDP profile/result/parser | S2; standards research | Bounded headers; no generic unsafe analyzer | Fixed response/header maxima | Valid/malformed/duplicate vectors | Protocol contract approved |
| F1.8-B | Implement M-SEARCH collection | PR-01–04; S2 | Register before send; no direct RX | Worker + one slot + table | Lab responder, timeout/cancel/filter tests | Backend/resource gate passes |
| F1.8-C | Add SSDP UI/details | Service Discovery scenes | Main-thread UI and clear retry states | Bounded menus | Physical no-link/multi-device/repeat | Canonical docs updated |

20. **Acceptance:** Multiple valid replies are correlated/deduplicated; malformed headers cannot escape bounds; cancellation restores filters and releases state.
21. **Regression:** mDNS mode, multicast filters, automatic responders, active scanner transitions.
22. **Summary:** New Service Discovery mode; Wave 2; high placement confidence.

## 19. F1.9 — Traceroute (ICMP TTL and UDP variants)

1. **Historical intent:** Trace ordered network hops using ICMP TTL behavior and UDP variants, reusing ICMP tools.
2. **Status:** NOT PREVIOUSLY ADOPTED. Ping and ICMP packet tools are prerequisites, not traceroute.
3. **Disposition:** ADD MODE / SUBMODE to Ping Host as “Trace Route.”
4. **Mapping:** Reuse target selection, scanner next-hop resolution, ICMP tooling, worker/result conventions.
5. **Gap:** TTL-controlled probes, ICMP Time Exceeded/Destination Unreachable matching, optional UDP probe profile, bounded hop/retry result model.
6. **Placement:** Ping scene family and a feature-local traceroute module using scanner request/reply; no new top-level menu.
7. **RX:** `scanner_session` registered wait per probe/attempt; never direct RX.
8. **TX:** Worker-owned ICMP/UDP construction in shared TX buffer; one outstanding correlated probe at a time initially.
9. **Worker:** One Ping/Trace `AppThreadOwner`; stop flag, sliced waits, join before results/UI teardown.
10. **Filter:** Baseline unicast/broadcast only unless research proves more.
11. **Network:** ENC/link/configured IPv4/subnet; gateway and MAC required for off-subnet target.
12. **Data/identity:** Ordered hop number plus responder IP, timeout state, optional RTT; bounded max TTL and attempts.
13. **Persistence:** None initially; shares confirmed Target IP semantics after PR-02.
14. **UI:** Mode selector in Ping, target/retry/max-hop configuration, progress and ordered result pages.
15. **Resources:** Existing-class feature worker, scanner semaphore per wait, bounded hop array; exact stack after call-depth review.
16. **Backlog:** PR-01/02/03; F4-011–016/027–029.
17. **Phase 8:** Scanner session is EXTEND WITH CURRENT CONSTRAINTS and matches bounded request/reply.
18. **External:** ICMP/UDP traceroute correlation, NAT/firewall behavior, and timer resolution require standards/interoperability research.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.9-A | Define probe/result model and ICMP matcher | PR-01/02; ICMP/scanner tools | Full length/correlation validation | Fixed hops/attempts | Packet vectors and route edge cases | Protocol model approved |
| F1.9-B | Implement ICMP trace backend | PR-03; Ping/module | Registered waits; one worker; cancel slices | Bounded array + semaphore | Lab multi-hop, timeout, BACK/repeat | Backend gate passes |
| F1.9-C | Add UDP variant and mode UI | F1.9-B; scenes | Shared backend, no duplicate worker/RX | Same bounded model | Variant/no-route/off-subnet tests | Behavior/architecture docs updated |

20. **Acceptance:** Ordered hops/timeouts render safely, target/gateway semantics are correct, cancellation is prompt, repeat/cross-feature transitions leak no resources.
21. **Regression:** Ping, target settings, scanner cache, automatic ICMP responder, startup guards.
22. **Summary:** New Ping submode; Wave 2; high placement confidence.

## 20. F1.10 — Reverse DNS sweep

1. **Historical intent:** Issue PTR queries for each host found by Scan Hosts and associate names with results.
2. **Status:** NOT PREVIOUSLY ADOPTED. Scan Hosts persists IP/MAC results; no DNS codec/resolver exists.
3. **Disposition:** EXTEND EXISTING FEATURE through an “Resolve Names” action backed by S2.
4. **Mapping:** `App.ip_list`/`last_scan.bin` provide the bounded input set; S2 provides DNS encoding/decoding and normalized names.
5. **Gap:** Resolver selection policy, PTR name construction, safe compressed-name parsing, sequential/bounded concurrency, result association and display.
6. **Placement:** Scan Hosts result workflow; DNS backend shared with mDNS and UDP service profiles.
7. **RX:** Scanner registered request/reply per host/query; no direct RX.
8. **TX:** Worker-owned DNS UDP query in shared TX buffer.
9. **Worker:** One Scan Hosts follow-up owner; cancellation between/slicing queries; join before result free.
10. **Filter:** Baseline unicast; no new filter unless resolver behavior proves otherwise.
11. **Network:** ENC/link/configured IPv4 and a defined DNS server route. Current App has no canonical DNS-server field, so PR-02 must define it or require user input.
12. **Data/identity:** Existing host IP/MAC plus one bounded PTR name and per-query status.
13. **Persistence:** Do not extend native `last_scan.bin` before PR-05 and an explicit versioned migration decision; initial names may be session-only/S4 input.
14. **UI:** Action on saved/current Scan Hosts results, progress, names in details; no new top-level feature.
15. **Resources:** Up to 255 hosts but one outstanding query initially; bounded name storage can be significant and must be feature-scoped or sparse.
16. **Backlog:** PR-01/02/03/05; F4-011–022/027–029.
17. **Phase 8:** Scanner fits bounded exchange; last-scan persistence is FIX CORRECTNESS FIRST.
18. **External:** DNS RFCs, compression rules, timeout/retry, and resolver source require research.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.10-A | Add safe DNS/PTR codec to S2 | Standards; S2 protocol tools | Compression/bounds proof; no generic PCAP parser reuse | Fixed name/message bounds | Valid/cyclic/truncated vectors | Codec review approved |
| F1.10-B | Define resolver/network state and sweep backend | PR-01–03; Scan Hosts/S2 | Registered wait; no silent mixed tuple | Sparse/bounded names; one wait | Local DNS, timeout/cancel/255-host bounds | Backend/resource gate |
| F1.10-C | Integrate names and decide persistence | PR-05; scenes/storage | Do not mutate old format silently | Versioned store only if approved | Relaunch compatibility and I/O failure tests | Behavior/storage docs updated |

20. **Acceptance:** Every query is correlated, malformed names are rejected, missing DNS is explicit, and old scan files remain compatible.
21. **Regression:** Scan Hosts cancellation/history, shared target/connection state, S2 mDNS name codec.
22. **Summary:** Extend Scan Hosts via S2; Wave 2; medium confidence pending DNS-state decision.

## 21. F1.11 — Wake-on-LAN sender

1. **Historical intent:** Send a Wake-on-LAN magic packet using broadcast or directed addressing.
2. **Status:** NOT PREVIOUSLY ADOPTED.
3. **Disposition:** EXTEND EXISTING FEATURE: host action from Scan Hosts plus direct MAC/target entry; no standalone top-level feature.
4. **Mapping:** Reuse saved-host MAC selection, ENC send, target/edit conventions after PR-02, and existing UDP/Ethernet builders where appropriate.
5. **Gap:** Validated magic-packet builder, destination policy, repeat count, confirmation/result UI.
6. **Placement:** Scan Hosts host details/action and a direct “Wake Host” entry within that workflow.
7. **RX:** None required.
8. **TX:** Feature/scene-owned bounded frame in shared TX buffer under ENC mutex; optional UDP wrapper only if selected and researched.
9. **Worker:** No worker for a single bounded send unless repetitions/delays require one; if used, one owner-tagged worker.
10. **Filter:** None.
11. **Network:** ENC/link and destination MAC; directed IP/subnet requires a valid configured tuple, plain Ethernet broadcast does not require gateway.
12. **Data/identity:** Target MAC plus optional secure-on/password and destination mode; no discovery record.
13. **Persistence:** None by default; reuse saved hosts, do not persist sensitive optional data without decision.
14. **UI:** Host action/direct MAC input, mode/repeat confirmation, success/failure status.
15. **Resources:** One frame ≤1518 bytes; no large allocation; startup guard only if worker/UI SDK demand warrants.
16. **Backlog:** PR-02/03; F4-015/016/029/030 and network-state findings where directed.
17. **Phase 8:** ENC is healthy; edit transactions simplify first; avoid unnecessary worker.
18. **External:** WoL packet and broadcast interoperability — EXTERNAL STANDARD / RESEARCH REQUIRED.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.11-A | Define/send validated magic packet | PR-02; protocol tools/ENC | Exact bounds/checksums as applicable; no shared-buffer overlap | One frame | Golden vectors and lab wake test | Backend approved |
| F1.11-B | Integrate saved/direct target UI | PR-03/ByteInput contract; Scan Hosts | Confirm/cancel transaction; no hidden persistence | Existing views | BACK/OK, no-link, repeat tests | APP_BEHAVIOR updated; accepted |

20. **Acceptance:** Selected host wakes in supported lab, invalid input cannot send, and cancellation does not mutate saved configuration.
21. **Regression:** Scan Hosts selection/history, MAC editor behavior, ENC TX and auto responders.
22. **Summary:** Extend Scan Hosts; Wave 2; high placement confidence.

## 22. F1.12 — Wake-on-LAN passive capture

1. **Historical intent:** Passively identify WoL magic packets, historically “hardware-unlocked” through ENC MPEN.
2. **Status:** NOT PREVIOUSLY ADOPTED. `ERXFCON_MPEN` exists only as a constant.
3. **Disposition:** MERGE / CONSOLIDATE into Traffic Monitor’s WoL mode.
4. **Mapping:** Reuse S1 and optional hardware magic-packet acceptance; do not overload Sniffer’s PCAP UI.
5. **Gap:** Validated magic-packet detection at possible offsets/encapsulations, event/source/target presentation, hardware-versus-software filter decision.
6. **Placement:** S1 event classifier and Traffic Monitor mode.
7. **RX:** Registered observation.
8. **TX:** None.
9. **Worker:** Traffic Monitor owner and bounded event store.
10. **Filter:** PR-04 must own MPEN/broad reception and restoration; software predicate remains necessary for complete validation.
11. **Network:** ENC/link only.
12. **Data/identity:** Event keyed by target MAC plus source/time/count; bounded ring.
13. **Persistence:** Session-only; explicit export later if justified.
14. **UI:** WoL monitor mode with event list/details.
15. **Resources:** One shared S1 slot/worker and small bounded event ring; no PCAP index.
16. **Backlog:** PR-01/03/04; F4-027–029/032/034.
17. **Phase 8:** Filter ownership is SIMPLIFY FIRST; Traffic Monitor avoids Passive DB expansion.
18. **External:** ENC MPEN exact behavior and WoL encapsulations require datasheet/standard research.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.12-A | Define WoL detector and filter policy | PR-04; S1/ENC research | Software-validate; do not trust MPEN alone | Fixed event record | Golden/malformed vectors and hardware feasibility | Policy approved |
| F1.12-B | Implement monitor backend/UI | PR-01/03; Traffic Monitor | Registered RX, scoped filter, bounded ring | Shared worker/slot | Lab magic packets, cancel/restore/repeat | Canonical docs updated |

20. **Acceptance:** Valid events are detected without false acceptance from short frames; filter and resources restore on every exit.
21. **Regression:** Normal filter, Sniffer, Passive, automatic responders.
22. **Summary:** Traffic Monitor mode; Wave 3; medium confidence pending MPEN contract.

## 23. F1.13 — TCP banner grab

1. **Historical intent:** Connect to HTTP/SSH/SMTP/FTP and read/log bounded greeting/banner data.
2. **Status:** PARTIALLY IMPLEMENTED. TCP builders and a currently uncalled direct-RX handshake helper exist; no safe registered stream/banner session exists.
3. **Disposition:** ADD MODE / SUBMODE to Ports Scanner using S3; legacy handshake is not the architecture template.
4. **Mapping:** Reuse target/port selection and TCP packet crafting; replace direct-RX helper with registered state machine.
5. **Gap:** TCP state/sequence handling, bounded payload receive, optional protocol request, timeout/close, sanitized rendering and structured service result.
6. **Placement:** Ports Scanner “Service Banner” mode; S3 backend supplies handshake/receive/close.
7. **RX:** Registered multi-packet TCP session owned by feature worker; explicit callback-to-session queue/state.
8. **TX:** Shared TX buffer from one worker; callback does not build in shared buffer; ACK/FIN/RST ordering researched.
9. **Worker:** One Ports owner; cancellation unblocks session, unregisters, closes/best-effort resets, joins.
10. **Filter:** Baseline unicast.
11. **Network:** ENC/link/configured IPv4/subnet; gateway/MAC for off-subnet targets.
12. **Data/identity:** Target IP+port+transport, bounded banner bytes, protocol hint and truncation/error status.
13. **Persistence:** Session-only; structured result can feed S4, not Settings or last-scan directly.
14. **UI:** Ports mode/profile selection, progress, service/banner details.
15. **Resources:** One worker/slot; bounded stream buffer chosen after stack/heap review; no 1,500-byte uncontrolled stack copy.
16. **Backlog:** PR-01–03; F4-001–004/011–016/027–029.
17. **Phase 8:** Scanner is bounded request/reply, but stream state requires S3 rather than enlarging scanner session.
18. **External:** TCP state semantics and protocol greeting/request rules require standards/interoperability research.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.13-A | Build registered bounded TCP session in S3 | PR-01–03; TCP tools | No direct RX; correct sequence/lifetime | Fixed session/banner buffers | SYN/ACK/data/close/loss vectors | S3 gate passes |
| F1.13-B | Add protocol banner profiles | S3; research | Bounded request/response, escaped display | Profile constants only | HTTP/SSH/SMTP/FTP lab cases | Profile review |
| F1.13-C | Add Ports mode/UI and S4 result hook | scenes/S4 | Main-thread UI; one worker | Bounded results/menu | Cancel/repeat/off-subnet/transitions | Behavior/architecture docs updated |

20. **Acceptance:** Supported banners are captured or explicitly timed out/truncated; malformed TCP cannot escape bounds; teardown unregisters before buffers disappear.
21. **Regression:** TCP SYN scan, OS probes, target/network state, automatic responders.
22. **Summary:** Ports mode via new S3; Wave 2; medium risk/high placement confidence.

## 24. F1.14 — Payload-aware UDP service scan

1. **Historical intent:** Replace generic UDP scanning with DNS, SNMP GET, NTP version, NetBIOS name, mDNS, and SSDP protocol probes.
2. **Status:** PARTIALLY IMPLEMENTED / CURRENT CANDIDATE. A dormant UDP branch sends generic probes but cannot distinguish closed/no-response and has no payload profiles.
3. **Disposition:** HARDEN / COMPLETE CURRENT IMPLEMENTATION as a Ports Scanner “UDP Services” mode backed by S2/S6.
4. **Mapping:** Reuse scanner routing/register-before-trigger and UDP builder; do not expose the current persisted protocol branch before F4-009/010.
5. **Gap:** Per-service builders/parsers/correlation, ICMP-unreachable handling, result semantics, supported-profile UI, bounds and protocol-index validation.
6. **Placement:** Ports Scanner owns target/range/profile workflow; S2 owns profile registry/results; S6 owns SNMP encoding.
7. **RX:** Scanner registered wait for simple profiles; bounded multi-reply S2 collection where required.
8. **TX:** One worker/shared TX buffer; profile-owned payload builders.
9. **Worker:** Existing Ports owner after PR-03; cancel between/sliced waits.
10. **Filter:** Baseline unicast; mDNS/SSDP profiles use PR-04 multicast and may delegate to Service Discovery rather than duplicate.
11. **Network:** ENC/link/configured IPv4; gateway for off-subnet; multicast only for corresponding profiles.
12. **Data/identity:** Target IP+port+profile with response/closed/timeout and bounded service metadata.
13. **Persistence:** Validated profile selection in Settings only if useful; results session-only/S4.
14. **UI:** Replace generic TCP/UDP selector with explicit scan profiles; no raw “UDP means open on timeout” claim.
15. **Resources:** One worker/slot, per-probe semaphore, bounded response and results; duration budget explicit.
16. **Backlog:** F4-009/010 hard blockers; PR-01–03; network and GUI F4 findings.
17. **Phase 8:** Current dormant UDP is not safe to activate; S2 avoids an oversized scanner utility.
18. **External:** Every protocol profile requires authoritative standard research and lab vectors.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.14-A | Remove unsafe selector state and define profile/result API | PR-02; F4-009/010; Ports/S2 | No generic timeout=open semantics | Fixed profile/result bounds | Settings migration and state tests | F4 gates resolved |
| F1.14-B | Implement DNS/NTP/NetBIOS profiles | PR-01/03; S2 | Register before send; strict parsers | Per-profile bounded buffers | Golden/malformed/lab tests | Profiles individually accepted |
| F1.14-C | Reuse mDNS/SSDP and SNMP backends | F1.5/F1.8/S6 | No duplicate codecs/multicast ownership | Shared infrastructure | Cross-feature equivalence tests | Shared-backend review |
| F1.14-D | Add Ports UI/progress/results | scenes | Main-thread UI; honest response states | Bounded menu | Cancel/repeat/no-link/range tests | Canonical docs updated |

20. **Acceptance:** Each profile has positive, negative, timeout, malformed, and cancellation behavior; generic dormant UDP path is not exposed as equivalent.
21. **Regression:** TCP scan, settings schema, Service Discovery, scanner session, startup guards.
22. **Summary:** Partial candidate; complete Ports mode via S2/S6; Wave 2; high confidence.

## 25. F1.15 — SYN scan (half-open)

1. **Historical intent:** Faster half-open SYN port enumeration rather than completing connections.
2. **Status:** PARTIALLY IMPLEMENTED. Current `tcp_syn_scan()` registers before SYN and records SYN-ACK without completing a connection, but result/GUI and protocol cleanup semantics need hardening.
3. **Disposition:** HARDEN / COMPLETE CURRENT IMPLEMENTATION in Ports Scanner.
4. **Mapping:** Existing Ports scene, `tcp_syn_scan()`, scanner session, TCP matcher/builder.
5. **Gap:** Explicit RST/close policy after SYN-ACK, closed-response classification/pacing, structured results, main-thread UI, cancellation/resource validation.
6. **Placement:** Existing default TCP profile using S3 response semantics.
7. **RX:** Existing registered scanner waits retained.
8. **TX:** SYN and researched cleanup packet from feature worker/shared TX buffer.
9. **Worker:** Existing 5 KiB Ports worker, owner-tagged; migrate UI events under PR-03.
10. **Filter:** Baseline unicast.
11. **Network:** ENC/link/configured IPv4/subnet/gateway route.
12. **Data/identity:** Target IP+TCP port and open/closed/filtered/timeout state.
13. **Persistence:** Session-only/S4; scan settings remain validated settings.
14. **UI:** Existing Ports workflow; improve result model without a new scene unless details justify it.
15. **Resources:** Existing worker and per-port semaphore; bounded results/menu remain data-dependent and guarded.
16. **Backlog:** PR-01–03; F4-011–016/027–029.
17. **Phase 8:** Scanner request/reply is compatible; worker GUI is external-contract first.
18. **External:** Correct half-open cleanup and response classification require TCP research.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.15-A | Formalize S3 SYN response/cleanup semantics | PR-01/02; TCP/scanner | No direct RX; correlated flags/ports | Existing wait objects | Open/closed/filtered vectors | Protocol review |
| F1.15-B | Move results/progress to safe UI events | PR-03; Ports scene | No worker GUI mutation | Bounded result model | Range/cancel/repeat/memory tests | F4-029 contract satisfied |
| F1.15-C | Physical interoperability hardening | S3 | Preserve one-worker/filter baseline | No new large allocation | Multiple hosts/firewalls, transitions | Behavior docs and acceptance |

20. **Acceptance:** Scan states are accurate within documented semantics, cleanup does not leave sessions, cancellation is prompt, and repeated runs remain stable.
21. **Regression:** Ports settings, S3 banners/flag modes, scanner cache, OS Detector.
22. **Summary:** Partial existing capability; harden Ports Scanner; Wave 2; high confidence.

## 26. F1.16 — FIN/NULL/Xmas scans

1. **Historical intent:** User-selectable FIN, NULL, and Xmas TCP scan variants and OS-sensitive interpretation; repair the historical Xmas sender.
2. **Status:** PARTIALLY IMPLEMENTED. Current FIN/NULL/Xmas builders are called by OS Detector; Xmas success return is fixed. They are not port-scan modes and response semantics remain OS-oriented.
3. **Disposition:** ADD MODE / SUBMODE to Ports Scanner through S3.
4. **Mapping:** Reuse current builders after validation and scanner routing; do not reuse OS direct-RX loop.
5. **Gap:** Port-range orchestration, RST/no-response classifiers per mode, UI selection, honest open/closed/filtered semantics.
6. **Placement:** Ports advanced TCP profiles; shared S3 matcher/result model.
7. **RX:** Registered scanner/S3 waits.
8. **TX:** Worker/shared TX buffer with validated flag builders.
9. **Worker:** Ports owner; cancellation and UI event contract from PR-03.
10. **Filter:** Baseline unicast.
11. **Network:** Same as SYN scan.
12. **Data/identity:** Target+port+scan profile and qualified response state.
13. **Persistence:** Validated profile choice only; results session/S4.
14. **UI:** Advanced scan mode within Ports; warnings about ambiguous no-response results.
15. **Resources:** Reuse one worker/wait/result table; do not run modes concurrently.
16. **Backlog:** PR-01–03; OS F4-001/002 must be resolved before extracting shared probe behavior; F4-027–029.
17. **Phase 8:** S3 is required so scanner session does not become an oversized TCP service.
18. **External:** RFC TCP behavior and platform/firewall interpretation require research.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.16-A | Validate builders and define per-mode result semantics | PR-01; S3/TCP tools | No response is not universally open | No new long-lived data | Golden packets/response vectors | Protocol contract approved |
| F1.16-B | Add registered range-scan profiles | PR-02/03; Ports/S3 | No OS direct RX; one mode at a time | Existing-class worker/results | Lab targets, cancel/range tests | Backend gate |
| F1.16-C | Add advanced-mode UI and warnings | scenes/settings | Validated index; main-thread UI | Bounded menu | BACK/relaunch/settings migration | Docs updated; accepted |

20. **Acceptance:** Flag packets and interpretations match documented scope; ambiguity is displayed; no path competes for ENC RX.
21. **Regression:** OS Detector probes, SYN scan, protocol selection, target state.
22. **Summary:** Partial builders; add Ports modes; Wave 2; medium-high confidence.

## 27. F1.17 — SNMP community brute

1. **Historical intent:** Test `public`, `private`, and a local bounded wordlist against SNMP/UDP 161.
2. **Status:** NOT PREVIOUSLY ADOPTED.
3. **Disposition:** MERGE / CONSOLIDATE into a new SNMP Tools feature, “Community Audit” mode, sharing S6 with F1.18.
4. **Mapping:** UDP/scanner infrastructure exists; no BER/SNMP codec or wordlist lifecycle.
5. **Gap:** Safe SNMP v1/v2c codec, request ID correlation, candidate source/limits, rate/cancel policy, clear authorization warning, results.
6. **Placement:** Distinct SNMP workflow because credentials, target, rate, and subsequent walk form one lifecycle; not generic Ports Scanner.
7. **RX:** Registered scanner waits per request; no direct RX.
8. **TX:** S6 builds bounded datagrams in worker/shared TX buffer.
9. **Worker:** One SNMP owner; candidate loop cancellable between/sliced waits; file closed before join/free if wordlist used.
10. **Filter:** Baseline unicast.
11. **Network:** ENC/link/configured IPv4 and route/gateway as needed.
12. **Data/identity:** Target IP, SNMP version, community candidate result, request ID; never persist secrets by default.
13. **Persistence:** FileBrowser-selected wordlist may be read; successful community remains session-only unless explicit secure policy is approved.
14. **UI:** New SNMP Tools → Community Audit with target, built-in/file candidates, rate/limit, confirmation, progress/results.
15. **Resources:** One worker/slot, bounded BER buffers, one candidate string, optional SDK FileBrowser worker; no full wordlist load.
16. **Backlog:** PR-01–03/05; F4-011–018/027–031.
17. **Phase 8:** Scanner is compatible per request; FileBrowser and storage require external/fix-first contracts.
18. **External:** SNMP BER/v1/v2c, response/error semantics, safe default rate and wordlist policy require research.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.17-A | Build bounded S6 BER/SNMP request/response codec | Standards; protocol tools | Length/correlation validation; no generic analyzer | Fixed datagram buffers | Golden/malformed vectors | Codec accepted |
| F1.17-B | Implement candidate source/session | PR-01–03/05; scanner/storage | Stream wordlist; cancel; no secret persistence | One line buffer + wait | Built-in/file, I/O failure, cancel tests | Session/security review |
| F1.17-C | Add SNMP Tools mode/UI | scenes | Warning/confirmation; main-thread UI | Bounded results | Lab agent, wrong/right community, repeat | Behavior/security docs updated |

20. **Acceptance:** Only bounded authorized candidates are sent; responses are correlated; cancellation/file failure cleanly closes all resources.
21. **Regression:** FileBrowser, scanner slots, network state, F1.18 S6 reuse.
22. **Summary:** New SNMP Tools mode; Wave 4; medium confidence pending standards/security review.

## 28. F1.18 — SNMP v1/v2c walk

1. **Historical intent:** Walk bounded `ifTable`, bridge/MAC table, and IP-MIB data to construct switch topology.
2. **Status:** NOT PREVIOUSLY ADOPTED.
3. **Disposition:** MERGE / CONSOLIDATE as SNMP Tools “Walk” mode using S6. “Complete topology” is treated as an output goal requiring bounded evidence, not an unlimited promise.
4. **Mapping:** Depends on F1.17 S6; no current MIB/object/result model.
5. **Gap:** GETNEXT/GETBULK policy by version, OID codec, loop/end/error handling, selected MIB profiles, bounded table/topology model.
6. **Placement:** SNMP Tools; structured findings feed S4 Asset Inventory.
7. **RX:** Registered request/reply sequence through S6/scanner.
8. **TX:** Worker/shared TX buffer, one correlated request at a time initially.
9. **Worker:** Same SNMP owner; stop breaks bounded walk, unregisters, joins.
10. **Filter:** Baseline unicast.
11. **Network:** Configured IPv4/route and an explicitly supplied session community.
12. **Data/identity:** Agent IP + OID/value with typed bounded representations; topology relationships carry source/provenance.
13. **Persistence:** Session-only result plus explicit S4 export; never silently store community.
14. **UI:** SNMP target/profile/version/community configuration, progress, tables/details/export action.
15. **Resources:** Bounded rows/OID/value lengths and request count; feature-scoped allocation guarded for total/max block.
16. **Backlog:** Same PR-*/F4 dependencies as F1.17 plus PR-05 for export.
17. **Phase 8:** New S6 prevents scanner-session overgrowth and App permanent table growth.
18. **External:** SNMP/BER/MIB standards, bridge MIB variants, interoperability — research required.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.18-A | Extend S6 with bounded walk state machine | F1.17-A; standards | Detect loops/end/error; strict OID/value bounds | Fixed response + bounded rows | Simulated agents/malformed/loop vectors | Backend accepted |
| F1.18-B | Add selected MIB profiles and S4 mapping | S4/S6 | Explicit provenance; no “complete” inference without data | Feature-scoped tables | ifTable/bridge/IP-MIB fixtures | Data/resource review |
| F1.18-C | Add Walk UI/export | PR-03/05; SNMP scenes | Cancel/close/join; no secret persistence | Bounded menus/export | Lab switch, failure/repeat/relaunch | Canonical docs updated |

20. **Acceptance:** Walk terminates deterministically, enforces all limits, renders supported objects, and exports provenance without storing credentials.
21. **Regression:** Community Audit, scanner/session resources, S4 identity merge, storage.
22. **Summary:** New SNMP Tools mode; Wave 4; medium confidence.

## 29. F1.19 — Top-talkers / heat-map

1. **Historical intent:** Display live ranked traffic activity as an on-device bar chart/heat map.
2. **Status:** NOT PREVIOUSLY ADOPTED. Sniffer exposes only a total packet count.
3. **Disposition:** MERGE / CONSOLIDATE into Traffic Monitor, using per-host counters from F1.20/S1.
4. **Mapping:** Reuse source/destination extraction only after bounded L2/L3 validation; use a custom view only if Widget cannot safely render the required chart.
5. **Gap:** Stable host identity, byte/packet aggregation, ranking window, refresh cadence, bounded visualization and “other” bucket.
6. **Placement:** Traffic Monitor Top Talkers mode; presentation over S1 rather than parsing inside draw callbacks.
7. **RX:** S1 registered observation.
8. **TX:** None.
9. **Worker:** Traffic Monitor owner aggregates; GUI thread consumes snapshots/custom events.
10. **Filter:** PR-04 scoped broad reception, restored on exit.
11. **Network:** ENC/link only; IPv4 configuration not required to observe frames.
12. **Data/identity:** Primarily MAC, with validated IPv4 association where observed; direction relative to local MAC must be explicit.
13. **Persistence:** None initially; explicit S4 export may consume final snapshot.
14. **UI:** Traffic Monitor chart/list mode with bounded refresh; details show counts/bytes/window.
15. **Resources:** Shares bounded host table with F1.20; snapshot/double-buffer only if source model proves need; one worker/slot.
16. **Backlog:** PR-01/03/04; F4-027–029/032/034.
17. **Phase 8:** New S1 prevents Sniffer callback/UI growth and avoids Passive/PCAP data models.
18. **External:** GUI thread/draw contract and time-window semantics require SDK/product confirmation.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.19-A | Define ranking/window/snapshot contract | F1.20-A; GUI research | Draw immutable snapshot; bounded top-N | Fixed top-N/snapshot | Deterministic aggregate fixtures | UX/resource contract approved |
| F1.19-B | Add chart/list rendering and refresh events | PR-03; Traffic Monitor view | No dispatcher/worker GUI mutation | Bounded view state | Load, BACK, repeated refresh tests | SDK contract satisfied |
| F1.19-C | Physical traffic validation | S1/UI | Preserve filter/worker teardown | No new persistent data | Known flows, low/high load, transitions | Behavior docs updated |

20. **Acceptance:** Ranking matches validated counters, refresh cannot starve RX, and stop/cancel restores all state.
21. **Regression:** F1.20 counters, Sniffer capture, Passive/auto handlers, GUI responsiveness.
22. **Summary:** Traffic Monitor view over S1; Wave 3; medium-high confidence.

## 30. F1.20 — Per-host packet/byte counters time-series

1. **Historical intent:** Maintain per-host packet/byte counters over RX Dispatch, including time evolution.
2. **Status:** NOT PREVIOUSLY ADOPTED.
3. **Disposition:** NEW SHARED BACKEND: S1 observation engine, exposed through Traffic Monitor.
4. **Mapping:** RX Dispatch supplies frames but no per-host metrics; Sniffer counter is global only.
5. **Gap:** Bounded validated frame metadata, host identity, directional counters, sampling buckets, eviction/overflow policy, snapshot API.
6. **Placement:** S1 owns live aggregation; Traffic Monitor owns configuration/UI. It is not a permanent app-wide daemon.
7. **RX:** One bounded registered callback; extract/copy only required metadata.
8. **TX:** None.
9. **Worker:** One Traffic Monitor worker owns table/time buckets; callback synchronization must be explicit and short.
10. **Filter:** Scoped broad reception through PR-04.
11. **Network:** ENC/link only.
12. **Data/identity:** MAC-first host key plus optional validated IP; fixed host count and fixed rolling bucket count.
13. **Persistence:** Session-only by default; export final aggregate through S4 only.
14. **UI:** Counter/time-series mode and shared backend for F1.19/F1.25.
15. **Resources:** New bounded feature-scoped host×bucket allocation; exact count/block must be measured against responder and GUI overlap before approval.
16. **Backlog:** PR-01/03/04; F4-026–029/032/034.
17. **Phase 8:** Registered RX is constrained; no permanent App array or unbounded callback work; startup guard retains total/max-block checks.
18. **External:** SDK synchronization and timebase contracts; no protocol standard beyond validated Ethernet/IP accounting.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.20-A | Specify S1 metadata, bounds, identity, sampling | PR-01/03/04; S1 responsibility | Fixed bounds; callback does minimal work | Compute host×bucket payload/block | Source memory/coexistence review | Architecture/resource approval |
| F1.20-B | Implement registered aggregation backend | RX/S1 | Quiesce before free; no direct RX | One slot + guarded feature allocation | Packet fixtures, overload/eviction/cancel | Backend gate passes |
| F1.20-C | Add counters/time-series mode | Traffic Monitor UI | Snapshot-based main-thread render | Bounded UI strings/view | Physical flows, repeat/transitions | Canonical docs updated |

20. **Acceptance:** Counts/bytes/direction/buckets are reproducible; capacity behavior is explicit; handler latency and teardown meet the contract.
21. **Regression:** Dispatcher latency/stack, automatic responders, Sniffer/Passive filtering, memory headroom.
22. **Summary:** S1 shared-backend prerequisite and Traffic Monitor mode; capability implementation Wave 3; high architecture confidence.

## 31. F1.21 — Asset inventory CSV/JSON export

1. **Historical intent:** Export a consolidated IP/MAC/hostname/OS/services inventory to SD in CSV/JSON.
2. **Status:** NOT PREVIOUSLY ADOPTED. Current results live in incompatible transient/saved structures and UI strings.
3. **Disposition:** NEW STANDALONE USER FEATURE, Asset Inventory, backed by S4. Export formats are outputs, not the primary in-memory model.
4. **Mapping:** Producers include Scan Hosts, Ports, active/passive OS, Passive Discovery, S1/S2/S6. Current source lacks a shared structured result contract.
5. **Gap:** Provenance-aware asset identity/merge, bounded catalog, producer APIs, browse/details, explicit export and error handling.
6. **Placement:** S4 shared backend and Administration Asset Inventory scene. Producers publish structured final results; inventory must not scrape Submenu text.
7. **RX:** None directly; producer features retain their own RX models.
8. **TX:** None directly.
9. **Worker:** Export may use one owner-tagged worker if file work is nontrivial; browsing need not create a worker.
10. **Filter:** None directly.
11. **Network:** None for browsing/export; collection prerequisites belong to producers.
12. **Data/identity:** Asset key policy must reconcile MAC, IP, protocol neighbor, service and observation provenance without false merging.
13. **Persistence:** New bounded versioned internal catalog only if cross-session inventory is approved; CSV/JSON are replace/create exports with escaping and failure propagation.
14. **UI:** New top-level Administration item justified by cross-feature browse/merge/export lifecycle.
15. **Resources:** Potentially significant bounded catalog/export buffer. Stream export; do not construct a whole JSON document in one contiguous block.
16. **Backlog:** PR-05; F4-017–024/026–031/033; producer-specific findings.
17. **Phase 8:** App and persistence are simplify/fix-first; S4 avoids growing `App.ip_list`, `neighbor_t`, or PCAP state.
18. **External:** CSV/JSON interoperability, filesystem replacement, privacy/redaction requirements.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.21-A | Define S4 identity/provenance and capacity | PR-05; producer schemas | No UI scraping/false merge | Exact bounded record budget | Merge/conflict fixtures | Data/resource contract approved |
| F1.21-B | Add producer adapters incrementally | Scan/Ports/OS/S1/S2/S6 | Producer ownership retained | No duplicate large live DBs | Per-producer static/transition tests | Each adapter separately accepted |
| F1.21-C | Add browse/details workflow | PR-03; scenes | No worker unless required | Bounded menus/details | Empty/full/conflict/relaunch | Behavior docs updated |
| F1.21-D | Add streaming CSV then JSON export | PR-05; storage | Exact writes/escaping/failure surfaced | Small streaming buffer | Malformed text, full SD, reopen validation | Format docs and gate pass |

20. **Acceptance:** Catalog provenance is inspectable, bounds deterministic, exports parse externally, and I/O failure never reports success or destroys the last valid internal state.
21. **Regression:** All producer lifetimes, storage, memory coexistence, app relaunch.
22. **Summary:** New Asset Inventory feature/S4; Wave 4; medium confidence pending identity/product decisions.

## 32. F1.22 — PCAP live-write with BPF-lite filter

1. **Historical intent:** Add a pre-capture filter screen for broadcast, IP, and port while writing PCAP live.
2. **Status:** PARTIALLY IMPLEMENTED. Sniffer already writes PCAP through a registered handler but captures broadly and ignores write failure.
3. **Disposition:** ADD MODE / SUBMODE to Packet Sniffer after PR-06 capture hardening.
4. **Mapping:** Existing Sniffer lifecycle, PCAP writer, path/file UI and registered predicate/handler are the owner.
5. **Gap:** Validated filter expression/presets, configuration UI, predicate semantics, capture metadata, write-error propagation.
6. **Placement:** Sniffer configuration and capture-session-owned filter; not the unsafe stored-PCAP analyzer.
7. **RX:** Existing registered Sniffer consumer with predicate compiled from immutable bounded configuration.
8. **TX:** None.
9. **Worker:** Existing Sniffer owner; handler/capture session lifetime explicit; main-thread UI snapshots.
10. **Filter:** ENC remains scoped promiscuous/CRC capture; BPF-lite is software selection unless a proven hardware optimization preserves semantics.
11. **Network:** ENC/link only; no configured IP required.
12. **Data/identity:** Immutable filter config plus captured packet records/count/error state.
13. **Persistence:** Existing PCAP files; filter presets in settings only if explicit product need and schema validation.
14. **UI:** Existing Sniffer gains filter preset/config before Start and displays write errors.
15. **Resources:** Existing 4 KiB worker plus bounded filter state; no 16 KiB reader index during capture; automatic handlers may overlap.
16. **Backlog:** PR-03/04/06; F4-008/026–029/032/034.
17. **Phase 8:** Sniffer is EXTEND WITH CURRENT CONSTRAINTS; PCAP parser redesign is not required for predicate implementation but PR-06 capture correctness is.
18. **External:** Define “BPF-lite” grammar/preset semantics; SDK GUI and PCAP interoperability checks.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.22-A | Define bounded filter presets/IR and matcher | PR-04/06; Sniffer/protocol tools | Full length checks; immutable callback config | Small fixed filter state | Match/nonmatch/truncated vectors | Filter contract approved |
| F1.22-B | Propagate capture write/error/session state | F4-008; capture module | Counter advances only on success | No new large block | SD failure/full/cancel/close tests | Capture gate passes |
| F1.22-C | Add pre-capture UI and integration | PR-03; scenes/settings | Main-thread edits/confirmation | Existing views | Filtered lab capture/Wireshark/repeat | APP_BEHAVIOR/docs updated |

20. **Acceptance:** Only matching frames are written, errors stop/report consistently, output remains valid PCAP, and exit restores filter/file state.
21. **Regression:** Unfiltered Sniffer, auto responders, filter baseline, FileBrowser/View Packets.
22. **Summary:** Extend Sniffer; Wave 3; high confidence.

## 33. F1.23 — Link integrity diagnostics

1. **Historical intent:** Show link up/down, 10/100, full/half duplex, and relevant IEEE 802.3/PHY diagnostics.
2. **Status:** PARTIALLY IMPLEMENTED; the 10/100 branch is OBSOLETE. Current source reads link only; ENC28J60 is fixed 10BASE-T.
3. **Disposition:** NEW STANDALONE USER FEATURE in Administration because it has a hardware-only workflow distinct from Get IP.
4. **Mapping:** Reuse ENC instance/mutex and current link read. New PHY-register access must remain inside driver boundary.
5. **Gap:** Supported duplex/config/status/PHY error interpretation, refresh/snapshot UI, documented unsupported metrics.
6. **Placement:** Driver exposes typed diagnostic snapshot; scene renders it. No protocol module or network state mutation.
7. **RX:** None.
8. **TX:** None; PHY register reads/writes only if explicitly justified.
9. **Worker:** Prefer no worker or a bounded timer/main-thread refresh; if blocking reads require one, use one owner-tagged worker.
10. **Filter:** None. Must not disturb RX/filter state.
11. **Network:** Controller availability only; link may be down; no IP/gateway.
12. **Data/identity:** Ephemeral controller/PHY snapshot with validity flags.
13. **Persistence:** None.
14. **UI:** New Administration “Link Diagnostics” item with supported/unsupported fields and refresh.
15. **Resources:** Small snapshot; no large heap/registry slot. Do not increase main stack.
16. **Backlog:** PR-02/04; F4-013/027/032; external PHY/HAL facts.
17. **Phase 8:** ENC operations are HEALTHY; connectivity naming is simplify-first; feature uses exact typed prerequisites.
18. **External:** ENC28J60 datasheet/errata and board/switch duplex behavior — authoritative research required.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.23-A | Define supported diagnostic snapshot from authoritative PHY facts | PR-04; driver/HARDWARE | No 100M claim; no raw register UI | Small struct | Datasheet-to-field review | Hardware contract approved |
| F1.23-B | Add mutex-protected driver API | ENC driver | No scene register access/state mutation | Negligible | Link up/down/duplex lab checks | Driver review |
| F1.23-C | Add diagnostics scene | Admin scenes | Clear stale/unsupported state | Existing GUI | Cable/no-cable/re-enter/relaunch | Canonical docs updated |

20. **Acceptance:** Reported facts match supported PHY state; no 100M capability is implied; diagnostics do not disturb normal networking.
21. **Regression:** Controller startup, link checks, RX IRQ/filter state.
22. **Summary:** Partial; new Link Diagnostics feature; Wave 2; high placement confidence.

## 34. F1.24 — Stealth mode (TX disable)

1. **Historical intent:** Use `PHCON2.TXDIS` to make passive capture transmit nothing, historically to eliminate capture fingerprints.
2. **Status:** NOT PREVIOUSLY ADOPTED. Constant exists but no driver API/ownership mode.
3. **Disposition:** ADD MODE / SUBMODE to Sniffer as “Stealth Capture,” not a global ambiguous Settings toggle.
4. **Mapping:** Existing Sniffer lifecycle naturally owns activation/restoration. Permanent auto responders remain registered but hardware TX suppression must have defined effects.
5. **Gap:** Mutex-protected scoped TX-disable operation, prior-state restore, UX warning, interaction with link/duplex/auto responders and failed cleanup.
6. **Placement:** Sniffer capture-session option; PR-04 owns controller-mode token/snapshot.
7. **RX:** Existing Sniffer registered consumer.
8. **TX:** Hardware-disabled for session; no feature send. Behavior of attempted automatic replies must be explicit and counted/ignored safely.
9. **Worker:** Existing Sniffer worker activates after ownership acquisition and restores before join completion/file teardown finalization as contract dictates.
10. **Filter:** Promiscuous plus TX-disable are one scoped controller-state transaction, fully restored on cancel/error/app exit.
11. **Network:** ENC/link only; no IP. Mode must not rewrite configured network tuple.
12. **Data/identity:** Capture session mode/status only.
13. **Persistence:** Optional preference only after settings semantics are corrected; default should not silently persist until product decision.
14. **UI:** Sniffer mode/confirmation clearly states receive-only limitations.
15. **Resources:** No large allocation; same Sniffer worker/File; possible small controller-state token.
16. **Backlog:** PR-03/04/06; F4-008/029/032 and hardware contract.
17. **Phase 8:** Filter/controller ownership is SIMPLIFY FIRST; Sniffer is constrained extension seam.
18. **External:** ENC TXDIS exact behavior, errata, recovery after reset/link changes — research required.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.24-A | Establish TX-disable hardware/ownership contract | PR-04; ENC/HARDWARE research | Snapshot/restore; no raw scene PHY write | Small state token | Register/reset/error reasoning and bench plan | Hardware decision approved |
| F1.24-B | Integrate scoped mode into capture session | PR-03/06; Sniffer | Restore on every failure/cancel/app exit | Existing resources | No-TX measurement, capture, forced failure tests | Lifecycle gate passes |
| F1.24-C | Add UI/behavior documentation | scenes | Explicit mode, not generic “connected” state | Negligible | BACK/repeat/normal-after-stealth | Canonical docs updated |

20. **Acceptance:** Physical test shows no application TX during the scoped session and normal TX returns afterward, including error/cancellation paths.
21. **Regression:** Auto ARP/ICMP, link startup, normal Sniffer, DHCP and later active features.
22. **Summary:** Sniffer submode after PR-04; Wave 3; medium confidence pending hardware validation.

## 35. F1.25 — Passive OS fingerprint

1. **Historical intent:** Infer OS passively in a p0f-style manner, complementing active OS Detector.
2. **Status:** NOT PREVIOUSLY ADOPTED. Active OS Detector does not satisfy passive behavior.
3. **Disposition:** MERGE / CONSOLIDATE into Traffic Monitor’s Passive OS mode using S1; do not extend the current direct-RX OS architecture.
4. **Mapping:** Reuse bounded IP/TCP parsing only after independent validation; active OS result UI may inspire labels but not RX/lifecycle.
5. **Gap:** Passive signature feature extraction, licensed/provenance-reviewed signature corpus, confidence/evidence model, per-host aggregation and details.
6. **Placement:** S1 classifier over observed traffic and Traffic Monitor UI. It is not Passive Discovery’s link-neighbor model.
7. **RX:** S1 registered observation.
8. **TX:** None.
9. **Worker:** Traffic Monitor owner; classification off callback if expensive; unregister before signature/result state free.
10. **Filter:** PR-04 scoped broad reception.
11. **Network:** ENC/link only; no local IP required.
12. **Data/identity:** MAC/IP host plus observed signature evidence, timestamp/count, confidence and ambiguity; no definitive OS without evidence.
13. **Persistence:** Session-only initially; S4 may export attributed findings.
14. **UI:** Passive OS mode and host details under Traffic Monitor; active OS Detector remains separate.
15. **Resources:** Bounded host/signature state; corpus storage placement and lookup cost require measurement; no unbounded packet retention.
16. **Backlog:** PR-01/03/04; active OS F4-001–003 independent but must be resolved before sharing code; F4-027–029/034.
17. **Phase 8:** New S1 avoids blocked active OS RX and redesign-first PCAP analyzer.
18. **External:** p0f method/signature licensing/provenance and modern TCP fingerprint research required.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.25-A | Define fingerprint evidence and corpus policy | Standards/license research; S1 | Qualified inference; no copied incompatible corpus | Measure corpus/lookup cost | Signature fixtures and ambiguity cases | Legal/data decision approved |
| F1.25-B | Implement bounded extractor/classifier | PR-01/04; S1/protocol tools | Strict lengths; no packet retention/direct RX | Bounded host evidence | Malformed/fragment/option vectors | Backend/resource gate |
| F1.25-C | Add Passive OS UI and S4 hook | PR-03; Traffic Monitor/S4 | Main-thread UI; provenance retained | Shared S1 records | Physical mixed-host/cancel tests | Canonical docs updated |

20. **Acceptance:** Supported signatures produce reproducible qualified results; unknown/ambiguous hosts remain so; malformed traffic is safe.
21. **Regression:** S1 counters/top talkers, active OS Detector, RX latency/stack, filters.
22. **Summary:** Traffic Monitor mode; Wave 3; medium confidence pending corpus research.

## 36. F1.26 — On-device PCAP analyzer

1. **Historical intent:** Enhance View Packets with filters, approved credential-related FTP/HTTP metadata, and TLS metadata.
2. **Status:** PARTIALLY IMPLEMENTED. FileBrowser, 2,000-entry index, packet navigation, and generic textual analysis exist; current parsing/index validation is unsafe.
3. **Disposition:** HARDEN / COMPLETE CURRENT IMPLEMENTATION after PR-07; no expansion before correctness and session redesign.
4. **Mapping:** Existing Browser/Read PCAP scenes and capture module remain user owner; `analysis_module.c` is not the safe future parser boundary.
5. **Gap:** Fully validated PCAP global/record parsing, bounded protocol parse representation, filter/search UI, safe metadata extractors, explicit privacy scope.
6. **Placement:** S5 validated parser/session backend, View Packets UI. Do not route stored packets through live S1/Passive handlers without lifetime proof.
7. **RX:** None; stored file input only.
8. **TX:** None.
9. **Worker:** One PCAP reader owner plus SDK FileBrowser contract; cancel/join/close/free order explicit.
10. **Filter:** No ENC filter impact.
11. **Network:** No controller/link/IP prerequisite for offline analysis.
12. **Data/identity:** Validated packet record and protocol metadata with source offsets/lengths; sensitive fields explicitly classified/redacted.
13. **Persistence:** Reads existing PCAP; analysis filters may be session-only. No modified PCAP until separately designed.
14. **UI:** Existing View Packets gains filter/summary/detail modes after parser safety.
15. **Resources:** Reassess current 16,000-byte contiguous index/2,000 limit, 4 KiB worker, text growth and packet buffer; keep feature-scoped lifetime.
16. **Backlog:** PR-07; F4-005–007/026–031 are hard blockers; F4-034 relevant to generic frame parsing assumptions.
17. **Phase 8:** PCAP parser/analyzer is REDESIGN BEFORE MAJOR EXPANSION / BLOCKED BY CORRECTNESS.
18. **External:** PCAP variants/link types, TLS record metadata, FileBrowser quiescence, and privacy requirements.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.26-A | Complete PR-07 PCAP structural parser/session | F4-005–007; capture/read scenes | Validate before index/read; one session owner | Re-evaluate index/block | Corpus of truncated/invalid/valid PCAPs | P0/P1 findings resolved |
| F1.26-B | Define S5 bounded packet representation | Protocol tools | Parse/render separation; every field length-proven | Fixed packet/metadata bounds | Ethernet/ARP/IP/TCP/UDP malformed vectors | Parser review |
| F1.26-C | Rebuild current packet details on S5 | PR-03; reader UI | No unsafe legacy fallback | Bounded text growth | Output parity and navigation/cancel tests | Existing behavior restored safely |
| F1.26-D | Add filters and approved metadata analyzers incrementally | S5; standards/privacy research | One protocol per review; no credential overclaim | Per-analyzer bounded state | Protocol fixtures and redaction tests | Each analyzer separately accepted/docs updated |

20. **Acceptance:** Malformed files/frames cannot exceed bounds; navigation and cancellation are stable; every displayed field has validated provenance.
21. **Regression:** FileBrowser, Sniffer-produced PCAPs, 16 KiB coexistence, offline use without add-on.
22. **Summary:** Partial; redesign/harden View Packets; Wave 5; high confidence.

## 37. F1.27 — PCAP-to-report auto-summary

1. **Historical intent:** Produce a report summarizing hosts, services, approved credential-related metadata, and DNS queries from a PCAP.
2. **Status:** NOT PREVIOUSLY ADOPTED.
3. **Disposition:** EXTEND EXISTING FEATURE after F1.26, using S5 parsing and S4 report/export model.
4. **Mapping:** Existing reader supplies file selection/navigation only; no safe aggregate or report writer exists.
5. **Gap:** Streaming validated aggregation, identity/provenance, bounded host/service/DNS records, privacy/redaction rules, report UI/export.
6. **Placement:** View Packets “Summary” action; S5 reads packets, S4 receives attributed findings and streams report output.
7. **RX:** None.
8. **TX:** None.
9. **Worker:** One PCAP analysis owner; cancellation between records, close/join/free before scene exit.
10. **Filter:** None.
11. **Network:** None; fully offline.
12. **Data/identity:** S4 asset/service/query observations keyed with PCAP file and record-offset provenance; no unsupported topology inference.
13. **Persistence:** New explicit report export, streamed and versioned/formatted; source PCAP remains unchanged.
14. **UI:** Summary progress, bounded on-device overview, export result/error; no new top-level feature.
15. **Resources:** Must stream over PCAP rather than retain every packet; bounded aggregate table plus current/revised index; guard total/max block.
16. **Backlog:** PR-05/07; F4-005–008/017/026–031/033; hard dependency F1.26.
17. **Phase 8:** Builds only after PCAP redesign; reuses S4 instead of duplicating asset/report state.
18. **External:** Report format/privacy, DNS/TLS/application metadata standards, filesystem replacement behavior.
19. **Subphases:**

| ID | Objective | Prereq/areas | Invariant/no shortcut | Resource | Acceptance/validation | Docs/gate |
|---|---|---|---|---|---|---|
| F1.27-A | Define bounded summary schema/provenance/privacy | F1.26/S4/S5/PR-05 | No unsupported inference or secret exposure | Exact aggregate maxima | Mixed-PCAP expected summaries | Product/data policy approved |
| F1.27-B | Implement streaming aggregation | Safe S5 reader | Validate every record; cancel/close cleanly | Bounded aggregate, no packet archive | Large/truncated PCAP, cancellation, headroom | Backend gate passes |
| F1.27-C | Add summary UI and streamed export | PR-03/05; View Packets/storage | Failure surfaced; source unchanged | Small write buffer | Full SD, reopen/export parser tests | Behavior/format docs updated |

20. **Acceptance:** Reports are reproducible, bounded, attributable to validated records, externally readable, and safe on malformed or interrupted input.
21. **Regression:** F1.26 navigation, S4 inventory identity, FileBrowser, storage replacement, memory coexistence.
22. **Summary:** Extend View Packets after F1.26; Wave 5; medium-high confidence.

## 38. Full F1 master matrix

Abbreviations: `Reg` = RX Dispatch registration; `Scan` = scanner bounded wait; `Obs` = S1 observation; `File` = offline PCAP; `—` = none/not applicable. “1W” means one owner-tagged app worker; exact future stack size requires implementation call-depth/high-water validation.

| F1 | Capability | Historical status | Current implementation | Remaining gap | Disposition | User placement | Backend | RX | Worker | Filter | Network prerequisite | Data/persistence | Phase 8 readiness | F4 blockers | Foundation | Wave | Subphases | Risk | Confidence |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1.1 | LLDP | Implemented/superseded | Passive LLDP | None material | No new work | Passive mode | Existing handlers/DB | Reg | Existing 1W | Multicast scoped | ENC+link | Passive History | Constrained existing seam | 023/024 | — | Current | None | Low | High |
| 1.2 | CDP | Implemented/superseded | Passive CDP | None material | No new work | Passive mode | Existing handlers/DB | Reg | Existing 1W | Multicast scoped | ENC+link | Passive History | Constrained existing seam | 023/024 | — | Current | None | Low | High |
| 1.3 | EAPOL | Implemented/superseded | Passive EAPOL | None material | No new work | Passive mode | Existing handlers/DB | Reg | Existing 1W | Multicast scoped | ENC+link | Passive History | Constrained existing seam | 023/024 | — | Current | None | Low | High |
| 1.4 | VLAN sniffer | Partial | LLDP VLAN only | Traffic-wide tags | Merge | Traffic Monitor/VLAN | S1 | Obs | 1W | Broad scoped | ENC+link | Session table | New constrained seam | 027–029/034 | PR-01/03/04 | 3 | A–C | Med | Med-high |
| 1.5 | mDNS harvest | Not adopted | None | DNS-SD stack/results | Merge | Service Discovery/mDNS | S2 | Reg | 1W | Multicast | ENC+link | Session services | New model avoids Passive | 027–029/032/034 | PR-01/03/04 | 2 | A–C | Med | Med |
| 1.6 | DHCP fingerprint | Not adopted | DORA only | Passive PRL/signatures | Merge | Traffic Monitor/DHCP | S1 | Obs | 1W | Broadcast scoped | ENC+link | Session clients | New constrained seam | 027–029/034 | PR-01/03/04 | 3 | A–C | Med | Med |
| 1.7 | DHCP tracker | Not adopted | Analyzer label only | Bounded live log | Merge | Traffic Monitor/DHCP | S1 | Obs | 1W | Broadcast scoped | ENC+link | Session ring/export | New constrained seam | 027–029 | PR-01/03/04/05 | 3 | A–B | Med | High |
| 1.8 | SSDP | Not adopted | None | M-SEARCH/profile/UI | Merge | Service Discovery/SSDP | S2 | Reg | 1W | Multicast | IPv4 local link | Session services | Scanner constrained | 011–016/027–029 | PR-01–04 | 2 | A–C | Med | High |
| 1.9 | Traceroute | Not adopted | Ping primitives | TTL probes/hops | Add mode | Ping/Trace Route | scanner+ICMP | Scan | 1W | Baseline | IPv4+gateway as needed | Session hops | Scanner constrained | 011–016/027–029 | PR-01–03 | 2 | A–C | Med | High |
| 1.10 | rDNS sweep | Not adopted | Saved hosts | DNS/PTR/resolver | Extend | Scan Hosts | S2 | Scan | 1W | Baseline | IPv4+DNS route | Names; storage undecided | Last-scan fix first | 017–022/027–029 | PR-01/02/03/05 | 2 | A–C | High | Med |
| 1.11 | WoL sender | Not adopted | None | Builder/action | Extend | Scan Hosts/Wake | Packet tools | — | None/1W | None | Link+MAC; tuple if directed | None | ENC healthy/edit simplify | 015/016/029/030 | PR-02/03 | 2 | A–B | Low-med | High |
| 1.12 | WoL capture | Not adopted | MPEN constant | Detector/filter/UI | Merge | Traffic Monitor/WoL | S1 | Obs | 1W | MPEN/broad scoped | ENC+link | Session events | Filter simplify first | 027–029/032/034 | PR-01/03/04 | 3 | A–B | Med | Med |
| 1.13 | Banner grab | Partial | Legacy uncalled handshake | Safe TCP stream/banner | Add mode | Ports/Service | S3 | Reg | 1W | Baseline | IPv4 route | Session/S4 result | New backend needed | 001–004/011–016/027–029 | PR-01–03 | 2 | A–C | High | Med-high |
| 1.14 | UDP services | Partial/candidate | Dormant generic UDP | Protocol profiles/results | Complete | Ports/UDP Services | S2/S6 | Scan/Reg | 1W | Profile-specific | IPv4 route | Session/S4 result | Fix current path first | 009/010/011–016/029 | PR-01–03 | 2 | A–D | High | High |
| 1.15 | SYN scan | Partial | Active SYN scan | Cleanup/result/UI hardening | Complete | Ports/TCP SYN | S3 | Scan | Existing 1W | Baseline | IPv4 route | Session/S4 result | Scanner constrained | 011–016/027–029 | PR-01–03 | 2 | A–C | Med | High |
| 1.16 | FIN/NULL/Xmas | Partial | OS probe builders | Port modes/classifiers | Add modes | Ports/Advanced TCP | S3 | Scan | 1W | Baseline | IPv4 route | Session/S4 result | Correctness first | 001/002/027–029 | PR-01–03 | 2 | A–C | Med-high | Med-high |
| 1.17 | SNMP audit | Not adopted | None | S6/candidates/UI | Merge | SNMP Tools/Audit | S6 | Scan | 1W | Baseline | IPv4 route | Session; wordlist file | FileBrowser external | 011–018/027–031 | PR-01–03/05 | 4 | A–C | High | Med |
| 1.18 | SNMP walk | Not adopted | None | Walk/MIB/data/UI | Merge | SNMP Tools/Walk | S6/S4 | Scan | 1W | Baseline | IPv4 route | Bounded tables/export | New backend/storage | Same as 1.17 | PR-01–03/05 | 4 | A–C | High | Med |
| 1.19 | Top talkers | Not adopted | Total count only | Ranking/chart | Merge | Traffic Monitor/Top | S1 | Obs | 1W | Broad scoped | ENC+link | Session snapshot | GUI external/constraints | 027–029/032/034 | PR-01/03/04 | 3 | A–C | Med | Med-high |
| 1.20 | Host counters | Not adopted | None | S1 engine/time buckets | New backend | Traffic Monitor/Counters | S1 | Obs | 1W | Broad scoped | ENC+link | Bounded live table | Resource constrained | 026–029/032/034 | PR-01/03/04 | 3 | A–C | High | High |
| 1.21 | Asset export | Not adopted | Dispersed results | S4/merge/export | Standalone | Asset Inventory | S4 | — | None/1W export | None | None to browse | New bounded store/export | Persistence fix first | 017–024/026–031/033 | PR-05 | 4 | A–D | High | Med |
| 1.22 | Filtered PCAP | Partial | Live broad capture | Filter config/error path | Add mode | Sniffer | Capture session | Reg | Existing 1W | Promisc+software | ENC+link | PCAP | Sniffer constrained | 008/026–029/032/034 | PR-03/04/06 | 3 | A–C | Med | High |
| 1.23 | Link diagnostics | Partial/10/100 obsolete | Link up check | PHY snapshot/UI | Standalone | Admin/Link Diagnostics | ENC driver | — | None preferred | None | ENC only | None | ENC healthy | 013/027/032 | PR-02/04 | 2 | A–C | Med | High |
| 1.24 | Stealth TX off | Not adopted | TXDIS constant | Scoped controller mode | Add mode | Sniffer/Stealth | ENC+capture session | Reg | Existing 1W | Promisc+TXDIS | ENC+link | PCAP | Filter simplify first | 008/029/032 | PR-03/04/06 | 3 | A–C | High | Med |
| 1.25 | Passive OS | Not adopted | Active OS only | Signatures/evidence/UI | Merge | Traffic Monitor/OS | S1 | Obs | 1W | Broad scoped | ENC+link | Session/S4 evidence | New seam; corpus external | 001–003/027–029/034 | PR-01/03/04 | 3 | A–C | High | Med |
| 1.26 | PCAP analyzer | Partial | Unsafe viewer/analyzer | PR-06/S5/features | Complete | View Packets | S5 | File | Existing 1W | — | None | PCAP + bounded metadata | Redesign/correctness first | 005–007/026–031/034 | PR-07 | 5 | A–D | Critical | High |
| 1.27 | PCAP report | Not adopted | None | Streaming S4 summary/export | Extend | View Packets/Summary | S4/S5 | File | 1W | — | None | Report export | Depends on redesign | 005–008/017/026–031/033 | PR-05/07 | 5 | A–C | High | Med-high |

## 39. Per-F1 implementation subphase summary

| Item | Current implementation sequence |
|---|---|
| F1.1–F1.3 | No implementation subphases; retain current Passive ownership and address existing F4 work independently |
| F1.4 | Scope/data contract → S1 VLAN classifier → Traffic Monitor UI |
| F1.5 | DNS-SD model/parser → passive mDNS collection → Service Discovery UI |
| F1.6 | Fingerprint/provenance contract → DHCP classifier → evidence UI |
| F1.7 | Bounded client aggregation → tracker UI/export hook |
| F1.8 | SSDP profile/parser → M-SEARCH collection → UI |
| F1.9 | Probe/result model → ICMP trace → UDP variant/UI |
| F1.10 | DNS/PTR codec → resolver/sweep → UI/persistence decision |
| F1.11 | Magic-packet backend → saved/direct host action |
| F1.12 | Detector/filter policy → Traffic Monitor integration |
| F1.13 | Registered TCP session → banner profiles → Ports UI/S4 hook |
| F1.14 | Safe selector/result API → initial UDP profiles → shared mDNS/SSDP/SNMP reuse → UI |
| F1.15 | SYN semantics → safe result events → interoperability hardening |
| F1.16 | Builders/classifiers → registered range profiles → advanced UI |
| F1.17 | SNMP codec → candidate session → Community Audit UI |
| F1.18 | Walk state machine → MIB/S4 mapping → Walk UI/export |
| F1.19 | Ranking contract → chart/list → physical traffic validation |
| F1.20 | S1 resource/identity contract → aggregation backend → counters UI |
| F1.21 | S4 identity → producer adapters → browse → streaming CSV/JSON |
| F1.22 | Filter IR → capture error/session correction → Sniffer UI |
| F1.23 | PHY fact model → driver API → diagnostics scene |
| F1.24 | TXDIS contract → scoped capture integration → UI/docs |
| F1.25 | Corpus/evidence policy → bounded classifier → Passive OS UI/S4 hook |
| F1.26 | Safe PCAP session → S5 parse representation → current details → incremental analyzers |
| F1.27 | Summary schema → streaming aggregation → UI/export |

Every future-work item is divided into independently reviewable subphases. A subphase is not complete until its table’s acceptance, validation, documentation, and gate conditions are met.

The sequences above define F1 capability increments. They do not imply that every named PR-* gate requires implementation.

Before the first subphase of a capability is planned, its applicable PR-* gates must be evaluated against current source.

Likewise, an S* responsibility is implemented only when required by an approved consumer and only to the extent required by the current shared contract.

## 40. Roadmap execution protocol

Future agents must execute one subphase at a time:

1. Select exactly one approved roadmap subphase such as `F1.14-B`.
2. Re-read that dossier and the authoritative current documents.
3. Rediscover and verify the current source-impact set, reachability, resources, APIs, and Git state. Paths or modules named in this roadmap are architectural guidance, not an exhaustive future edit list.
4. Evaluate only the PR-* gates, S* responsibilities, F4 findings, external-standard requirements, and SDK/HAL contracts applicable to the selected subphase.
5. For every applicable PR-* gate, record whether it is SATISFIED, PARTIALLY SATISFIED, BLOCKING, or NOT APPLICABLE from current source evidence. Do not implement prerequisite work merely because a PR-* identifier is listed.
6. If a prerequisite is BLOCKING, stop F1 implementation and define a bounded prerequisite correction separately. Do not hide prerequisite correction inside the F1 implementation diff.
7. If all hard gates are satisfied, produce a focused implementation plan for the selected F1 subphase only.
8. The focused implementation plan maps the already-approved roadmap design onto current source: exact files, current APIs, bounded edits, build steps, and validation steps. It must not redesign the complete capability.
9. Implement only that bounded increment; do not opportunistically begin adjacent modes, unrelated prerequisites, shared backends, or F1 items.
10. Build and perform the specified static, malformed-input, cancellation, physical, transition, storage, filter, and memory checks as applicable.
11. Review the complete diff against ownership, RX, filter, worker, buffer, storage, and resource invariants.
12. Update `APP_BEHAVIOR`, `ARCHITECTURE`, `HARDWARE`, `DECISIONS`, `BACKLOG`, and this roadmap only where the accepted change alters their owned truth.
13. Record subphase status and evidence without rewriting historical sources.
14. Stop for user review and manual commit.
15. Begin another subphase only after acceptance.

### Execution status and evidence

Execution units use the following lifecycle states:

- **NOT EVALUATED**
- **READY**
- **BLOCKED**
- **IN PROGRESS**
- **IMPLEMENTED**
- **VALIDATED**
- **SUPERSEDED**

When an execution unit changes state after actual roadmap execution, record:

- Status
- Completion commit, when applicable
- Validation evidence
- Canonical documentation impact
- Remaining blockers

Do not pre-mark future PR-* or F1.x-* units READY solely from this roadmap. Readiness is established from the repository state at execution time.

### Scope discipline

A future implementation agent must not reinterpret the complete historical F1 roadmap while executing one approved subphase.

It must not:

- reopen historical F0;
- redesign unrelated F1 placement;
- renumber historical F1 items;
- create new global prerequisite phases;
- implement unrelated S* backends speculatively;
- expand the selected subphase because adjacent work appears convenient.

Architectural reconsideration requires an explicit roadmap amendment before implementation.

The roadmap is a planning authority only after user review. Current source remains implementation truth, and current canonical documentation remains authoritative for behavior and architecture. If source or backlog changes invalidate a dossier, reconcile that dossier before implementation rather than following stale instructions.
