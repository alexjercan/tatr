# Add Epic graph, frontier, claims, and phase context

- STATUS: OPEN
- PRIORITY: 85
- TAGS: feature,flow,epic,parallel
- KIND: TASK
- FLOW STEP: BACKLOG
- PLAN STATUS: DRAFT
- DEPENDS ON: 20260730-153325, 20260730-154657

## Story

As a flow driver, I want tatr to model Epic/Story relationships, dependencies,
frontiers, claims, and phase context, so large work can be divided across
contexts and parallel sessions without loading or editing the whole map.

## Steps

- [ ] Implement parent/dependency graph loading from the v2 schema and reject
      missing references, self-dependencies, cycles, and inconsistent
      Epic/Story relationships.
- [ ] Add an Epic index/frontier view that prints Destination/decision
      pointers plus open, unblocked, unclaimed Stories without expanding their
      bodies.
- [ ] Add an atomic claim/release mechanism suitable for concurrent sessions,
      with clear stale-claim inspection and recovery.
- [ ] Prevent starting blocked Stories and closing an Epic while required
      Stories remain open.
- [ ] Add `tatr context <id> --phase <phase>` to emit the minimal task and
      sibling-artifact path set for planning, work, review, compound, and
      resume; do not concatenate every file into the output.
- [ ] Add integration tests for graph validation, frontier ordering, claim
      contention/recovery, Epic close, and context selection.
- [ ] Update README.md, AGENTS.md, CHANGELOG.md, and `skills/tatr/SKILL.md`
      from the implemented commands.

## Definition of Done

- Graph checks catch missing links, cycles, and parent/child mismatches
  (test: `test_epic_graph_validation`).
- Frontier returns only open, unblocked, unclaimed Stories in deterministic
  order (test: `test_epic_frontier`).
- Exactly one concurrent claimant succeeds and stale claims are recoverable
  (test: `test_atomic_claim`).
- Phase context output lists only the artifacts owned by that phase
  (test: `test_phase_context_selection`).
- Full native and memory-check suites pass
  (cmd: `nix develop -c ./checker.sh && nix develop -c ./checker.sh --memcheck`).

## Notes

- Parent Epic: nix.dotfiles task 20260730-153122.
- Depends on: 20260730-153325, 20260730-154657.
- `Epic index` or `context map` is the local name; avoid "low-resolution map".
