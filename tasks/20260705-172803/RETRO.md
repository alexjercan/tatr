# Retro: add show command (task 20260705-172803)

## What went well
- The existing helpers (`tasks_dir_path_build`, `task_dir_path_build`,
  `task_file_path_build`, `ishuid`, `task_load`) composed cleanly into a single
  `task_resolve` that show, edit and rm can all share. No new low-level I/O was
  needed.
- Matching the `main_new`/`main_ls` argparse + `defer:` cleanup pattern kept the
  new command consistent and leak-free on the first valgrind run.

## What went wrong / difficulties
- The toolchain: `make` hardcodes `CC = clang`, and this shell only has `gcc`.
  clang (and valgrind) live in the nix dev shell, so the suite has to be run as
  `nix develop -c ./checker.sh`. For a quick local build, `make CC=gcc` works.
- checker.sh gotcha (cost two failed runs): under `set -e`, a bare
  `run_tatr ...` that exits non-zero aborts the whole script, and
  `local out=$(cmd)` swallows the command's exit code (you get `local`'s status,
  always 0). The correct pattern for testing a non-zero exit is:
  ```sh
  set +e
  local output
  output=$(run_tatr ... 2>&1)
  local exit_code=$?
  set -e
  ```
  This is exactly what the existing `test_filter_error_*` tests do.

## Lessons to apply to the next tasks (edit, rm)
- Reuse `task_resolve`; do not re-implement HUID/location logic.
- Write every "should fail" test with the `set +e` / split declaration pattern
  above.
- Run the suite via `nix develop -c ./checker.sh` (add `--memcheck` for leaks).
