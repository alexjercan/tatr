# Write the test-first-for-check-messages practice into AGENTS.md

- PRIORITY: 60
- TAGS: docs, testing, lessons
- KIND: TASK
- ACTIVITY: COMPOUNDING
- GATES: PLAN REVIEW RETRO
- RESOLUTION: DONE

## Story

As a tatr maintainer, I want the test-first practice for check-rule messages
recorded in AGENTS.md next to the checker.sh gotcha, so a rule's output format
is designed from its assertion rather than reverse-engineered into it. Promoted
from the lessons ledger at x3 (20260722-152010, 20260725-111031,
20260730-153325).

## Steps

- [x] Add a `### Writing a check rule` subsection under `## Testing`, beside
      the `checker.sh gotcha`, stating the practice: write the checker.sh
      assertion carrying the rule's EXACT expected message before the code
      that emits it.
- [x] Say why: the message format is then designed from the assertion rather
      than reverse-engineered into it, and the assertion is a real one from the
      start.
- [x] Record the exact-message pitfall: a negative assertion
      (`! echo "$out" | grep -q "<slug>"`) is satisfied by any fixture name
      CONTAINING that slug, so fixture names must not be substrings of one
      another. Cite the live example - `test_ledger_pending_requires_disposition`
      names its undecided entry `open-lesson`, not `undecided-lesson`, so the
      `! grep -q "decided-lesson"` assertion cannot match it.
- [x] Note `grep -qx` for whole-line matches on a rule's output.
- [x] Keep it to prose in AGENTS.md; no code or checker.sh change.

## Definition of Done

- `## Testing` carries the subsection, stating assertion-before-emitter and
  its reason (manual: read the new subsection in AGENTS.md).
- The substring-collision pitfall is stated with the `open-lesson` example
  (manual: read the new subsection in AGENTS.md).
- The cited example matches what checker.sh actually contains
  (cmd: `grep -n "open-lesson" checker.sh`).
- Full native and memory-check suites pass
  (cmd: `nix develop -c ./checker.sh && nix develop -c ./checker.sh --memcheck`).
- `tatr check` is clean (cmd: `nix develop -c ./dist/tatr check`).

## Notes

- Promoted by user disposition on 2026-07-31; the ledger entry points here.
- The practice: for a check rule, write the checker.sh assertion with the exact
  expected message BEFORE the emitting code.
- 20260730-154756 followed it for `promotion-awaiting-decision`,
  `bad-disposition` and `dangling-promotion-task`; the fixture-slug collision it
  surfaced (`undecided-lesson` matching a `! grep decided-lesson` assertion) is
  worth mentioning as a pitfall of exact-message tests.
