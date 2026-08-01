# Workflow

## Plan

- One cohesive request -> one task.
- Split only for independently implementable work or explicit EPIC/release scope.
- Match project tags and relative priorities.
- Record non-trivial follow-up work as tasks, not TODO comments.
- Approval marker: `tatr flow <id> --to PLANNED` only.

## Pick up

1. `tatr ls --sort priority` or filter OPEN tasks.
2. `tatr show <id>`; read the task and relevant siblings.
3. `tatr context <id> --phase <phase>`; read listed artifacts.
4. `tatr flow <id> --to WORKING`; resolve any named gate failure.
5. Record useful implementation notes in task artifacts.

## Finish

1. Run project tests and `tatr check`.
2. Record what/why, tradeoffs, bugs/fixes, and next-time improvement in the repository's declared record.
3. Move WORKING -> REVIEWING -> COMPOUNDING -> DONE. Fix-loop to WORKING when needed.
4. Commit task records with the related change.

## Gotchas

- No `tasks/`: create it at project root.
- Discoverable task: valid timestamp directory plus parseable `TASK.md`.
- IDs: local-time seconds; same-second `new` fails. Retry after the second changes.
- Impossible historical state: repair `TASK.md` by hand, then run `check`.
- No lifecycle `--force` or repair command.
