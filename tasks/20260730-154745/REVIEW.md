# Review: Scaffold and validate flow artifact schemas

- TASK: 20260730-154745
- BRANCH: feat/artifact-schemas

## Round 1

- REVIEWER: out-of-context
- VERDICT: REQUEST_CHANGES

Verified by the reviewer: `nix develop -c ./checker.sh` (92/92) and
`--memcheck` (92/92, no leaks); every `test:` proof in the DoD exists, is
registered in the run list and asserts exact finding messages; the `cmd:`
proof passes; `tatr check` on the live repo exits 0 silently; no `manual:`
proofs are pending; the diff is clean ASCII. Memory ownership in the new code
was traced by hand and valgrind agrees.

- [x] R1.1 (MAJOR) tatr.c:4106 - `tatr flow` can produce a state `tatr check`
  flags, which AGENTS.md states is impossible ("A transition may never produce
  a state the lint would flag - both read the same artifacts through the same
  helpers"). A task whose REVIEW.md is a bare `- VERDICT: APPROVE` and whose
  RETRO.md is `# Retro` walks WORKING -> REVIEWING -> COMPOUNDING -> DONE
  unchallenged, and the resulting CLOSED task yields 12 findings. The same gap
  exists at the plan gate: `flow --to PLANNED` never asks for `## Steps` /
  `## Definition of Done`, but `check` demands them from PLANNED on. Route the
  new validators into the gates through a shared helper that RETURNS unmet
  reasons, so `check` prints them and `flow` collects them.
  - Response: Confirmed by re-deriving it here: a task with a bare
    `- VERDICT: APPROVE` REVIEW.md and a one-line RETRO.md walked to DONE and
    `tatr check` then reported 12 findings (exit 1). This is the same class of
    miss as 20260730-154657's R1.1 - a guard written from a summary of the
    lint rather than from the lint. Fixed at the root: the rules now live in
    collector functions (`record_schema_problems`, `review_round_problems`,
    `spike_record_problems`, `task_record_problems`) that RETURN problems as
    data. `check` prints them as findings, `flow` appends them as unmet
    preconditions, and neither owns a copy. A new record gate runs on
    `PLANNING -> PLANNED` (the plan sections and their proofs, judged at the
    step being moved TO, since that transition is what makes the record owe
    them), `PLANNED -> WORKING` and `COMPOUNDING -> DONE`; the review gate now
    also holds REVIEW.md to its schema, and the close gate RETRO.md and
    DECISION.md. The duplicate open-BLOCKER/MAJOR message in the review gate
    was dropped in favour of the shared `approve-with-open-findings`.
    `test_transition_cannot_mint_a_flagged_record` walks the whole lifecycle
    and asserts each refusal names the lint's own rule slug, ending with a
    clean `tatr check` on the state the lifecycle produced.
- [x] R1.2 (MAJOR) tatr.c:4451 - `__attribute__((format(printf, 4, 5)))` on
  `check_finding` regresses the MinGW build from warning-clean to four
  warnings (`unknown conversion type character 'z'`, `too many arguments for
  format`). AGENTS.md requires the code stay warning-clean under
  `-Wall -Wextra` with MinGW, and the release pipeline builds `dist/tatr.exe`.
  Use the gnu archetype via `__MINGW_PRINTF_FORMAT` where it is defined.
  - Response: Confirmed - `x86_64-w64-mingw32-gcc -Wall -Wextra -O2` reported
    four warnings on this branch and none on master. Fixed with a
    `TATR_PRINTF_FORMAT` macro that selects `__MINGW_PRINTF_FORMAT` where it is
    defined and `printf` otherwise, applied to both `check_finding` and the new
    `record_problems_add`. The real gap was that nothing checked:
    `test_windows_build_target` only asserted the artifact was a PE file, so
    warnings scrolled past. It now fails on any `warning:` in the MinGW build
    output.
- [x] R1.3 (MINOR) README.md:514 - the documented output of
  `tatr proofs 20260730-154745` is invented: it shows 3 lines while the real
  command prints 7. README.md:499's `scaffold --list` example likewise shows
  `RETRO ... missing` for a task that has one. The task's own note requires
  commands be documented from real output.
  - Response: Fixed. Both examples are now pasted from real runs
    (`tatr proofs 20260730-154745` prints 7 lines; the `--list` example uses
    20260730-153325, whose SPIKE.md really is the only missing one).
- [x] R1.4 (MINOR) tatr.c:5593 - `proof_print_text` collapses every whitespace
  run, not just a wrapped bullet's newline+indent, so a `cmd:` proof does not
  "round-trip verbatim" as README.md, AGENTS.md, SKILL.md and CHANGELOG all
  claim: `` `grep -q "a  b" f` `` prints as `grep -q "a b" f`. Collapse only
  runs containing a newline, or drop the word from all four docs.
  - Response: Fixed in the code rather than the docs, since the docs stated the
    property that was wanted. `proof_print_text` now collapses only a
    whitespace run that CONTAINS a newline - the bullet's line wrap - and
    passes intra-line spacing through byte for byte. Verified:
    `` `grep -q "a  b" f` `` round-trips with both spaces, while a wrapped
    proof still prints on one line. Pinned in
    `test_proof_listing_does_not_execute`; the docs now say precisely what
    collapses.
- [x] R1.5 (MINOR) CHANGELOG.md:40 - "nothing written under the flow suite
  needed an entry" is contradicted by EXEMPTIONS.md's own "## Recent records"
  entry for 20260730-153325, a flow-suite task and a dependency of this one.
  - Response: Fixed. The CHANGELOG now states the composition honestly: 37
    lines over 21 tasks - 14 pre-flow, 6 early-flow, and one flow-suite retro
    (20260730-153325) that predates the fixed retro section vocabulary.
- [x] R1.6 (MINOR) tatr.c:4343 - the "Twenty-eight sibling records" figure,
  repeated in DECISION.md and CHANGELOG.md, matches nothing shipped:
  EXEMPTIONS.md carries 37 lines over 21 distinct task IDs. Re-derive it from
  the file or drop the count.
  - Response: Fixed. Re-derived from the shipped file: 37 exemption lines over
    21 distinct task IDs, out of 31 tasks. The invented count is gone from the
    `tatr.c` comment entirely (it does not need one) and corrected in
    DECISION.md, CHANGELOG.md and EXEMPTIONS.md's own preamble.
- [x] R1.7 (MINOR) tatr.c:5014 - the SPIKE rules are kind-gated, not
  presence-gated: a SPIKE.md on a `KIND: TASK` record - which
  `tatr scaffold <id> SPIKE` will happily write - is never validated, while
  README.md, AGENTS.md and SKILL.md all describe them as presence-gated.
  Validate any SPIKE.md that exists, keeping only `missing-spike-record`
  kind-gated.
  - Response: Fixed as suggested. `spike_record_problems` now runs on PRESENCE
    of a SPIKE.md whatever the task's kind; only `missing-spike-record` stays
    kind-gated. `test_check_spike_records` gained a `KIND: TASK` record with a
    SPIKE.md (flagged for its status and its missing sections, but never for
    `missing-spike-record`) and a task with neither (never flagged at all).

## Round 2

- REVIEWER: out-of-context
- VERDICT: REQUEST_CHANGES

Round 1's responses were re-derived and all seven hold: the MinGW build is
silent and its new assertion is real, both README examples match live output,
`proof_print_text` collapses only wrapped runs, the exemption counts are
accurate (37 lines, 21 of 31 tasks), and the SPIKE content rules fire on
presence. The refactor's memory ownership was traced and valgrind agrees; no
rule was dropped in the move to the collectors; the fixed-capacity buffers
truncate rather than overflow; and no existing test was weakened - the
`drive_task_to` change routes through `tatr scaffold`, which strengthens it.

One asymmetry noted and deliberately kept: `flow_unmet_add_problems` does not
consult `EXEMPTIONS.md`, so a repository that exempts a rule can still be
refused a transition for it. That only affects historical records, which do not
walk edges.

- [x] R2.1 (MAJOR) tatr.c:4531 - the review gate does not apply `bad-severity`,
  so R1.1's invariant is still breakable. `review_round_problems` never calls
  `artifact_severity_is_known`; that rule lives only in `check_task`'s own loop.
  A REVIEW.md with `- [x] R1.1 (LOW) ...` and VERDICT APPROVE walks
  REVIEWING -> COMPOUNDING -> DONE, and `tatr check` then exits 1 with
  `bad-severity`. Move the severity scan into `review_round_problems` and
  delete the copy in `check_task`.
  - Response: Confirmed by re-running the repro. Fixed exactly as suggested:
    the severity check now sits next to the `bad-finding-id` block inside
    `review_round_problems`, and `check_task`'s loop is gone, so REVIEW.md has
    one rule set with one home.
- [x] R2.2 (MAJOR) tatr.c:4559 - the close gate applies only
  `record_schema_problems` and `artifact_decision_status` to a DECISION.md, not
  the supersede rules `check_decision` applies, so the same invariant breaks
  through DECISION.md. A `SUPERSEDED by tasks/19990101-000000/DECISION.md`
  status satisfies the gate, reaches DONE, and `tatr check` then reports
  `dangling-supersede`. Factor `check_decision`'s body into a
  `decision_record_problems` collector and call it from the close gate.
  - Response: Confirmed by re-running the repro. Fixed exactly as suggested:
    `decision_record_problems` now carries `bad-record-schema`,
    `dangling-decision-task`, `bad-decision-status`, `dangling-supersede` and
    `nonreciprocal-supersede`; `check_decision` is a three-line wrapper over it
    and the close gate's hand-written status switch is gone, so the refusal
    also carries the rule slug every other gate message does. The three
    reference helpers moved up beside the other collectors to make that
    possible.
  - Note: R2.1 and R2.2 are one root cause, not two - the round-1 fix moved
    SOME rules into collectors and left others behind, which is the same
    incomplete-application miss R1.1 itself was. AGENTS.md now states the
    structural rule ("every record rule lives in a collector and nowhere else")
    rather than listing the collectors that happen to exist.
- [x] R2.3 (MINOR) tatr.c:5763 - the R1.4 fix newly lets a literal tab inside a
  proof escape into the output, breaking `tatr proofs`'s own
  `<n><TAB><kind><TAB><text>` record format: a consumer splitting on tabs sees
  four fields where it expects three. Collapse a run containing a tab like one
  containing a newline, and say which in the README.
  - Response: Fixed as suggested. A whitespace run now collapses when it holds
    any byte that would break the record format - newline, carriage return,
    tab, vertical tab or form feed - and is passed through byte for byte
    otherwise. `test_proof_listing_does_not_execute` gained a proof containing
    a real tab and now asserts every output line has exactly three
    tab-separated fields. README, AGENTS.md and SKILL.md say precisely this.
- [x] R2.4 (NIT) tatr.c:3548 - the comment still names `check_review_rounds`, a
  function the refactor renamed to `review_round_problems`.
  - Response: Fixed.

## Round 3

- REVIEWER: out-of-context
- VERDICT: APPROVE

The tying invariant was enumerated rule by rule and then fuzzed differentially:
7 source steps x 16 adversarial record mutations x every legal outgoing edge.
For every transition that succeeded, `tatr check`'s finding set afterwards was a
subset of the set before - zero violations. The reverse direction was checked
too: an EPIC reaches DONE with no REVIEW.md, no RETRO.md and an unchecked child
and the lint is clean, so no gate refuses a state the lint would accept.

The round-2 move was diffed line by line: the three DECISION.md reference
helpers moved byte-identically, and `decision_record_problems` is
`check_decision`'s old body with `check_finding` mechanically replaced by
`record_problems_add` - same five rules, same order, same messages, same
control flow including the "empty header is treated as absent" carve-out.
Memory ownership is sound on every path and valgrind agrees.

No test was weakened across the three rounds: every removed `checker.sh` line is
fixture content replaced by a schema-valid, scaffold-generated equivalent, not
one `grep -q` assertion was deleted, and no test function was removed or
unregistered. All 93 defined `test_*` functions are registered in the run list.

- [x] R3.1 (NIT) tatr.c:5739 - `proof_print_text` also collapses a whitespace
  run containing `\v` or `\f`, but README.md, AGENTS.md and SKILL.md all state
  the collapse happens only for a newline or a tab and promise everything else
  survives byte for byte. Neither byte breaks line- or tab-splitting, so either
  drop them from the condition or name them in the three docs.
  - Response: Fixed in the code, so the documented contract is the true one:
    the condition is now exactly `\n`, `\r` and `\t`. Verified - a `cmd:` proof
    containing a vertical tab round-trips with the 0x0B byte intact while a
    real tab still collapses, and both lines remain three tab-separated fields.
