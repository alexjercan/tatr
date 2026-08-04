# Workflow

## Plan

- One cohesive request -> one task.
- Split only for independently implementable work or an explicit container/release scope. A container is a task others name as PARENT, not a type.
- Match project tags and relative priorities.
- Record non-trivial follow-up work as tasks, not TODO comments.
- Approval marker: the PLAN gate, recorded by `tatr flow <id>` out of PLANNING.

## Pick up

1. `tatr ls --sort priority` or filter OPEN tasks.
2. `tatr show <id>`; read the task and relevant siblings.
3. `tatr context <id> --phase <phase>`; read listed artifacts.
4. `tatr flow <id>` until ACTIVITY is WORKING; resolve any named gate failure.
   Leaving UNDERSTANDING needs `tatr scaffold <id> DECISION`, filled in.
5. Record useful implementation notes in task artifacts.

## Finish

1. Run project tests and `tatr check`.
2. Record what/why, tradeoffs, bugs/fixes, and next-time improvement in the repository's declared record.
3. `tatr flow <id>` through REVIEWING and COMPOUNDING; the last one closes as DONE.
   Fix loop: `tatr rewind <id> --to WORKING` (clears REVIEW, keeps PLAN).
4. Commit task records with the related change.

## Gotchas

- No `tasks/`: create it at project root.
- Discoverable task: valid timestamp directory plus parseable `TASK.md`.
- IDs: local-time seconds; same-second `new` fails. Retry after the second changes.
- Impossible historical state: repair `TASK.md` by hand, then run `check`.
- No repair command. `rewind --force` clears earned gates and nothing else does.
- Legacy record (`- STATUS: `, `- FLOW STEP: `, `- KIND: `): every command refuses it; `tatr migrate --apply`.
