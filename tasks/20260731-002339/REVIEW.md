# Review: Write the test-first-for-check-messages practice into AGENTS.md

- TASK: 20260731-002339
- BRANCH: master (done directly on master at the user's instruction)

## Round 1

- REVIEWER: out-of-context
- VERDICT: APPROVE

Both findings fixed in the same working tree before commit. No open BLOCKER or
MAJOR.

- [x] R1.1 (MAJOR) AGENTS.md:101-103 - the sentence inverted the logic it was
  teaching. A negative assertion `! echo "$output" | grep -q "decided-lesson"`
  is DEFEATED, not "satisfied", by a fixture name containing that slug: the
  grep matches, the `!` negates, and the test goes RED. As written a reader
  would conclude the collision causes a silent false PASS and would hunt the
  wrong symptom when it bites.
  Suggested: say the assertion is broken by any OTHER fixture containing the
  slug, and that the test fails for a reason unrelated to the rule.
  - Response: fixed, and re-derived in session before adopting: with an
    `undecided-lesson` fixture the output contains the substring, `grep -q`
    matches, `!` makes the clause false, and the test fails spuriously. The
    prose now says "is broken by any OTHER fixture whose name contains that
    slug ... the test fails for a reason that has nothing to do with the rule".
    Teaching the wrong failure symptom in an instructions file other agents
    follow is worth the MAJOR.

- [x] R1.2 (NIT) tasks/20260731-002339/TASK.md:20-22 - Step 2 was ticked for
  two clauses but the prose delivered only the first ("designed from the
  assertion rather than reverse-engineered"); "the assertion is a real one from
  the start" was not stated.
  Suggested: state it, or untick the dropped half.
  - Response: fixed by stating it rather than unticking - "Writing it first
    also proves the assertion is a real one: it fails before the rule exists,
    so you have seen it go red."

The reviewer verified the surviving factual claims against the tree rather than
on trust: `test_ledger_pending_requires_disposition` does name its undecided
entry `open-lesson` (checker.sh:2457) and does carry
`! echo "$output" | grep -q "decided-lesson"` (checker.sh:2470), so the
substring collision is real; `grep -qx` matches whole lines where bare `grep -q`
matches substrings.

One honesty correction the reviewer flagged as unverified rather than as a
finding, fixed anyway: the first draft claimed `open-lesson` was chosen over
`undecided-lesson` "for exactly this reason". No record supports that motive -
only the present-tense fact. The prose now says the test "is a live example"
and that naming it `undecided-lesson` instead WOULD trip the assertion, which
is checkable, instead of asserting an intent that is not.

Pending USER checks - the manual DoD items, which APPROVE does not resolve:

- `## Testing` carries the subsection, stating assertion-before-emitter and its
  reason.
- The substring-collision pitfall is stated with the `open-lesson` example.
