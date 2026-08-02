# Historical schema exemptions

`tatr check` validates every sibling record against the schema table in
`tatr.c`, and `tatr flow` gates its transitions on the same rules. The records
below were written before the rule they now trip, and the flow trail is
append-only history: a task record is not rewritten to satisfy a rule invented
after it landed. Each line classifies one such record explicitly - 24 lines
over 24 of this repository's 39 tasks.

Two forms, one exemption per line:

```
- <task-id> <rule>: <why this record is exempt>
- <task-id>: <why this record is exempt>
```

The first suppresses that one rule for that task. The second suppresses every
rule for that task, and is the right shape when a record's whole header or body
predates the schema rather than one clause of it: listing four rules for one
pre-flow record says nothing the one line does not.

An entry of either form that never fires is reported as `unused-exemption` on a
full `tatr check`, so neither form can rot: when a record is legitimately
rewritten, its exemption must go with it.

New work does not get exemptions. Scaffold the record with
`tatr scaffold <id> <RECORD>` and it is schema-clean from the first byte.

## Pre-flow records (2026-03-29 .. 2026-07-18)

These predate the /flow suite entirely. Their REVIEW.md files are a single
`- VERDICT:` line with no round structure, reviewer or task pointer; their
RETRO.md files use free-form headings; and their v0 headers recorded
`PLAN STATUS: NOT_REQUIRED`, which `tatr migrate` mapped to no `PLAN` gate -
truthfully, since no plan was ever approved for them. A cursor at COMPOUNDING
without that gate is `inconsistent-gates`, and it is: the drift is real and
historical, which is exactly what an exemption is for.

- 20260329-123700: pre-flow record; free-form REVIEW/RETRO and no plan gate
- 20260330-202358: pre-flow record; free-form REVIEW/RETRO and no plan gate
- 20260331-141856: pre-flow record; free-form REVIEW/RETRO and no plan gate
- 20260331-144635: pre-flow record; free-form REVIEW/RETRO and no plan gate
- 20260401-142442: pre-flow record; free-form REVIEW/RETRO and no plan gate
- 20260403-194821: pre-flow record; free-form REVIEW/RETRO and no plan gate
- 20260403-194846: pre-flow record; free-form REVIEW/RETRO and no plan gate
- 20260410-183029: pre-flow record; free-form REVIEW/RETRO and no plan gate
- 20260705-172803: pre-flow record; free-form REVIEW/RETRO and no plan gate
- 20260705-172804: pre-flow record; free-form REVIEW/RETRO and no plan gate
- 20260705-172805: pre-flow record; free-form REVIEW/RETRO and no plan gate
- 20260705-172806: pre-flow record; free-form REVIEW/RETRO and no plan gate
- 20260709-193044: pre-flow record; free-form REVIEW/RETRO and no plan gate
- 20260718-235158: pre-flow record; free-form REVIEW/RETRO and no plan gate

## Early-flow records (2026-07-20 .. 2026-07-22)

The flow suite arrived here and the record shapes settled over the following
week. These carry rounds and verdicts but predate the `# Review:` / `# Retro:`
title convention, the `- TASK:` / `- BRANCH:` pointers, or the fixed retro
section vocabulary - and, like the pre-flow set, they closed without a recorded
plan gate.

- 20260720-152503: RETRO predates the TASK/BRANCH header and the fixed section set; no plan gate
- 20260720-171902: RETRO predates '## What to improve next time'; no plan gate
- 20260720-220046: REVIEW/RETRO predate the titled-record and pointer convention; no plan gate
- 20260720-225230: pre-flow-suite epic container and its free-form REVIEW/RETRO
- 20260720-233308: REVIEW/RETRO predate the titled-record and pointer convention; no plan gate
- 20260722-151939: pre-flow-suite epic container and its free-form REVIEW/RETRO

Three more from the same week carry schema-clean records and trip only the
missing plan gate, so they keep the narrow form:

- 20260720-220059 inconsistent-gates: closed before plan approval was recorded
- 20260720-220114 inconsistent-gates: closed before plan approval was recorded
- 20260722-152010 inconsistent-gates: closed before plan approval was recorded

## Recent records

- 20260730-153325 bad-record-schema: RETRO omits BRANCH and the improve/action-item sections; the cycle's lessons went straight to LESSONS.md
