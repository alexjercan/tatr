# Tatr Check Rules

Default `tatr check` findings:

- `bad-record-schema`: wrong title prefix, missing or empty required header
  field, or missing or empty required section in `TASK.md`, `SPIKE.md`,
  `DECISION.md`, `REVIEW.md`, or `RETRO.md`.
- `bad-review-round`: no `## Round 1`, or review rounds are not numbered from
  1 without gaps.
- `bad-verdict`: missing review verdict, or value outside
  `APPROVE|REQUEST_CHANGES`.
- `missing-reviewer`: missing or empty `- REVIEWER:`.
- `bad-finding-id`: finding id is not `R<round>.<index>`, is in the wrong
  round, or skips an index.
- `approve-with-open-findings`: latest verdict is APPROVE with an unticked
  `BLOCKER` or `MAJOR` finding.
- `bad-proof-syntax`: a DoD item has no `test:`, `cmd:`, or `manual:` proof.
- `missing-spike-record`: a planned `KIND: SPIKE` has no `SPIKE.md`.
- `bad-spike-status`: `SPIKE.md` status is outside
  `RECOMMENDED|INCONCLUSIVE|DROPPED`.
- `dangling-seeded-task`: a task id under `SPIKE.md` `## Next steps` has no
  `TASK.md`.
- `dangling-decision-task`: `DECISION.md` `- TASK:` is not an existing task id.
- `nonreciprocal-supersede`: supersede links do not resolve both ways.
- `missing-parent` / `missing-dependency`: referenced task does not exist.
- `self-parent` / `self-dependency`: task names itself.
- `duplicate-dependency`: dependency id appears twice.
- `parent-cycle` / `dependency-cycle`: following links returns to the task.
- `bad-epic-relationship`: parent is not `KIND: EPIC`, or `KIND: STORY` has no
  parent.
- `unused-exemption`: an exemption in `tasks/EXEMPTIONS.md` never fired.
- `closed-unchecked`: a CLOSED task has unchecked `## Steps` items.
- `closed-missing-review`: a CLOSED task has no `REVIEW.md`.
- `closed-missing-retro`: a CLOSED task has no `RETRO.md`.
- `closed-not-approved`: latest review verdict is not APPROVE.
- `bad-severity`: finding severity is outside `BLOCKER|MAJOR|MINOR|NIT`.
- `malformed-header`: `TASK.md` is missing, unreadable, or its title and
  metadata block do not parse exactly.
- `unplanned-in-progress`: an IN_PROGRESS non-EPIC lacks
  `PLAN STATUS: APPROVED`.
- `bad-decision-status`: `DECISION.md` status is not `ACCEPTED` or
  `SUPERSEDED by <ref>`.
- `dangling-supersede`: supersede reference has no target `DECISION.md`.
- `promotion-stalled`: with `--ledger`, a lesson at x3 or more is outside
  `## Pending promotions` without a lifecycle marker.
- `promotion-awaiting-decision`: pending promotion has a bare count, or a DEFER
  whose count has been passed.
- `bad-disposition`: pending promotion annotation does not match the required
  grammar.
- `dangling-promotion-task`: PROMOTE names a missing task.

`DECISION.md` and `SPIKE.md` content rules are presence-gated. A task without
that sibling is not flagged by those rules. `missing-spike-record` keys only on
`KIND: SPIKE`, while any existing `SPIKE.md` is validated.

`bad-record-schema` asks for `## Steps` and `## Definition of Done` only from
`FLOW STEP: PLANNED` onward. `TASK` and `STORY` owe those sections, `EPIC` owes
`## Done Means` and `## Child Tasks`, and `SPIKE` owes `## Question` plus a
`SPIKE.md`.

Historical records are classified in `tasks/EXEMPTIONS.md`:

```text
- <task-id> <rule>: <reason>
```

Any rule can be exempted. Do not add exemptions for new work; scaffold or fix
the record instead. Unused exemptions are findings.
