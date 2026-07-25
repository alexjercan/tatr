# Retro: Add flow-state lint for planned work

- TASK: 20260725-111031
- BRANCH: feature/flow-state-lint
- REVIEW ROUNDS: 1

## What went well

- The checker tests were written before the implementation and failed only on
  the new flow-state behavior, which kept the C change focused.
- Reusing the existing checker patterns for exact STATUS validation and
  historical/goal exemptions avoided a new style of parser.

## What went wrong

- The initial task record did not carry `PLAN STATUS: APPROVED` before the
  worktree started. Root cause: the plan gate existed in chat and later in the
  nix task, but the tatr-side task did not receive the same durable marker
  until the branch was already cut.

## What to improve next time

- When a checker rule will enforce a marker, put that marker on the active
  task before enabling or exercising the rule.
- Keep exact-output checker tests as the first artifact for new lint rules.

## Action items

- [x] Bumped `validate-the-exact-parsed-token` and
  `test-first-for-check-messages` in LESSONS.md.
