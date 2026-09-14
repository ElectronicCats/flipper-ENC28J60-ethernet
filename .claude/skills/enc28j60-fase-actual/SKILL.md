---
name: enc28j60-fase-actual
description: Operational guidance for work on the FLIPPER ETHERNET Flipper Zero application.
---

# FLIPPER ETHERNET working guidance

Use this skill to orient repository work. It is not a project-status database.

## Authority and navigation

Read the relevant canonical documents before changing behavior or architecture:

- `docs/APP_BEHAVIOR.md` defines intended user-visible behavior and records known
  user-observed deviations. It is not implementation authority.
- Current source under `EthernetAppDemo/` is the ground truth for the current
  implementation.
- `docs/ARCHITECTURE.md` maps the current implementation and invariants.
- `docs/HARDWARE.md` documents the ENC28J60 integration and separates proven
  behavior from unresolved hardware/SDK contracts.
- `docs/DECISIONS.md` records accepted decisions and their present status.
- `docs/BACKLOG.md` is the canonical list of active findings, conditional
  questions, product limits, and closed work.

Dated plans, specifications, release notes, and old debugging conclusions are
historical evidence. Do not treat them as current authority or promote a
historical hypothesis into a defect without independently proving it in current
source.

## Analysis workflow

1. Check the branch, HEAD, working-tree state, manifest, and compile-time flags.
2. Establish build-time reachability before analyzing questionable code.
3. Trace callers, state changes, ownership, lifetime, cancellation, and cleanup
   through current source.
4. Compare user-visible behavior with `docs/APP_BEHAVIOR.md`.
5. Preserve source-proven invariants and record unresolved SDK/HAL behavior as
   an external-contract question rather than an established fact.
6. Update the canonical document that owns the information; point to
   `docs/BACKLOG.md` instead of copying its finding list elsewhere.

Prefer static analysis, call-flow tracing, and explicit ownership/lifecycle
reasoning. Do not depend on Flipper CLI logging as the primary means of proving
behavior.

## Architectural invariants to preserve

- `App` owns application-lifetime state and GUI resources.
- At most one application-owned feature worker occupies the shared worker slot;
  it must stop and join before borrowed feature state is released.
- RX Dispatch is the primary receive path, but it is not universal. Direct-RX
  exceptions must be identified and coordinated explicitly.
- RX registrations remain valid until unregistration and callback quiescence are
  established.
- ENC28J60 operations use the controller instance and its synchronization
  boundary; do not reintroduce file-static bank ownership.
- Feature-specific receive-filter changes must restore the documented baseline
  state on every exit path.
- Automatic protocol responders can overlap dispatcher-active features and must
  use storage valid for their callback lifetime.
- Total free heap and largest contiguous block are distinct constraints. Do not
  claim a leak, allocator fragmentation, or stack overflow without proof.

## Change discipline

Keep implementation, intended behavior, architecture, hardware contracts,
decisions, and backlog status distinct. Preserve dated historical records rather
than silently modernizing them. Verify the final diff for unintended source,
build, workflow, license, or historical-document changes.
