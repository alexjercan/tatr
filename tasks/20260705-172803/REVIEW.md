# Review: Add show command

- VERDICT: APPROVE

## Scope reviewed

- `task_resolve` helper (HUID validation + task location, shared with edit/rm).
- `task_print_full` and `main_show`.
- Dispatch + help wiring.
- Three new checker.sh tests.

## Findings

### Correctness
- Memory: valgrind (`checker.sh --memcheck`) reports no leaks across all 33
  tests. `task_resolve` transfers ownership of `tasks_dir`/`task_file_path` to
  the caller and frees the internal `task_dir`; error paths free everything.
- Error handling is clean: invalid HUID and missing task both exit non-zero
  with distinct, readable messages; a missing positional is caught by argparse.
- Behaviour is consistent with `new`/`ls`: same upward `tasks/` search and the
  same `-r ROOT` override.

### Minor (non-blocking)
- `task_print_full` re-renders the task from parsed fields rather than echoing
  the raw file, so a hand-edited TASK.md with non-canonical spacing would be
  normalized on display. This is acceptable (and arguably desirable) and never
  touches the file on disk.
- Output ends with a single trailing blank line because the parsed description
  already carries the file's terminating newline. Cosmetic only.
- `main_show` receives `tasks_dir` from the shared helper but only needs the
  file path; it frees it correctly. Kept for a uniform helper signature that
  `rm` (which needs the dir) will reuse.

## Tests
- show existing task (fields + clickable path), show invalid id (non-zero),
  show non-existent id (non-zero). All pass, including under memcheck.
