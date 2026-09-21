---
name: enc28j60-fase-actual
description: Repository interpretation and working guidance for the FLIPPER ETHERNET Flipper Zero application.
---

# FLIPPER ETHERNET repository guidance

This skill teaches agents how to interpret and work with the FLIPPER ETHERNET
repository. It does not replace the canonical documentation or the historical
roadmap documents.

## Knowledge layers and authority

### Current truth

- Current source under `EthernetAppDemo/` is the ground truth for the current
  implementation. Establish compile-time reachability before treating code as
  active.
- `docs/APP_BEHAVIOR.md` defines intended user-visible behavior and records
  known user-observed deviations. It is not implementation authority.
- `docs/ARCHITECTURE.md` describes current software structure and invariants.
- `docs/HARDWARE.md` describes the current ENC28J60 integration and separates
  source-proven transitions from unresolved hardware/SDK contracts.

### Current project state

- `docs/DECISIONS.md` owns accepted decisions and their present status.
- `docs/BACKLOG.md` owns current unresolved work, conditional questions,
  product/resource limits, and closed history.

Point to these documents instead of copying their scene lists, settings keys,
memory tables, decisions, or findings into this skill.

### Historical design lineage

- `ENC28J60_REFACTOR_PLAN.md` preserves the high-level refactor and roadmap
  intent from its period.
- `docs/superpowers/specs/**` preserves broader historical capability and design
  specifications.
- `docs/superpowers/plans/**` preserves implementation plans for particular
  development steps.
- Git history and release records can establish when and how the repository
  evolved, but do not override current source or canonical documentation.
- Use Git history to establish implementation lineage when current source shows
  that a historical capability exists but the historical plan no longer describes
  its present architecture. Prefer the commits that introduced or materially
  reworked the capability over inferring progress from document checklists alone.

Historical plans/specifications preserve design intent and planned
implementation history. Git history and current source establish what actually
landed and how that implementation subsequently evolved.
They are not authoritative descriptions of the current implementation.

## Historical roadmap and design lineage

Consult the historical roadmap, specs, and plans when investigating roadmap
progress, feature lineage, unfinished capabilities, architectural evolution,
future roadmap redesign, or whether an old capability still needs work.

For each historical item, identify its original problem and acceptance intent,
then reconcile it in this order:

1. current source and build-time reachability;
2. `docs/APP_BEHAVIOR.md`;
3. `docs/ARCHITECTURE.md` and `docs/HARDWARE.md` as relevant;
4. `docs/DECISIONS.md`;
5. `docs/BACKLOG.md`.

Classify the item as `IMPLEMENTED`, `PARTIALLY IMPLEMENTED`, `SUPERSEDED`,
`OBSOLETE`, `HISTORICAL / NOT CURRENTLY ADOPTED`, or `CURRENT CANDIDATE`,
with evidence. An unchecked historical checklist item is not proof that work
remains, and a similarly named module is not proof that the intended capability
landed.

Use `CURRENT CANDIDATE` only when current project evidence such as
`docs/BACKLOG.md`, `docs/DECISIONS.md`, or an explicitly adopted current
planning decision establishes that the capability is still under consideration.
Historical inclusion alone supports only `HISTORICAL / NOT CURRENTLY ADOPTED`.

## Durable implementation evolution

Use these as orientation, then verify details in current source and canonical
documents:

- Bootstrap work established the documentation, decision, issue-template, and
  ENC-driver license boundaries that later work evolved.
- Cross-scene scan configuration moved into `App.scan_params`. Current source
  still has separate Ping and Ports/OS target fields, so central storage did not
  by itself deliver the intended shared-target behavior.
- Settings persistence landed and evolved beyond its first design: the current
  writer emits schema v3 and the loader accepts v1 through v3. Current save and
  editor semantics must still be compared with `APP_BEHAVIOR.md`.
- `scanner_session_t` landed, then evolved from the early polling POC into
  reusable next-hop resolution and registered/semaphore-backed packet waits.
  Its presence does not prove that every receive path uses RX Dispatch.
