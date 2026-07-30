# Review: Add transactional flow lifecycle commands and guards

- TASK: 20260730-154657
- BRANCH: feat/flow-lifecycle

## Round 1

- VERDICT: REQUEST_CHANGES
- REVIEWER: out-of-context

- [x] R1.1 (MAJOR) tatr.c:3696 - The EPIC review-gate exemption is broader than
  `tatr check`'s, so `tatr flow` can produce a state the lint flags, breaking
  the DECISION's tying invariant: `check_task` only exempts a container from
  `closed-missing-review` (the file's absence), not from `closed-not-approved`
  (a present REVIEW.md whose latest verdict is not APPROVE), but
  `flow_check_preconditions` skips the whole review gate when `exempt`.
  Verified: a `KIND: EPIC` task with a `REVIEW.md` containing
  `- VERDICT: REQUEST_CHANGES` walks all eight edges to DONE and `tatr check`
  then reports `closed-not-approved` (exit 1). Change the gate so only the
  *presence* requirement is exempt, and extend `test_transition_epic_exemptions`
  with the REVIEW.md-present case.
  - Response: Confirmed by re-deriving it here (an EPIC with a REQUEST_CHANGES
    REVIEW.md reached DONE, `tatr check` exit 1). Fixed: the review gate now
    reads REVIEW.md unconditionally and only the "there is no REVIEW.md"
    message is behind `!exempt`; the verdict and the open BLOCKER/MAJOR count
    are evaluated whenever the file exists.
    `test_transition_epic_exemptions` gained the REVIEW.md-present case, which
    fails without the fix.
- [x] R1.2 (MINOR) tatr.c:3793 - `main_flow` reimplements `task_load` inline
  (read + `task_deserialize` + `task->_buffer = raw.str` + the failure-path
  free) instead of reusing the spine AGENTS.md and the promoted
  `one-resolve-spine` lesson require, putting the `_buffer` ownership subtlety
  in a third place. Add a `task_load_raw(path, task, &raw)` and have `task_load`
  delegate to it.
  - Response: Fixed exactly as suggested. `task_load_raw` is the one loader;
    `task_load` is now a one-line delegation passing NULL for the raw slice,
    and `main_flow` calls `task_load_raw` for the bytes the close gate counts
    Steps in.
- [x] R1.3 (MINOR) README.md:335 - The `tatr flow` refusal transcript shows the
  same command failing with "there is no REVIEW.md" and then, on the next line,
  succeeding into COMPOUNDING, with no visible cause. Insert the intervening
  command that creates the record.
  - Response: Fixed; the transcript now shows the `printf` that writes
    `REVIEW.md` between the refusal and the successful move.
- [x] R1.4 (MINOR) checker.sh:1000 - `PLAN STATUS: NOT_REQUIRED` lost its only
  behavioural coverage when `-S` was removed from `test_v2_new_and_edit_fields`;
  the value is now unreachable through the CLI but 24 live records depend on it
  parsing. Add a fixture that hand-writes it and asserts it parses, lists,
  filters and round-trips through `edit`.
  - Response: Fixed; `test_v2_plan_status_not_required` hand-writes the value
    and asserts it parses, lists, filters with `:plan_status eq NOT_REQUIRED`,
    shows, and survives an unrelated `edit`.
- [x] R1.5 (NIT) README.md:345 - The `flow` options list documents only
  `--to <STEP>` while `tatr flow --help` prints `-o, --to`. Document both
  spellings or drop the short name.
  - Response: Documented both; README now says `-o, --to <STEP>` and SKILL.md's
    command block says `tatr flow <id> [-o|--to <STEP>]`. The short name is
    kept: every other option in the tool carries one.
- [x] R1.6 (NIT) checker.sh:548 - The nine-line doc comment written for
  `drive_task_to` sits above `flow_step_of`, so the helper that needs the
  explanation has none; and `drive_task_to` repeats `flow_step_of`'s `sed`.
  - Response: Fixed; the comment sits with `drive_task_to` and the helper now
    calls `flow_step_of`.

Verification notes (not findings):

- Round 1 was produced by an out-of-context reviewer with no sight of the
  implementing session. It ran the full suite (80/80), the memcheck suite
  (80/80, zero leaks), `tatr check --ledger LESSONS.md` (exit 0), both DoD
  doc greps, and re-ran the five named DoD tests against deleted guards.
- The in-session pass re-derived R1.1 independently before adopting it, and
  confirmed the fix by replaying the same reproduction (the close is now
  refused with "the latest REVIEW.md verdict is 'REQUEST_CHANGES', not
  APPROVE", and `tatr check` stays clean).
- Pending user check (`manual:` DoD item): the README transcripts match real
  `tatr flow` output, pasted not paraphrased. The reviewer replayed them and
  found every `tatr` line byte-accurate modulo the declared `tatr.c:<line>:`
  elision, which the README states in place.
- Pre-existing, not this branch: `task_sibling_read` conflates "absent" with
  "unreadable", so `flow_check_dependencies` says "does not exist" for a
  TASK.md that exists but cannot be read - the same conflation `check` already
  had. Several older fixtures still use the `local out=$(cmd); local rc=$?`
  shape AGENTS.md warns about.

## Round 2

- VERDICT: APPROVE
- REVIEWER: out-of-context

All six round-1 findings were re-verified against the new code and confirmed
resolved (R1.1 through R1.6), and their checkboxes are ticked on that
confirmation. One new finding:

- [ ] R2.1 (NIT) checker.sh:1113 - `local listed=$(run_tatr ls 2>&1)` followed
  by `local listed_code=$?` is the exact trap AGENTS.md's "checker.sh gotcha"
  and the promoted `checker-set-e-exit-codes` lesson describe: `local` resets
  `$?`, so `listed_code` is always 0 and the `[ $listed_code -eq 0 ]`
  assertion in the new test is vacuous. Split the declaration or drop the
  assertion.
  - Response: Fixed by splitting the declaration, with a comment naming the
    gotcha so the next fixture author sees why. Suite re-run: 81/81, exit 0.
    Left unticked because the round-2 reviewer has not re-verified it; it is a
    NIT and does not affect the APPROVE.

Verification notes (not findings):

- The reviewer re-ran the full suite (81/81, exit 0) and
  `tatr check --ledger LESSONS.md` (exit 0), and spot-valgrinded the
  refactored load path (`flow` refusal, `flow` into COMPOUNDING and DONE,
  `show`, `edit`) with no errors and no leaks.
- It mutation-tested both new guards on patched binaries: reverting the review
  gate to `if (review_gate && !exempt)` fails `test_transition_epic_exemptions`
  on two assertions, and breaking `NOT_REQUIRED` in `Plan_Status_Strings` fails
  `test_v2_plan_status_not_required` on three.
- It re-ran the R1.1 reproduction: a reviewed EPIC is now refused at
  `REVIEWING -> COMPOUNDING` with both reasons, while a REVIEW.md-less EPIC
  still walks to DONE with its step box unticked and `tatr check` stays clean -
  the exemption narrowed to presence only, matching `check`'s
  `closed-missing-review` / `closed-not-approved` split.
- It confirmed the two commits since round 1 touch only README.md, SKILL.md,
  checker.sh, tatr.c and REVIEW.md, so no ticked step or doc claim was quietly
  restated to fit the fixes.
- Full memcheck suite (run in-session, after the round-2 review):
  `nix develop -c ./checker.sh --memcheck` 81/81, exit 0, zero leaks.

Pending user check (`manual:` DoD item): the README transcripts match real
`tatr flow` output, pasted not paraphrased. Both reviewer passes replayed them
and found every `tatr` line byte-accurate modulo the `tatr.c:<line>:` elision
the README declares in place.
