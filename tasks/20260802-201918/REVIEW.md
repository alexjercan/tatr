# Review: Replace the FLOW STEP chain with ACTIVITY, GATES and RESOLUTION

- TASK: 20260802-201918
- BRANCH: feat/activity-gates-resolution

## Round 1

- REVIEWER: out-of-context
- VERDICT: REQUEST_CHANGES

- [x] R1.1 (MAJOR) tatr.c:6318 - `main_close` appends a fresh
  `## Dropped` / `- REASON: ` block to the description every time, so
  `close --resolution WONTDO` -> `reopen` -> `close --resolution WONTDO`
  leaves two `## Dropped` sections in one record, and `artifact_field`
  returns the FIRST match, so the record reports the stale reason while
  the new one sits below it. `tatr check` is silent on the duplicate.
  This is reachable only because `reopen` is new - v0 `DROPPED` was
  terminal - and `reopen` already clears `duplicate_of` (tatr.c:6394)
  while leaving the WONTDO reason behind, which is the same asymmetry.
  Strip an existing trailing `## Dropped` block from `task.description`
  before appending in `main_close`, or clear it in `main_reopen` the way
  `- DUPLICATE OF: ` is cleared, and add a `checker.sh` case asserting
  `grep -c '^## Dropped'` is 1 after a re-close.
  - Response: fixed, on the `reopen` side. `close` writes three things - the
    resolution, the `- DUPLICATE OF: ` pointer and the `## Dropped` block -
    and `reopen` now clears all three, which is the asymmetry the finding
    names rather than only its symptom. Stripping in `close` would not have
    covered the WONTDO -> reopen -> DUPLICATE path, where `close` never
    reaches its append at all and the stale reason would survive.
    `task_description_without_dropped` (tatr.c:6167) cuts a `## Dropped`
    heading only when no later `## ` heading follows it, so an author's own
    mid-record section is left alone. New `test_reopen_clears_dropped_reason`
    asserts `grep -c '^## Dropped'` is 1 after the first close, 0 after
    reopen, 1 again after the re-close with only the second reason present,
    and that body prose above the block survives; it fails when the strip is
    stubbed out. README, CHANGELOG and both `skills/tatr` surfaces say so.

- [x] R1.2 (MINOR) tatr.c:6741 - `unplanned-in-progress` is now fully
  subsumed by the `inconsistent-gates` rule added 5 lines below it.
  Its predicate (non-container, `ACTIVITY >= WORKING`, no `PLAN` gate,
  no `RESOLUTION`) is a strict subset of the new rule's (non-container,
  cursor past `PLANNING`, no `PLAN` gate), so it can never fire alone:
  a hand-written record at `ACTIVITY: WORKING` with `GATES: -` emits two
  findings naming the same fact, and a repository that wants to accept
  it must write two exemption lines. Delete `unplanned-in-progress`
  (tatr.c:6734-6745) and let `inconsistent-gates` own the fact, updating
  `README.md:621` and `skills/tatr/check-rules.md:36`.
  - Response: fixed. The rule is deleted; `inconsistent-gates` carries a
    comment saying it owns the fact. `README.md` and
    `skills/tatr/check-rules.md` lost the entry, the EPIC exemption list
    lost the name, and `CHANGELOG.md` records the removal with the reason.
    The old `test_check_unplanned_in_progress` became
    `test_check_working_without_plan_gate`, which now asserts the record
    draws exactly one finding (`grep -c` on its id) and that it is the
    `inconsistent-gates` line - the double-finding the report describes is
    what the test would catch if the rule came back.
    One behavior change worth naming: `unplanned-in-progress` carried a
    `!has_resolution` guard that `inconsistent-gates` does not, so a CLOSED
    record whose cursor is past `PLANNING` without the `PLAN` gate is now
    flagged. That was already true before this change - the two rules
    disagreed there and `inconsistent-gates` won - so the deletion only
    makes the docs honest. `README.md`'s "closed tasks are never asked for
    one" sentence was wrong and is corrected.

