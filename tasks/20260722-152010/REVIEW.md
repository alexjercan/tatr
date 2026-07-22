# Review: Add bad-decision-status and dangling-supersede checks to tatr check

- TASK: 20260722-152010
- BRANCH: feature/decision-checks

## Round 1

- VERDICT: APPROVE
- REVIEWER: out-of-context

Out-of-context reviewer built warning-free under -Wall -Wextra, ran the full
suite (70/70) and memcheck (leak-free), checked both backlogs clean, ran every
DoD proof, and hand-probed edge cases (self-reference, target-dir-exists-without-
DECISION.md, garbage ref, empty ref, multiple STATUS lines, CRLF, empty file,
comment-only file, bare-huid ref, inline comments) - all correct. In-session
pass re-derived the load-bearing claim by sabotage: with `check_decision`
neutralized, `test_check_bad_decision_status` and `test_check_dangling_supersede`
FAIL while the three clean-case tests stay green (68/70), confirming the firing
tests pin the implementation. Sabotage reverted.

Only NITs, all optional - none block APPROVE; left to implementer discretion.

- [ ] R1.1 (NIT) tatr.c - `check_decision` scans the buffer twice (STATUS pass,
  Supersedes pass). Fine for small files and matches the existing per-rule scan
  style; a single pass is possible but less clear. Optional.
  - Response: acknowledged, left as-is - matches the existing bad-severity /
    status per-rule scan style; clarity over micro-optimization on small files.
- [ ] R1.2 (NIT) tatr.c - an empty `SUPERSEDED by ` ref prints
  `invalid STATUS 'SUPERSEDED by'` (trailing space trimmed). Correct
  classification, cosmetic spacing loss in the message. Not worth changing.
  - Response: acknowledged, left as-is - the classification is correct and the
    message is still clear.
- [ ] R1.3 (NIT) checker.sh - `test_check_good_decision_status` covers only the
  ACCEPTED clean path; the resolving-SUPERSEDED-by clean path is covered by
  `test_check_resolving_supersede`. Coverage complete across the two. No action.
  - Response: acknowledged, coverage is complete across the two tests.
