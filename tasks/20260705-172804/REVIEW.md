# Review: Add edit command

Verdict: APPROVE

## Scope reviewed
- `task_status_is_valid` helper.
- `main_edit` (reuses `task_resolve`, `task_load`, `task_save`).
- Dispatch + help wiring.
- Seven new checker.sh tests.

## Findings

### Correctness
- Partial-update semantics are correct: `argparse_get_value` returns NULL for
  any unspecified flag, so only provided fields are applied; verified by
  `test_edit_partial_preserves_fields` (status changed, priority/tags/title and
  the description body all preserved).
- The description body round-trips untouched because `task_load`/`task_save`
  reuse the same serialize/deserialize path; only whitespace is canonicalized.
- Status is validated up front (`task_status_is_valid`) and a bad value aborts
  before any write, so the file is never left half-updated. Verified the file
  is unchanged after a rejected status.
- Tag replacement resets `task.meta.tags.count = 0` and re-appends; the old tag
  slices point into the loaded buffer (not owned per-slice), so no leak or
  double free. valgrind is clean across all 40 tests.

### Notes
- `edit` validates status strictly, whereas `new` still silently defaults an
  unknown status to OPEN. Tightening `new` is out of scope for this task; worth
  a follow-up task if we want them symmetric.
- Reused `task_resolve` verbatim, as the show retro recommended.

## Tests
- status, priority, tags (replacement), title, partial-preserves-fields,
  invalid-status-rejected, missing-task. All pass under `--memcheck`.