- [x] R1.3 (NIT) tatr.c:8038 - `migrate_parse_v0` copies
  `aids_string_slice_atol(&priority_slice, (long *)&task->meta.priority, 10)`
  from `task_deserialize` (tatr.c:612). `aids_string_slice_atol` writes a
  `long` through that pointer, so on LP64 it stores 8 bytes into a 4-byte
  `unsigned int` - benign only because `Task_Meta` happens to pad after
  `priority`. The new call site is a second copy of a latent defect;
  parse into a `long` local and assign it to `priority`.
  - Response: fixed at both call sites. `migrate_parse_v0` (tatr.c:8047) and
    the original in `task_deserialize` (tatr.c:612) both parse into a `long`
    local and narrow to `priority`. The second is pre-existing, but it is the
    same two-line defect this diff copied, and leaving the source of the copy
    in place would have invited the next one; a comment at the original says
    why the local is there.

- [x] R1.4 (NIT) tatr.c:6315 - `close --resolution SUPERSEDED --of Y`
  writes `- DUPLICATE OF: Y`, which reads as "this task duplicates Y"
  when the fact is "Y superseded this task"; the record format already
  has `- SUPERSEDED BY: ` for that, and `dropped-bad-superseder` still
  lints it. The Steps specify the shared line, so this is a naming call
  rather than a deviation - but if it stays, say so in `README.md:519`'s
  field table, since the spelling is frozen by v1.0.0.
  - Response: it stays, and the README now says so. One pointer field serves
    both resolutions and `RESOLUTION` is what says which relation it records;
    a second field would carry no information the pair does not already.
    `- SUPERSEDED BY: ` is a body line an author writes, not a metadata field
    `close` owns, which is why `dropped-bad-superseder` still lints it
    separately. The "Resolution values" entry in the record-format section
    now states that the reuse is deliberate and that the spelling is frozen
    by v1.0.0.

- [x] R1.5 (NIT) checker.sh:3479 - the test was rewritten around the PLAN
  gate but its `log_test` label still reads "Steps and DoD are owed from
  PLANNED on, not at BACKLOG", so the suite prints two retired chain node
  names as if they were live. The fixture below it correctly writes
  `PLANNING` + `PLAN`. Retitle to "owed once the PLAN gate is earned, not
  before" and drop `PLANNED`/`BACKLOG` from the comment on line 3496 and
  the `planned_out` variable name. (checker.sh:623's `PLANNED` pseudo-target
  in `drive_task_to` is deliberate and documented - leave it.)
  - Response: fixed. The label reads "check (Steps and DoD are owed once the
    PLAN gate is earned, not before)", the comment reads "The same body once
    the PLAN gate is earned is a finding", and `planned_out` is now
    `gated_out`. `drive_task_to`'s `PLANNED` pseudo-target is untouched.

Verified independently: the full suite and the memcheck suite, both
111/111 with zero leaks; `clang`, `gcc` and `x86_64-w64-mingw32-gcc` all
warning-clean under `-Wall -Wextra`; the three version sites read 1.0.0;
`tatr check` clean over all 39 migrated records with none carrying
`- FLOW STEP: `. Re-derived by hand rather than read off the tests: the
half-success write moves `GATES` and nothing else and holds the cursor at
`PLANNING`; `rewind` refuses forward, equal and gate-discarding targets
byte-identically; every `close` refusal and the `reopen` round-trip;
`migrate` dry-run, apply and idempotence including the gates read from
`REVIEW.md` and `RETRO.md`; all three exemption forms including a preamble
prose line containing a colon; and the retired and new filter spellings.

- The DoD's doc-sweep proof was rewritten during implementation (broad
  repo sweep -> README plus `AGENTS.md` and `skills/`). The narrowing is
  justified - `checker.sh` must test the v0 refusal and `CHANGELOG.md`
  must name the removal - and an unfiltered sweep of every `*.md` and
  `*.nix` outside `tasks/` confirms no surviving mention of a retired
  token as a live concept. Accepted as a correction, not a weakening.

- Pre-existing, not from this diff, so not a finding: any `tatr`
  subcommand given an unknown option crashes (`free(): invalid pointer`
  or SIGSEGV) after argparse prints `Error: unknown argument`. A `master`
  build aborts identically on `tatr ls -q ':status eq OPEN'`. Worth its
  own task.

- The one open `manual:` proof - the user replaying `NOTES.md` end to end -
  is a pending user check. `DECISION.md` records where the implementation
  deliberately departs from that sketch.

## Round 2

- REVIEWER: out-of-context
- VERDICT: APPROVE

