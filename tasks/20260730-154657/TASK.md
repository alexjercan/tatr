# Add transactional flow lifecycle commands and guards

- STATUS: OPEN
- PRIORITY: 90
- TAGS: feature,flow,lifecycle
- KIND: TASK
- FLOW STEP: BACKLOG
- PLAN STATUS: DRAFT
- DEPENDS ON: 20260730-153325

## Story

As a flow driver, I want tatr to perform legal state transitions
transactionally, so an agent cannot bypass plan, review, retro, or closure
requirements and leave a knowingly invalid intermediate state.

## Steps

- [ ] Define event-driven transitions over the v2 fields, including the
      REVIEWING -> WORKING fix loop and task-kind-specific exemptions.
- [ ] Add lifecycle commands for plan approval, start, phase advance, and
      close; keep a work task IN_PROGRESS through review and compound, then
      close it atomically at DONE.
- [ ] Make `tatr edit -s` reuse the lifecycle validation for flow-managed
      tasks or reject it with a pointer to the transition command.
- [ ] Validate each transition before writing: approved plan, closed
      dependencies, completed Steps, latest review APPROVE with no open
      BLOCKER/MAJOR findings, required RETRO, and applicable user/manual
      decisions.
- [ ] Preserve failure atomicity and emit actionable diagnostics naming the
      unmet precondition.
- [ ] Add integration tests for every legal transition, forbidden skip,
      review loop, exemption, and failure rollback.
- [ ] Update README.md, AGENTS.md, CHANGELOG.md, and `skills/tatr/SKILL.md`
      from real command output.

## Definition of Done

- A work task cannot start without an approved plan and closed dependencies
  (test: `test_transition_start_guards`).
- Review may return to work, but no other illegal phase skip is accepted
  (test: `test_transition_state_machine`).
- Close refuses incomplete Steps, non-APPROVE review, open blocking findings,
  or a missing retro without changing the task
  (test: `test_transition_close_is_atomic`).
- `tatr edit -s` cannot bypass the lifecycle
  (test: `test_edit_status_uses_transition_guards`).
- Full native and memory-check suites pass
  (cmd: `nix develop -c ./checker.sh && nix develop -c ./checker.sh --memcheck`).

## Notes

- Parent Epic: nix.dotfiles task 20260730-153122.
- Depends on: 20260730-153325.
- This replaces prompt-only transition discipline with a CLI guard.
