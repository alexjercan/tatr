# Require user disposition for lesson promotions

- STATUS: OPEN
- PRIORITY: 80
- TAGS: feature,flow,lessons
- KIND: TASK
- FLOW STEP: BACKLOG
- PLAN STATUS: DRAFT
- DEPENDS ON: 20260730-153325, 20260730-154745

## Story

As the lessons-ledger owner, I want every threshold promotion to receive an
explicit user disposition and every approved change to use the normal reviewed
task lifecycle, so lessons cannot stall forever or silently rewrite policy.

## Steps

- [ ] Define structured ledger dispositions for unresolved Pending, PROMOTE,
      DEFER, RETIRE, and ABSORBED, including date/reason and target/task
      references where applicable.
- [ ] Change ledger checking so a bare threshold entry in Pending promotions
      emits `promotion-awaiting-decision` instead of passing indefinitely.
- [ ] Validate disposition syntax, referenced promotion tasks, and named
      absorbed targets; preserve existing counts and history.
- [ ] Add a command that lists pending decisions and records a selected
      disposition without applying the promoted tool/template/skill change.
- [ ] Ensure PROMOTE creates or references a tatr task whose eventual change
      must satisfy the ordinary plan, review, retro, and close guards.
- [ ] Revalidate the tatr ledger and add integration tests for every
      disposition and failure mode.
- [ ] Update README.md, AGENTS.md, CHANGELOG.md, and `skills/tatr/SKILL.md`.

## Definition of Done

- A bare threshold entry in Pending fails conformance
  (test: `test_ledger_pending_requires_disposition`).
- Each explicit disposition parses, persists, and avoids repeated prompts
  (test: `test_ledger_dispositions`).
- PROMOTE requires a resolvable task reference and never edits the promotion
  target itself (test: `test_ledger_promote_requires_task`).
- Invalid disposition writes are atomic
  (test: `test_ledger_disposition_atomicity`).
- Full native and memory-check suites pass
  (cmd: `nix develop -c ./checker.sh && nix develop -c ./checker.sh --memcheck`).

## Notes

- Parent Epic: nix.dotfiles task 20260730-153122.
- Depends on: 20260730-153325, 20260730-154745.
- The user decision is mandatory. The CLI records it; the agent must use the
  platform user-input mechanism or ask directly before calling the command.