All five round-1 findings are verified fixed and ticked. The one finding
below is new, and is not a regression from those fixes.

- [ ] R2.1 (NIT) tasks/20260802-201918/TASK.md:283 - the Close-out
  `**Evidence.**` paragraph carries two numbers the current tree does not
  produce. Line 283 reads `111/111`, but the round-1 fix added
  `test_reopen_clears_dropped_reason` and both suites now report 112/112 -
  the "Review round 1" paragraph at line 318 says so, so the same document
  disagrees with itself. Lines 287-288 read "21 whole-task entries plus 3
  narrow ones"; `tasks/EXEMPTIONS.md` actually carries 20 whole-task and 4
  rule-scoped lines (the fourth narrow one is
  `20260730-153325 bad-record-schema`). The source-side count in the same
  sentence - 37 rule-scoped lines over 21 tasks - is correct, and matches
  `master`. Set line 283 to `112/112` and lines 287-288 to "20 whole-task
  entries plus 4 narrow ones". Nothing behind the prose is wrong: the
  collapse happened, `tatr check` is clean, and no proof was claimed that
  was not run.
  - Response:

Verified independently, not read off the implementer's summary:

- `nix develop -c ./checker.sh` 112/112 and
  `nix develop -c ./checker.sh --memcheck` 112/112, zero leaks, both run
  from this worktree. `gcc` and `x86_64-w64-mingw32-gcc` warning-clean under
  `-Wall -Wextra`; the `make` build is silent under `clang`.
- Every `cmd:` proof in the DoD re-run green: no stored `- STATUS: `, no
  surviving `- FLOW STEP: ` with `tatr check` clean over all 39 records, the
  three version sites at 1.0.0, and both doc-sweep greps empty.
- R1.1 re-derived by hand rather than trusting `test_reopen_clears_dropped_reason`.
  A record with its own `## Dropped` section followed by a later `## Tail`
  heading survives a close and reopen untouched, so only the trailing block
  `close` wrote is cut. The `WONTDO` -> `reopen` -> `DUPLICATE` path the
  Response claims a `close`-side strip would have missed does leave no stale
  reason, and `check` is clean on the result. `reopen` on a task carrying no
  `RESOLUTION` refuses and leaves `TASK.md` byte-identical, so an authored
  trailing `## Dropped` block cannot be eaten by a stray `reopen`.
  `main_reopen`'s new allocation mirrors `main_close`'s: freed in `defer`
  before `task_cleanup`, which frees only `_buffer`, so there is no double
  free - and the memcheck run over the new test agrees.
- R1.2's replacement test asserts the record draws exactly one finding and
  that it is the `inconsistent-gates` line, which is what would catch the
  deleted rule coming back. `unplanned-in-progress` survives only in
  `CHANGELOG.md` (recording its removal), one `tatr.c` comment naming it as
  the v0 rule, and the append-only `tasks/` history.
- R1.3: no `(long *)&` cast remains anywhere in `tatr.c`.
- The rewind clear table re-derived by hand: `rewind --to WORKING` from
  `COMPOUNDING` keeps `PLAN` and clears `REVIEW`; without `--force` it
  refuses and leaves `TASK.md` byte-identical; forward and equal targets are
  both refused by name with a pointer to `tatr flow`.
- Spot-checked the README close/reopen transcript against a real run: the
  `gate RETRO recorded`, `moved COMPOUNDING -> CLOSED (RESOLUTION: DONE)`,
  CLOSED-refusal and `reopened at COMPOUNDING` lines match byte for byte.

- `checker.sh:3433`'s comment still calls a bare record "BACKLOG", a name
  this diff retires. It is not a finding: the line is identical on `master`,
  the fixture under it is correct, and the DoD's doc-sweep proof
  deliberately excludes `checker.sh` because that file must test the v0
  refusal. Worth a passing edit if the file is touched again.

- Pending user checks, which do not block this APPROVE:
  - `manual:` the README and `skills/tatr/lifecycle.md` transcripts match
    real output, pasted not paraphrased. The spot-check above supports it
    but does not discharge it.
  - `manual:` the user replays the `NOTES.md` transcript end to end.
    `DECISION.md` records where the implementation deliberately departs from
    that sketch, so it is a shape check rather than a byte comparison.
