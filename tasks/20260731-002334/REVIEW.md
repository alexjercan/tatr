# Review: Write the mutation-test-the-new-guard practice into AGENTS.md

- TASK: 20260731-002334
- BRANCH: master (done directly on master at the user's instruction)

## Round 1

- REVIEWER: out-of-context
- VERDICT: APPROVE

No findings against this task. The reviewer's two findings both landed on the
sibling task 20260731-002339 and are recorded there.

The reviewer fact-checked this subsection's central claim against the landed
commit rather than taking it on trust, and confirmed it precisely: the
`saw_count_prefix` fallback in `ledger_entry_parse` (tatr.c:6405-6411) returns
true with `problem` set and `count_end`/`paren_close` NULL; the malformed-entry
refusal sits at tatr.c:7033-7037; the splice at tatr.c:7095-7098 computes
`match.count_end - match_line.str`, which wraps for NULL. It also confirmed
that the invariant is now asserted at the point of use (tatr.c:7089-7091), so
the subsection's past tense is correct. `20260730-154756`'s RETRO.md documents
the deleted guard producing an infinite loop rather than a wrong message.

It further confirmed the `0 * report(...)` idiom is real in this codebase -
`ledger_check_promotion_task` (tatr.c:6422-6437) prints and then returns a
count, so zeroing the return value would leave the output intact.

Pending USER checks - the manual DoD items, which APPROVE does not resolve:

- `## Testing` carries the subsection, naming side-effect deletion, the
  per-guard red, and the pre-review timing.
- The unkillable-guard and hang/corrupt-mutant failure modes are both stated.
- The revert-in-the-same-step rule is stated.
