# Add new body input

- STATUS: CLOSED
- PRIORITY: 100
- TAGS: cli

Add `-b, --body` to `tatr new`. Accept a Markdown file path or `-` for stdin.

## Plan

- [x] Add integration checks for file input, stdin input, and read failure without task creation.
- [x] Read the full body before task creation and preserve its bytes in `TASK.md`.
- [x] Document the new option and verify all supported toolchains and memcheck.

## Done when

- `tatr new "Title" --body file.md` uses the file as the task body.
- `printf 'body' | tatr new "Title" --body -` uses stdin as the task body.
- An unreadable body exits non-zero and creates no task.
- Required build and test commands pass.

## Result

- Added `-b, --body` to `new` for a file path or `-` for stdin.
- Read and validate the input before generating the task ID. Read failure cannot create a task directory.
- Reused the task-owned buffer for cleanup. No second body allocation.
- Added file, stdin, and missing-file integration coverage.

## Evidence

- `nix develop -c make clean all`: pass with clang.
- `nix develop -c ./checker.sh`: 8/8 pass.
- `nix develop -c ./checker.sh --memcheck`: 8/8 pass.
- `nix develop -c make clean all CC=gcc`: pass.
- `nix develop -c make windows`: pass.
- Guard mutation that removed body assignment: new body check failed.

## Reflection

- Initial check did not stop after a failed pipeline because `check` invokes test functions in a conditional context. Added explicit `|| return 1` guards to every new success assertion.
