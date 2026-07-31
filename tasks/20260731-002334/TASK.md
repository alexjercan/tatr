# Write the mutation-test-the-new-guard practice into AGENTS.md

- STATUS: OPEN
- PRIORITY: 60
- TAGS: docs,testing,lessons
- KIND: TASK
- FLOW STEP: BACKLOG
- PLAN STATUS: DRAFT


## Story

As a tatr maintainer, I want the mutation-testing practice recorded in
AGENTS.md, so a guard's own test is proven to hold it before review rather than
after. Promoted from the lessons ledger at x3 (20260730-154657, 20260730-154745,
20260730-154740, and again in 20260730-154756).

## Notes

- Promoted by user disposition on 2026-07-31; the ledger entry points here.
- The practice: delete each new guard one at a time and watch ITS OWN test go
  red, before review. The mutation must remove the SIDE EFFECT, not just the
  return value - `0 * report(...)` still prints.
- 20260730-154756 is the worked example: mutating one guard turned nothing red,
  which exposed two redundant guards; splitting their responsibilities made both
  falsifiable. A guard no test can kill is the finding.
