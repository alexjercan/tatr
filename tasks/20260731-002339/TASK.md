# Write the test-first-for-check-messages practice into AGENTS.md

- STATUS: OPEN
- PRIORITY: 60
- TAGS: docs,testing,lessons
- KIND: TASK
- FLOW STEP: BACKLOG
- PLAN STATUS: DRAFT


## Story

As a tatr maintainer, I want the test-first practice for check-rule messages
recorded in AGENTS.md next to the checker.sh gotcha, so a rule's output format
is designed from its assertion rather than reverse-engineered into it. Promoted
from the lessons ledger at x3 (20260722-152010, 20260725-111031,
20260730-153325).

## Notes

- Promoted by user disposition on 2026-07-31; the ledger entry points here.
- The practice: for a check rule, write the checker.sh assertion with the exact
  expected message BEFORE the emitting code.
- 20260730-154756 followed it for `promotion-awaiting-decision`,
  `bad-disposition` and `dangling-promotion-task`; the fixture-slug collision it
  surfaced (`undecided-lesson` matching a `! grep decided-lesson` assertion) is
  worth mentioning as a pitfall of exact-message tests.
