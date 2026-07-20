# Lessons ledger

One or two lines per lesson: slug, one sentence, an occurrence count, and a
task id or two. /compound and /lessons append new lessons or bump counts; two
lines is the cap. At three occurrences a lesson moves to Pending promotions.
Counts stay bare - (xN) - until a lifecycle event annotates them; the
annotation is the lifecycle marker and exempts the entry from the
promotion-stalled lint (tatr check --ledger): (xN, PROMOTED <date> -> <target>),
(xN, absorbed by <tool or template>, <date>), or (xN, RETIRED <date>: <reason>).
Seeded 2026-07-20 by distilling the pre-flow docs/retros/ (since removed;
git history keeps them); new per-task retros live in tasks/<id>/RETRO.md.

## Process lessons

- `reproduce-before-fixing` (x1): create the failing case and run it before
  touching code, so the fix is aimed, not guessed. 20260709-193044
- `docs-sweep-stale-claims` (x2): a docs pass re-reads aging claims and sweeps
  EVERY doc surface (README, AGENTS.md, docs/, skills) for the old
  invocation/target, not just the one the task names. 20260705-172806, 20260720-220059
- `fix-the-tool-not-the-prompt` (x1): a lesson recurring despite prompt-level
  mitigations (x7 in a downstream ledger) means the tool should make the
  mistake impossible - tatr new now fails on ID collision. 20260718-235158
- `ask-the-cheap-design-question` (x1): fail-vs-bump on ID collision was
  decided by the user before implementation; cheap to ask, expensive to redo. 20260718-235158
- `read-the-callee-not-the-name` (x1): find_current_tasks_dir returns project
  dirs, not tasks dirs - the misleading name cost a silent no-op walk. 20260720-152503
- `validate-the-exact-parsed-token` (x1): a trimmed re-validation of an
  untrimmed parse is a hole; check the exact bytes the parser consumes
  (task_status_from_string silently defaults to OPEN). 20260720-152503

## Domain lessons (project-specific)

- `one-resolve-spine` (x3, PROMOTED 2026-07-05 -> AGENTS.md Code conventions):
  new commands reuse task_resolve / task_load / task_save instead of
  reimplementing HUID or path logic. 20260705-172803, 20260705-172804, 20260705-172805
- `checker-set-e-exit-codes` (x1, PROMOTED 2026-07-05 -> AGENTS.md checker.sh
  gotcha): under set -e, `local out=$(cmd)` swallows the exit code; should-fail
  tests use the set +e / split-declaration pattern. 20260705-172803
- `huid-gates-destruction` (x1, PROMOTED 2026-07-05 -> AGENTS.md Code
  conventions): deletion only ever touches tasks/<id>/ behind a validated
  HUID; never build a destructive path from raw input. 20260705-172805
- `aids-listdir-raw-dirents` (x1): aids_io_listdir returns raw dirents
  including `.` and `..`; skip them before unlinking. 20260705-172805
- `named-literal-char-predicate` (x1): extend the filter lexer via the named
  is_literal_char predicate, not inline ctype checks - one documented edit
  point for why `.` and `-` lex but `/` does not. 20260709-193044

## Pending promotions (3+ occurrences, user decides)

- `build-through-nix-dev-shell` (x3, absorbed by Makefile build guard, 2026-07-20):
  prose in AGENTS.md was re-learned twice, so a Makefile guard now fails a bare
  `make` (no IN_NIX_SHELL/NIX_BUILD_TOP) with a `nix develop -c make` pointer;
  TATR_ALLOW_BARE_BUILD=1 opts out (CI provisions its own toolchain). The bare
  shell has only gcc, so every build and test runs via nix develop -c (clang +
  valgrind). 20260705-172803, 20260709-193044, 20260718-235158, 20260720-220059
