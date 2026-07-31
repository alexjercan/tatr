# Review: Require user disposition for lesson promotions

- TASK: 20260730-154756
- BRANCH: feature/ledger-dispositions

## Round 1

- REVIEWER: out-of-context agent (opus)
- VERDICT: REQUEST_CHANGES

Reviewer independently reproduced 107/107 native and memcheck, valgrinded 20
hand-picked `tatr ledger` invocations (zero leaks), and confirmed splice
byte-preservation with `cmp`/`xxd` on CRLF files, files with no trailing
newline, and the repo's real LESSONS.md.

- [x] R1.1 (MAJOR) tatr.c:7027 - the pre-write re-parse checks disposition,
  count and problem, but never that the PAYLOAD round-trips, so an unbalanced
  CLOSING paren in `--reason`/`--target` closes the count group early: the
  decision is silently truncated and the lesson text is corrupted, while the
  command reports success and `check --ledger` reports the file clean.
  Repro: `--target "scaffold ) and check"` writes
  `` - `a` (x3, ABSORBED 2026-07-31 by scaffold ) and check): the lesson text ``
  which reads back as target `scaffold`. The `ledger_payload_is_writable`
  comment claiming the re-parse catches this is therefore wrong: it catches an
  unbalanced `(` only.
  Suggested: require `verify.payload` to equal the intended payload (and
  `verify.defer_count == match.count` for DEFER).

- [x] R1.2 (MAJOR) checker.sh - the "mutation-test each new guard" step is
  asserted vacuously: six guards survive deletion with the suite fully green.
  Reviewer deleted each side effect one at a time and reran the suite:
  duplicate-slug refusal (tatr.c:6953), malformed-entry refusal (6958), the
  disposition/count half of the verify (7029), the `--target` extra-arg refusal
  (6879), the empty-payload guard (6731) and `--disposition` without `--slug`
  (6816) each kept 107/107. The duplicate-slug hole is the worst: `match` is
  overwritten by the last duplicate and the write lands on the wrong entry.
  Suggested: add assertions for all six, or drop the step's tick.

- [x] R1.3 (MINOR) tatr.c:6281-6296 - a mistyped count group makes a Pending
  entry invisible to every rule, contradicting DECISION.md's "every entry either
  carries a disposition or is a finding". `- \`b\` (x3: no close`, `- \`c\` (x3`
  and `- \`d\` (x3 , RETIRE ...)` all exit 0 with no finding.
  Suggested: under Pending, a `- ` line containing `(x<digits>` with no valid
  group is a bad-disposition finding, not a skipped line.

- [x] R1.4 (MINOR) tatr.c:6417 - a hand-written DEFER at an arbitrarily high
  count is a permanent silence, the exact failure the feature exists to close.
  `(x3, DEFER 2026-01-01 at x999999: r)` passes forever, while AGENTS.md, README
  and CHANGELOG all claim a DEFER cannot become one.
  Suggested: `defer_count > count` is bad-disposition - a DEFER records the
  count it was taken at.

- [x] R1.5 (MINOR) tatr.c:6288 and 6488 - count accumulation overflows
  silently: `(x99999999999999999999999)` prints `x200376420520689663` in a
  finding and feeds the DEFER comparison.
  Suggested: stop at a sane digit bound and treat an over-long run as
  not-a-counter.

- [x] R1.6 (NIT) README.md:539 - the `tatr ledger` console example shows this
  repo's own two slugs as `awaiting-decision`, but the same commit records them
  as PROMOTE. Use placeholder slugs.

- [x] R1.7 (NIT) tatr.c:6639 - the `-I <id>` branch already resolved
  `tasks_dir`, yet the ledger block re-runs `tasks_dir_path_build`: a second
  upward filesystem walk and allocation.

- [x] R1.8 (NIT) tatr.c:6816 - `tatr ledger --reason x` with no `--slug` and no
  `--disposition` silently lists and ignores the stray payload flags, while the
  symmetric `--disposition` without `--slug` is refused.

### Round 1 resolution

All eight addressed. Native 107/107 and memcheck green after the fixes.

R1.1 - the pre-write verify now requires `verify.payload` to equal the intended
payload byte for byte, plus `verify.defer_count == match.count` for DEFER.
Mutation-tested: dropping ONLY the payload comparison writes the reviewer's
exact corrupted line (`ABSORBED 2026-07-31 by scaffold ) and check`) and exits
0, so the `closing_paren` assertion genuinely kills.

R1.2 - assertions added for the duplicate-slug refusal, the malformed-entry
refusal, `--target`/`--task` extra-arg refusals, the empty-payload guard, the
stray-payload-while-listing guard, and `--disposition` without `--slug`. The
`verify.disposition`/`verify.count` terms the reviewer counted as a sixth guard
are NOT independently killable and are not claimed as such: the verify parse
reads the same leading count group as the original parse, so those two terms
cannot disagree once the payload matches. They stay as round-trip completeness
- a check that compares only part of what it parsed would be the smell - and
the enclosing `if` is killed by the R1.1 mutation above.

