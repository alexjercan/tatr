# Tatr Lifecycle

`PLAN STATUS: APPROVED` is durable proof that the user accepted the plan gate.
It is written only by `tatr flow <id> --to PLANNED`. Do not treat checked
`## Steps` items as approval. `NOT_REQUIRED` is for pre-flow history and is
written by hand, not the CLI.

Eight lifecycle edges exist:

```text
BACKLOG -> UNDERSTANDING -> PLANNING -> PLANNED -> WORKING -> REVIEWING
                                          ^                      |
                                          |                      v
                                          +------- (fix) --- COMPOUNDING -> DONE
```

A bare `tatr flow <id>` takes the single successor of the current step.
`REVIEWING` defaults to `COMPOUNDING`; the fix loop is
`tatr flow <id> --to WORKING`. `DONE` is terminal.

`STATUS` is derived from the step:

- `BACKLOG`, `UNDERSTANDING`, `PLANNING`, `PLANNED` -> `OPEN`
- `WORKING`, `REVIEWING`, `COMPOUNDING` -> `IN_PROGRESS`
- `DONE` -> `CLOSED`

Gated edges:

- `PLANNING -> PLANNED`: the plan gate; writes `PLAN STATUS: APPROVED`.
- `PLANNED -> WORKING`: needs approval, closed dependencies, and no conflicting
  claim.
- `REVIEWING -> COMPOUNDING`: needs a schema-clean `REVIEW.md` whose latest
  verdict is `APPROVE` with no unticked `BLOCKER` or `MAJOR` finding.
- `COMPOUNDING -> DONE`: needs all earlier gates, zero unchecked `## Steps`
  items, a schema-clean `RETRO.md`, and a valid `DECISION.md` status when that
  sibling exists.

`KIND: EPIC` containers are exempt from plan approval, review, retro, and
unchecked-Steps requirements, matching `tatr check`. Their dependencies and
`DECISION.md` are not exempt.
