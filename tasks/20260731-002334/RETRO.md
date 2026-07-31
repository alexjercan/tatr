# Retro: Write the mutation-test-the-new-guard practice into AGENTS.md

- TASK: 20260731-002334
- BRANCH: master (done directly on master at the user's instruction)
- REVIEW ROUNDS: 1

## What went well

The promotion did what the feature was built for. The lesson reached x3, the
user was asked, the disposition pointed at this task, and the AGENTS.md change
went through plan, review, retro and close rather than being edited in place by
whoever noticed. That is the whole argument of 20260730-154756, exercised on
its own first customer.

Writing the subsection from the worked example rather than from the ledger's
one-liner made it concrete. The ledger line says "watch its own test go red";
the AGENTS.md text can say what the two failure results MEAN, because the
source task had actually hit both.

## What went wrong

Nothing on this task - the reviewer returned no findings against it. Recorded
as a short retro rather than padded.

The one process friction was self-inflicted and caught by the tooling: the
first attempt to move both tasks PLANNING -> PLANNED was refused with
`bad-proof-syntax` on five Definition of Done items, because docs claims were
written as bare assertions with no `test:`, `cmd:` or `manual:` proof. The
guard was right; the plan was sloppy. Docs DoD items are `manual:` proofs, and
one of them (`the cited example matches checker.sh`) was mechanisable as a
`cmd:` proof once the guard forced the question.

## What to improve next time

- When planning a docs task, write DoD items as `manual:` proofs from the
  start. A docs claim is still a claim someone has to check; the proof marker
  is what says who checks it.
- Prefer a `cmd:` proof over a `manual:` one wherever the claim is about what a
  file literally contains - `grep -n "open-lesson" checker.sh` is checkable by
  anyone, forever, where "read the subsection" is not.

## Action items

- None. The ledger entry for `mutation-test-the-new-guard` should move from
  `## Pending promotions` to its own section carrying the applied marker
  `PROMOTED <date> -> AGENTS.md Testing`, which is this cycle's ledger update.
