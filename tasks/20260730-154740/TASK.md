# Add Epic graph, frontier, claims, and phase context

- PRIORITY: 85
- TAGS: feature, flow, epic, parallel
- ACTIVITY: COMPOUNDING
- GATES: PLAN REVIEW RETRO
- RESOLUTION: DONE
- DEPENDS ON: 20260730-153325, 20260730-154657, 20260730-154745

## Story

As a flow driver, I want tatr to model Epic/Story relationships, dependencies,
frontiers, claims, and phase context, so large work can be divided across
contexts and parallel sessions without loading or editing the whole task map.

## Steps

- [x] Inspect the v2 metadata parser, the lifecycle commands from
      20260730-154657, and the artifact schema/proof commands from
      20260730-154745 before choosing command names or output formats.
- [x] Implement task graph loading over PARENT and DEPENDS ON, including
      existence checks, duplicate dependency checks, self-dependency checks,
      dependency cycles, parent cycles, and Epic/Story relationship
      consistency.
- [x] Add an Epic frontier command that prints deterministic open work under an
      Epic: unblocked Stories first, blocked Stories with blocker IDs visible,
      and no expanded task bodies.
- [x] Add claim, release, and claim-inspection commands using an atomic
      filesystem operation; record enough owner and timestamp data to diagnose
      stale claims and recover them deliberately.
- [x] Wire graph and claim guards into lifecycle start/close paths so blocked
      Stories cannot start, claimed Stories cannot be double-started, and an
      Epic cannot close while required Stories remain open.
- [x] Add `tatr context <id> --phase <phase>` that prints only the task and
      sibling artifact paths needed for understand, plan, work, review,
      compound, resume, and landing phases.
- [x] Add integration tests for graph validation, frontier ordering, claim
      contention, stale-claim recovery, lifecycle guard integration, Epic close,
      and phase context selection.
- [x] Update README.md, AGENTS.md, CHANGELOG.md, and `skills/tatr/SKILL.md`
      from the implemented commands and output.

## Definition of Done

- Graph checks catch missing links, duplicate dependencies, self-links, cycles,
  and parent/child mismatches (test: `test_epic_graph_validation`).
- Frontier output is deterministic and separates unblocked, blocked, and
  claimed Stories without printing description bodies (test:
  `test_epic_frontier`).
- Exactly one concurrent claimant succeeds, active claims block competing
  starts, and stale claims are recoverable only through an explicit command
  (test: `test_atomic_claim`).
- Lifecycle commands refuse blocked Story starts and open-child Epic closes
  (test: `test_epic_lifecycle_guards`).
- Phase context output lists only the artifacts owned by that phase (test:
  `test_phase_context_selection`).
- Full native and memory-check suites pass
  (cmd: `nix develop -c ./checker.sh && nix develop -c ./checker.sh --memcheck`).

## Notes

- Parent Epic: nix.dotfiles task 20260730-153122.
- Depends on: 20260730-153325, 20260730-154657, and 20260730-154745.
- 20260730-154657 is the approved lifecycle-command prerequisite; this task
  should extend those guards rather than adding a second start/close path.
- 20260730-154745 is the artifact-schema prerequisite; phase context should
  list schema-owned sibling artifacts, not duplicate schema parsing.
- `Epic index` or `context map` is the local name; avoid "low-resolution map".
- Decision to defer to implementation: the exact claim file name and payload
  can be chosen after a small local concurrency probe, but the mechanism must
  rely on an atomic filesystem operation and must be documented in README.md.