- RX Dispatch replaced the old application receive worker as the primary RX
  architecture and later gained ENC `/INT` wake-up. It is not universal: direct
  RX and pause boundaries must be traced from current source.
- ENC register-bank/SPI ownership and feature-worker lifecycle were hardened
  after the early plans. Current `AppThreadOwner` and shared-worker rules, not
  old per-scene thread sketches, define application-owned worker coexistence.
- Historical roadmap items LLDP, CDP, and EAPOL were described as separate
  capabilities. Current source consolidates them as modes of Passive Discovery
  under one feature worker, protocol-handler table, RX-wait path, live neighbor
  database, detail UI, and persistence model.
- Saved Passive Neighbor History was added as a later cross-session capability;
  it is not represented by the early LLDP-only decomposition.
- Sniffer and PCAP support predated the roadmap and later evolved through RX
  registration, capture/navigation hardening, and on-demand indexing. Broader
  historical PCAP-filter/analyzer/report goals are not implied by that base.
- Startup/resource guards were added during later hardening. They are advisory
  preflight checks with SDK-dependent assumptions, not allocation reservations.
- Historical dual-artifact CI, bulk-SPI, alternate controller-SRAM layout, and
  later LAN/OT/drop-box goals must be reconciled individually; their appearance
  in an approved historical design does not make them current commitments.

The historical roadmap's UI/module decomposition is therefore a lineage map,
not a required shape for future features. A formerly separate roadmap item may
now belong inside an existing feature, mode, backend, or persistence model.

## Analysis workflow

1. Check branch, HEAD, working-tree state, manifest, and compile-time flags.
2. Read the current canonical owner for the question; consult historical
   material when intent or lineage matters.
3. Trace callers, state changes, ownership, lifetime, cancellation, cleanup,
   RX/TX paths, and resource coexistence through current source.
4. Compare user-visible behavior with `docs/APP_BEHAVIOR.md` and unresolved work
   with `docs/BACKLOG.md`.
5. Verify historical debugging hypotheses independently against current source.
6. Keep SDK/HAL behavior unresolved unless an authoritative contract proves it.

Prefer static code analysis, call-flow tracing, ownership/lifecycle analysis,
and comparison with a known working path. Do not rely on Flipper CLI logging as
the primary way to prove behavior.

## Method for future roadmap analysis

For a historical capability, ask:

- What problem and user capability did it intend?
- Is that capability implemented, partial, superseded, obsolete, or unadopted?
- Which current user-facing feature is its closest owner: an extension, submode,
  or genuinely new feature?
- Which current backend, RX/TX path, and persistence model can be reused?
- Would it change RX ownership, add persistent state, or increase heap, stack,
  storage, or contiguous-block pressure?
- Which current backlog findings or external contracts block it?
- Would it conflict with documented ownership, lifecycle, filter, or resource
  invariants?

Answering these questions produces evidence for a later roadmap decision; it
does not itself approve the capability or its placement.

## Architectural invariants to preserve

- `App` owns application-lifetime state and reusable GUI resources.
- At most one application-owned feature worker occupies the shared worker slot;
  it must stop and join before borrowed feature state is released.
- RX Dispatch is primary but not universal. Direct-RX exceptions and dispatcher
  pause boundaries require explicit coordination.
- RX registration context remains valid until unregistration and callback
  quiescence are established.
- Public ENC28J60 operations use the controller instance and its synchronization
  boundary; do not reintroduce file-static bank ownership.
- Feature-specific receive-filter changes restore the documented baseline on
  every exit path.
- Automatic responders can overlap dispatcher-active features and require data
  valid for their callback lifetime.
- Total free heap and largest contiguous block are distinct constraints. Do not
  claim a leak, persistent fragmentation, or stack overflow without proof.

## Change discipline

Keep current implementation, intended behavior, current project state, and
historical intent distinct. Update the canonical owner of current information,
preserve dated historical records, and review the final diff for unintended
source, build, workflow, license, or historical-document changes.
