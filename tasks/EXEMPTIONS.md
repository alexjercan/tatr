# Historical schema exemptions

`tatr check` validates every sibling record against the schema table in
`tatr.c`, and `tatr flow` gates its transitions on the same rules. The records
below were written before the rule they now trip, and the flow trail is
append-only history: a task record is not rewritten to satisfy a rule invented
after it landed. Each line classifies one such record explicitly - 37 lines
over 21 of this repository's 31 tasks.

Format, one exemption per line:

```
- <task-id> <rule>: <why this record is exempt>
```

An entry suppresses that rule for that task only. An entry that never fires is
reported as `unused-exemption` on a full `tatr check`, so the list cannot rot:
when a record is legitimately rewritten, its exemption must go with it.

New work does not get exemptions. Scaffold the record with
`tatr scaffold <id> <RECORD>` and it is schema-clean from the first byte.

## Pre-flow records (2026-03-29 .. 2026-07-18)

These predate the /flow suite entirely. Their REVIEW.md files are a single
`- VERDICT:` line with no round structure, reviewer or task pointer, and their
RETRO.md files use free-form headings.

- 20260329-123700 bad-record-schema: pre-flow REVIEW/RETRO, free-form headings
- 20260329-123700 bad-review-round: pre-flow REVIEW.md, single verdict, no rounds
- 20260330-202358 bad-record-schema: pre-flow REVIEW/RETRO, free-form headings
- 20260330-202358 bad-review-round: pre-flow REVIEW.md, single verdict, no rounds
- 20260331-141856 bad-record-schema: pre-flow REVIEW/RETRO, free-form headings
- 20260331-141856 bad-review-round: pre-flow REVIEW.md, single verdict, no rounds
- 20260331-144635 bad-record-schema: pre-flow REVIEW/RETRO, free-form headings
- 20260331-144635 bad-review-round: pre-flow REVIEW.md, single verdict, no rounds
- 20260401-142442 bad-record-schema: pre-flow REVIEW/RETRO, free-form headings
- 20260401-142442 bad-review-round: pre-flow REVIEW.md, single verdict, no rounds
- 20260403-194821 bad-record-schema: pre-flow REVIEW/RETRO, free-form headings
- 20260403-194821 bad-review-round: pre-flow REVIEW.md, single verdict, no rounds
- 20260403-194846 bad-record-schema: pre-flow REVIEW/RETRO, free-form headings
- 20260403-194846 bad-review-round: pre-flow REVIEW.md, single verdict, no rounds
- 20260410-183029 bad-record-schema: pre-flow REVIEW/RETRO, free-form headings
- 20260410-183029 bad-review-round: pre-flow REVIEW.md, single verdict, no rounds
- 20260705-172803 bad-record-schema: pre-flow REVIEW/RETRO, free-form headings
- 20260705-172803 bad-review-round: pre-flow REVIEW.md, single verdict, no rounds
- 20260705-172804 bad-record-schema: pre-flow REVIEW/RETRO, free-form headings
- 20260705-172804 bad-review-round: pre-flow REVIEW.md, single verdict, no rounds
- 20260705-172805 bad-record-schema: pre-flow REVIEW/RETRO, free-form headings
- 20260705-172805 bad-review-round: pre-flow REVIEW.md, single verdict, no rounds
- 20260705-172806 bad-record-schema: pre-flow REVIEW/RETRO, free-form headings
- 20260705-172806 bad-review-round: pre-flow REVIEW.md, single verdict, no rounds
- 20260709-193044 bad-record-schema: pre-flow REVIEW/RETRO, free-form headings
- 20260709-193044 bad-review-round: pre-flow REVIEW.md, single verdict, no rounds
- 20260718-235158 bad-record-schema: pre-flow REVIEW/RETRO, free-form headings
- 20260718-235158 bad-review-round: pre-flow REVIEW.md, single verdict, no rounds

## Early-flow records (2026-07-20 .. 2026-07-22)

The flow suite arrived here and the record shapes settled over the following
week. These carry rounds and verdicts but predate the `# Review:` / `# Retro:`
title convention, the `- TASK:` / `- BRANCH:` pointers, or the fixed retro
section vocabulary.

- 20260720-152503 bad-record-schema: RETRO predates the TASK/BRANCH/REVIEW ROUNDS header and the fixed section set
- 20260720-171902 bad-record-schema: RETRO predates the '## What to improve next time' section
- 20260720-220046 bad-record-schema: REVIEW/RETRO predate the titled-record and TASK/BRANCH pointer convention
- 20260720-225230 bad-record-schema: pre-flow-suite epic container and its free-form REVIEW/RETRO
- 20260720-225230 bad-review-round: REVIEW.md is a single verdict, written before rounds existed
- 20260720-233308 bad-record-schema: REVIEW/RETRO predate the titled-record and TASK/BRANCH pointer convention
- 20260722-151939 bad-record-schema: pre-flow-suite epic container and its free-form REVIEW/RETRO
- 20260722-151939 bad-review-round: REVIEW.md is a single verdict, written before rounds existed

## Recent records

- 20260730-153325 bad-record-schema: RETRO omits BRANCH and the improve/action-item sections; the cycle's lessons went straight to LESSONS.md
