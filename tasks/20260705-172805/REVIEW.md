# Review: Add rm command

- VERDICT: APPROVE

## Scope reviewed
- `main_rm` (reuses `task_resolve`, `task_dir_path_build`, `aids_io_listdir`,
  `cleanup_string_slice_array`).
- Dispatch + help wiring.
- Five new checker.sh tests.

## Findings

### Correctness / safety
- Only ever operates on `tasks/<id>/` after `task_resolve` has validated the
  HUID and confirmed the directory exists, so a bad or crafted ID cannot make it
  touch anything else.
- Removes every entry in the task dir (not just TASK.md) before `rmdir`, so it
  correctly deletes dirs that also hold a REVIEW.md - verified by
  `test_rm_nonempty_dir`. `.`/`..` are skipped explicitly.
- Siblings are untouched (`test_rm_preserves_siblings`).
- Error handling: invalid id, missing task and missing positional all exit
  non-zero with clear messages.
- valgrind is clean across all 45 tests; the listdir name array is freed via
  `cleanup_string_slice_array`.

### Notes
- If a task dir ever contained a *subdirectory*, `unlink` would fail on it and
  rm would abort with a clear errno message rather than silently mishandle it.
  Task dirs are flat by design, so this is a safe failure mode, not a bug.
- Uses POSIX `unlink`/`rmdir` directly (from `<unistd.h>`, already included)
  because aids.h has no remove primitive.

## Tests
- rm existing, rm dir with extra files, rm preserves siblings, rm invalid id,
  rm missing task. All pass under `--memcheck`.
