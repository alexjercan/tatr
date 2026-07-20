# new: fail on same-second ID collision; add --body-file

- STATUS: CLOSED
- PRIORITY: 80
- TAGS: feature,historical

## What changed

- `task_create` now refuses to reuse an existing task directory: two
  `tatr new` calls in the same second used to silently overwrite the first
  task's TASK.md (this bit agent workflows seven recorded times in the
  nova-protocol repo). The second call now errors with "already exists
  (same-second ID collision); retry to get a fresh ID". Deliberately a
  failure, not a bumped-into-the-future ID: task IDs should reflect the real
  creation time.
- `tatr new -b/--body-file <path>` seeds the description body from a file,
  or from stdin with `-`. The body is read before the ID is generated, so a
  bad path fails without creating anything on disk.
- Version bumped to 0.2.0. README, AGENTS.md and checker.sh updated; three
  new integration tests (body from file incl. missing-file failure, body
  from stdin, collision refusal with retry across second boundaries).

## Why

The silent overwrite was the root cause behind repeated task-file loss in
agent-driven repos; prompt-level warnings ("never chain tatr new") only
mitigated it. Fixing the tool removes the failure mode. --body-file removes
the create-then-hand-edit dance when agents author task bodies.
