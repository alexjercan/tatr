# Retro: Add a Windows tatr.exe build

- TASK: 20260728-095149
- BRANCH: feature/windows-exe-build
- REVIEW ROUNDS: 1

## What went well

- The first DoD command was run before implementation and failed for the right
  reason: `make windows` did not exist. That kept the work anchored to the
  requested artifact.
- Probing MinGW directly before writing shims narrowed the C portability work to
  the real compiler error, then review caught a cleanup issue in the edited
  helper before landing.
- The docs, Makefile, flake package, and checker test all point at the same
  artifact path: `dist/tatr.exe`.

## What went wrong

- I added the build target before adding ignore rules for its generated output,
  so the review pass had to stop and clean up `/dist` and `/result` as a
  separate correction.
- The first version of the `aids_io_mkdir` edit preserved the old early-error
  cleanup shape, which could leak `current_path` if a later recursive mkdir step
  failed. Root cause: I focused on the MinGW signature mismatch and did not
  immediately re-audit ownership after expanding the helper.
- I parallelized pre-land commands that all mutate the same build outputs; one
  command cleaned `./tatr` while `checker.sh` was still running, causing a
  transient exit 127. Root cause: I treated read-only verification and
  build-mutating verification as equally parallelizable.

## What to improve next time

- For target-platform support, run the target compiler first and let the actual
  diagnostics define the first shim set.
- When a build target creates new generated paths, update `.gitignore` in the
  same edit as the target.
- After changing an allocation-owning helper, re-read the whole helper's error
  exits before committing, not just the line that failed to compile.
- Run build-mutating verification commands serially when they share artifacts in
  one worktree.

## Action items

- [x] Added `target-compiler-first` to `LESSONS.md`.
- [x] Added `serialize-build-artifact-checks` to `LESSONS.md`.
