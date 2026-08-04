# tatr check: historical tag also exempts closed-unchecked

- PRIORITY: 55
- TAGS: feature
- ACTIVITY: COMPOUNDING
- GATES: REVIEW RETRO
- RESOLUTION: DONE

## Story

As a maintainer, I want `historical`-tagged CLOSED tasks to also be exempt from
the `closed-unchecked` rule (not just the strict review/retro rules), so that
frozen pre-flow tasks with genuinely not-done steps (superseded, dropped,
premise-falsified) lint clean WITHOUT rewriting their step boxes - which would
violate the task-history immutability policy.

## Steps

- [x] Extend the check logic: when a CLOSED task is `historical`-tagged, skip the `closed-unchecked` finding (reuse `check_task_is_history_exempt`, or a historical-only variant).
- [x] Decided: reuse the combined helper - `goal` umbrellas also skip closed-unchecked (they usually have no Steps; keep or scope to historical-only).
- [x] checker.sh: a historical task with unchecked Steps is clean; a plain CLOSED task with unchecked Steps still flags.
- [x] Update AGENTS.md note on the historical marker to say it also covers closed-unchecked.

## Definition of Done

- A `historical` CLOSED task with unchecked Steps passes plain `tatr check` (cmd: checker.sh case).
- A non-historical CLOSED task with unchecked Steps still flags (cmd: checker.sh case).
- Zero valgrind leaks (cmd: checker.sh --memcheck).

## Notes

- Follow-on to 20260720-220046 (historical/goal exemption for review/retro).
- Discovered during bevy-common-systems task 20260720-220102: 3 pre-flow tasks (20260704-102342 superseded, 20260705-140043, 20260711-094942 dropped steps) are historical-tagged but still flag closed-unchecked; their boxes must stay verbatim per the immutability policy.
