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
- `validate-the-exact-parsed-token` (x2): a trimmed re-validation of an
  untrimmed parse is a hole; check the exact bytes the parser consumes
  (task_status_from_string silently defaults to OPEN). 20260720-152503, 20260725-111031
- `re-enumerate-snapshot-counts` (x1): a task quoting "the N items to fix" is a
  snapshot of a moving backlog; re-derive the live set (git log, tatr check -S)
  before acting rather than trusting the number. 20260720-220114
- `guard-at-the-layer-that-holds-it` (x1): before adding a validation, name the
  invariant and check this layer can enforce it - a detector the producer can
  still route around adds false positives without closing the hole (a read-side
  body heuristic became a write-side re-parse). 20260730-153325
- `test-the-position-not-the-shape` (x1): when a rule depends on WHERE input
  appears, the fixture must put it exactly there; the right bytes somewhere else
  passes while the rule is broken. 20260730-153325
- `read-the-history-of-a-doc-claim` (x1): when docs describe behavior, grep the
  code before preserving OR deleting the claim, then `git log -S` the gap - the
  commit that dropped it says which side was intended. 20260730-153325
- `check-the-helper-signature` (x1): glance at one existing call site for a
  by-value-vs-by-pointer aids helper convention before writing a fresh call
  (aids_string_slice_starts_with takes the prefix by value). 20260722-152010
- `target-compiler-first` (x1): for cross-platform work, run the target compiler
  before designing portability shims so the first fix set follows real
  diagnostics. 20260728-095149
- `guard-from-the-rule-not-its-summary` (x2): when two components must agree
  (lint and lifecycle, parser and serializer), enumerate BOTH ACTUAL rule sets
  in code and diff the lists - in whichever direction the change arrives.
  Adding a rule to one side is the same failure as writing a guard from the
  other side's prose summary. 20260730-154657, 20260730-154745
- `fix-the-property-not-the-instance` (x1): a finding that names a broken
  invariant is not resolved by making the reported path hold; move the rule to
  one home so the invariant is structural, then write it down. Fixing the two
  instances left two more holes and a second MAJOR round. 20260730-154745
- `paste-transcripts-from-a-real-run` (x1): a command transcript in a doc is a
  claim; paste it from a run in the same session rather than typing what you
  expect. Three of one round's seven findings were invented output. 20260730-154745
- `a-passing-suite-is-not-a-covered-property` (x1): before trusting N/N on a
  cross-cutting property (warning-cleanliness, leaks, portability), read the
  test that supposedly covers it - the Windows test asserted a PE artifact and
  let four new compiler warnings through. 20260730-154745
- `no-outcome-before-the-run` (x1): never write a verdict, count or "confirmed"
  into a record before the thing producing it has run, however certain the
  outcome looks. 20260730-154657
- `mutation-test-the-new-guard` (x2): delete each new guard one at a time and
  watch its own test go red - cheap, and it turns "this test would fail without
  the fix" from an assertion into a check. The mutation must remove the SIDE
  EFFECT, not just the return value: `0 * report(...)` still prints.
  20260730-154657, 20260730-154745
- `serialize-build-artifact-checks` (x1): do not parallelize verification
  commands that clean or rewrite the same build outputs in one worktree.
  20260728-095149

## Domain lessons (project-specific)

- `one-resolve-spine` (x3, PROMOTED 2026-07-05 -> AGENTS.md Code conventions):
  new commands reuse task_resolve / task_load / task_save instead of
  reimplementing HUID or path logic. 20260705-172803, 20260705-172804, 20260705-172805
- `checker-set-e-exit-codes` (x4, PROMOTED 2026-07-05 -> AGENTS.md checker.sh
  gotcha): under set -e, `local out=$(cmd)` swallows the exit code; should-fail
  tests use the set +e / split-declaration pattern. Applies to YOUR OWN
  verification commands too - `./checker.sh --memcheck | tail` reported exit 0
  on a run that died mid-suite. 20260705-172803, 20260730-153325, 20260730-154657
- `huid-gates-destruction` (x1, PROMOTED 2026-07-05 -> AGENTS.md Code
  conventions): deletion only ever touches tasks/<id>/ behind a validated
  HUID; never build a destructive path from raw input. 20260705-172805
- `aids-listdir-raw-dirents` (x1): aids_io_listdir returns raw dirents
  including `.` and `..`; skip them before unlinking. 20260705-172805
- `named-literal-char-predicate` (x1): extend the filter lexer via the named
  is_literal_char predicate, not inline ctype checks - one documented edit
  point for why `.` and `-` lex but `/` does not. 20260709-193044
- `build-through-nix-dev-shell` (x3, absorbed by Makefile build guard, 2026-07-20):
  a bare `make` outside nix (no IN_NIX_SHELL/NIX_BUILD_TOP) now fails with a
  `nix develop -c make` pointer; TATR_ALLOW_BARE_BUILD=1 opts out (CI provisions
  its own toolchain). 20260705-172803, 20260709-193044, 20260718-235158, 20260720-220059

## Pending promotions (3+ occurrences, user decides)

- `test-first-for-check-messages` (x3): for a check rule, write the test with
  its exact expected message before the emitting code, so the format is designed
  from the assertion not reverse-engineered into it. 20260722-152010,
  20260725-111031, 20260730-153325