Fixing R1.2 exposed a hang, not just a missing message. With the
malformed-entry refusal disabled, `tatr ledger --slug broken ...` never
returned: the R1.3 fallback yields an entry with `problem` set but
`count_end`/`paren_close` NULL, and the splice computes
`(unsigned long)(NULL - match_line.str)`, which wraps to a runaway length and
spins in the string builder. The guard was load-bearing against a hang while
being entirely untested. `main_ledger` now checks the splice invariant and
calls `AIDS_UNREACHABLE`, so removing that guard aborts in milliseconds with
`spliceable entry without a count group` instead of hanging the suite.

R1.3 - `ledger_entry_parse` remembers that a line opened a `(x<digits>` group;
if no valid group closes, the line is reported as bad-disposition rather than
skipped as prose.

R1.4 - a DEFER whose recorded count is above the entry's own count is now
bad-disposition, with its own checker assertion.

R1.5 - count runs are bounded at `LEDGER_COUNT_MAX_DIGITS` (9) in both
`ledger_entry_parse` and `check_ledger`; an over-long run is not a counter.

R1.6 - the README console example uses placeholder slugs.

R1.7 - the ledger block reuses the `tasks_dir` the `-I <id>` branch already
resolved; only a full scan walks for one.

R1.8 - `--task`, `--reason` and `--target` without `--disposition` are refused
instead of silently ignored.

## Round 2

- REVIEWER: out-of-context
- VERDICT: APPROVE

No BLOCKER or MAJOR. The four findings below are MINOR/NIT; R2.1, R2.2 and
R2.3 were fixed anyway and R2.4 is accepted as-is.

The reviewer independently built the branch, ran native and memcheck to
107/107, ran all eight proofs on their own criteria, re-checked every round-1
finding against the diff rather than against the resolution prose, and ran 26
guard-by-guard mutations (23 killed). Its two surviving mutants outside R2.2
are the `verify.disposition`/`verify.count` and `verify.defer_count` terms,
which round 1 disclosed as round-trip completeness rather than independently
killable - the reviewer's mutations confirm that disclosure rather than
contradict it.

- [x] R2.1 (MINOR) tatr.c:8264 - the new `ledger` dispatch arm swallowed the
  `check` arm's `return_defer(result)`. Behaviour-neutral today because nothing
  sits between the chain and `defer:`, but it is an unintended edit to an
  unrelated command and no test can see it.
  Suggested: restore `return_defer(result);` in the `check` branch.
  - Response: fixed. Confirmed against `git show master:tatr.c` that the line
    was present before this branch, so it is a regression this diff
    introduced, not a pre-existing gap.

- [x] R2.2 (MINOR) tatr.c:6942 - the `%s takes no --reason` refusal has no
  assertion; deleting its side effect keeps the suite green, so
  `ABSORBED --target t --reason r` would silently drop the reason. Its two
  siblings are asserted and the mutation-test step is ticked, so this is the
  one guard the tick does not cover.
  Suggested: assert `ABSORBED takes no --reason` in
  `test_ledger_disposition_atomicity`.
  - Response: fixed. Assertion added and mutation-tested: with the guard
    disabled the command writes `decide-me ABSORBED t` and exits 0, so the new
    assertion genuinely kills. The step's tick now covers every guard it
    claims except the two disclosed above.

- [x] R2.3 (MINOR) tatr.c:6446 - a `- ` bullet under Pending with no
  `(x<digits>` at all is invisible to every rule, so stripping a count parks a
  lesson silently. Same shape R1.3 closed for a malformed count group, and it
  contradicts DECISION.md's "every entry either carries a disposition or is a
  finding".
  Suggested: treat a countless Pending bullet as bad-disposition.
  - Response: fixed. `ledger_check_pending_entry` now reports
    `a pending entry needs an '(xN)' count`, with its own assertion. Verified
    that a wrapped continuation line is not `- ` prefixed once trimmed and so
    is not flagged, and that this repo's own LESSONS.md stays clean.

- [ ] R2.4 (NIT) tatr.c:7114 - the round-trip refusal blames "an unbalanced
  parenthesis in the text is the usual cause" for every mismatch, including a
  `--reason`/`--target` with surrounding whitespace, which the parser trims.
  Correct and safe, but the message misdirects.
  Suggested: reject surrounding whitespace with its own message.
  - Response: accepted, not fixed. The refusal is correct and the ledger is
    untouched either way; the message is the only defect. Left open at the
    implementer's discretion rather than widening this branch's diff.

### The mandatory user decision

The reviewer flagged, correctly, that it could not verify from the diff whether
the two `PROMOTE` dispositions in LESSONS.md reflect an actual user decision -
the one thing the Story makes mandatory. They did not: they were recorded by an
earlier session with no record of the user being asked.

Both were put to the user this round and both were confirmed as PROMOTE:
`mutation-test-the-new-guard` -> 20260731-002334 and
`test-first-for-check-messages` -> 20260731-002339. The dispositions on disk
are unchanged because the user chose what was already written, but they are now
the user's decision rather than an assumed one.

No open `manual:` DoD items.
