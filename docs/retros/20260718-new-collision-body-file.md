# Retro: new fails on ID collision + --body-file (task 20260718-235158)

## What went well
- Both changes landed on existing seams: the collision guard is four lines in
  `task_create` (the only path that creates a task dir), and --body-file
  reuses `aids_io_read` plus one small `read_stdin_all` helper. No new
  translation units, no format changes.
- Ordering the body read BEFORE ID generation gave "a bad -b path creates
  nothing on disk" for free, matching the repo rule "validate before
  writing".
- The design question (bump to next second vs fail) was decided by the user
  before implementation: fail, because IDs should not drift into the future.
  Cheap to ask, expensive to redo.

## Difficulties / gotchas
- Testing a same-second collision deterministically is impossible from the
  outside (no clock injection), so the test runs new-pairs in a retry loop:
  same-second attempts must fail with "already exists", cross-boundary
  attempts must yield distinct IDs. Either way the overwrite bug cannot pass.
- Remember: build/test through `nix develop -c` (clang + valgrind); the bare
  shell only has gcc.

## Lessons / follow-ups
- Fixing a tool beats warning about it: this bug had x7 recurrences in a
  downstream repo's lessons ledger despite prompt-level mitigations in three
  skills. When a lesson keeps recurring, ask whether the TOOL can make the
  mistake impossible.
- Possible future follow-up: clock injection (env var) for fully
  deterministic collision tests, only if the retry-loop test ever flakes.
