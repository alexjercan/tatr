# Retro: Write the test-first-for-check-messages practice into AGENTS.md

- TASK: 20260731-002339
- BRANCH: master (done directly on master at the user's instruction)
- REVIEW ROUNDS: 1

## What went well

The out-of-context reviewer was worth it on a docs-only diff. The review skill
calls a process-defining docs change substantive rather than trivial, and that
judgement paid: the round's MAJOR was a logic inversion in prose that reads
perfectly well until you trace it, and an in-session reviewer carrying the
author's intent would very likely have skimmed past it.

Asking the reviewer to fact-check each claim against the code, naming the
specific claims, produced better verification than a generic "review this
diff". It read the actual test, the actual landed commit, and the actual
`0 * report(...)` idiom instead of accepting the prose.

## What went wrong

**The pitfall paragraph taught the wrong failure symptom.** It said a negative
assertion `! grep -q "decided-lesson"` is "satisfied by" a fixture name
containing that slug. The opposite is true: the grep matches, the `!` negates,
and the test goes RED. Root cause: the sentence was written from the ledger
note's shorthand ("`undecided-lesson` matching a `! grep decided-lesson`
assertion") without tracing what MATCHING does to the enclosing negation. The
shorthand was accurate about the collision and silent about the direction, and
the prose inherited that silence as an error. A reader would have hunted a
silent false pass for a defect that actually announces itself as a red test.

**A ticked step delivered only half its text.** Step 2 promised two clauses -
the format is designed from the assertion, AND the assertion is a real one from
the start. The prose delivered the first. The tick came from the step feeling
done rather than from re-reading its literal words, which is exactly the
failure the review dimension warns about.

**An unprovable motive was asserted as fact.** The first draft said
`open-lesson` was named that way "for exactly this reason". No record supports
it; only the present-tense collision is checkable. The reviewer flagged it as
unverified rather than as a finding, and it was still worth fixing - an
instructions file should not launder a guess as history.

## What to improve next time

- When writing prose about a boolean assertion, trace the whole expression
  including its negation before describing what "happens". Name the observable
  symptom (red test / silent pass) rather than saying a condition is
  "satisfied", which is ambiguous about which side of the `!` you mean.
- Re-read a step's literal text against the artifact before ticking it, clause
  by clause, when the step contains an "and".
- In an instructions file, write the checkable counterfactual ("naming it X
  instead WOULD trip the assertion") rather than a historical motive you cannot
  source.

## Action items

- None. The ledger entry for `test-first-for-check-messages` should move from
  `## Pending promotions` to its own section carrying the applied marker
  `PROMOTED <date> -> AGENTS.md Testing`, which is this cycle's ledger update.
