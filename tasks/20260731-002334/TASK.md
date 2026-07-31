# Write the mutation-test-the-new-guard practice into AGENTS.md

- STATUS: CLOSED
- PRIORITY: 60
- TAGS: docs, testing, lessons
- KIND: TASK
- FLOW STEP: DONE
- PLAN STATUS: APPROVED

## Story

As a tatr maintainer, I want the mutation-testing practice recorded in
AGENTS.md, so a guard's own test is proven to hold it before review rather than
after. Promoted from the lessons ledger at x3 (20260730-154657, 20260730-154745,
20260730-154740, and again in 20260730-154756).

## Steps

- [x] Add a `### Mutation-test every new guard` subsection under `## Testing`,
      after the `checker.sh gotcha`, stating the practice: delete each new
      guard's SIDE EFFECT one at a time, rebuild, and watch that guard's OWN
      test go red, before review.
- [x] Record the two failure modes the practice detects: a guard no mutation
      can kill (it needs a test, or it is redundant and the redundancy must be
      disclosed), and a mutant that hangs or corrupts instead of refusing
      cleanly (the code below assumed a precondition nobody asserts there).
- [x] Record the hygiene rule: revert the mutation in the same step that
      observes the red, so a deliberately broken build never outlives the step.
- [x] Keep it to prose in AGENTS.md; no code or checker.sh change.

## Definition of Done

- `## Testing` carries the subsection, naming side-effect deletion, the
  per-guard red, and the pre-review timing
  (manual: read the new subsection in AGENTS.md).
- The unkillable-guard and hang/corrupt-mutant failure modes are both stated
  (manual: read the new subsection in AGENTS.md).
- The revert-in-the-same-step rule is stated
  (manual: read the new subsection in AGENTS.md).
- Full native and memory-check suites pass
  (cmd: `nix develop -c ./checker.sh && nix develop -c ./checker.sh --memcheck`).
- `tatr check` is clean (cmd: `nix develop -c ./dist/tatr check`).

## Notes

- Promoted by user disposition on 2026-07-31; the ledger entry points here.
- The practice: delete each new guard one at a time and watch ITS OWN test go
  red, before review. The mutation must remove the SIDE EFFECT, not just the
  return value - `0 * report(...)` still prints.
- 20260730-154756 is the worked example: mutating one guard turned nothing red,
  which exposed two redundant guards; splitting their responsibilities made both
  falsifiable. A guard no test can kill is the finding.
