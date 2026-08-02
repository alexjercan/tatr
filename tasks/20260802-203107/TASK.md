# Remove the tatr migrate command once every backlog is on v1

- PRIORITY: 20
- TAGS: chore, cleanup
- KIND: TASK
- ACTIVITY: UNDERSTANDING
- GATES: -
- RESOLUTION: -
- DEPENDS ON: 20260802-201918

## Story

As the tatr maintainer, I want `tatr migrate` deleted once every backlog it
serves has been migrated, so the v1 binary stops carrying knowledge of the v0
record format.

`main_migrate` is the sole place in v1 that understands `FLOW STEP` and
`PLAN STATUS`. It exists to move existing repositories across the v1.0.0 break
and has no purpose after that. Task 20260802-201918 adds it deliberately as a
quarantined shim; this task is the other half of that decision.

## Notes

- Blocked until every repository with a `tasks/` directory has been migrated
  and committed. Enumerate them before starting; the count is not recorded
  anywhere.
- Removal is the command, its parser, its dispatch entry, its help line, its
  `checker.sh` fixtures, and the `tatr migrate` pointer in the
  `task_deserialize` refusal message, which becomes a plain format error.
- Ship as v1.1.0: a removed command is a breaking change to anyone who scripted
  it, but nobody should have.
