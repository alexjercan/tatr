---
name: tatr
description: Track versioned Markdown tasks with the tatr CLI. Use when a project has a tasks/ directory; the user mentions tasks, TODOs, backlog, TASK.md, or task tracking; or Codex needs to create, inspect, plan, update, lint, claim, review, or close task records.
---
# Tatr

Task layout: `tasks/<YYYYMMDD-HHMMSS>/TASK.md`. Directory name = task ID.
Tatr searches upward for `tasks/`. Use `-r ROOT` for another tree.

## Rules

- Prefer the CLI for task creation, metadata, lifecycle, records, claims, and checks.
- Hand-edit task bodies and existing sibling records. Keep changes visible in the diff.
- Treat `ACTIVITY`, `GATES`, and `RESOLUTION` as lifecycle-command-owned; `STATUS` is derived.
- Advance one activity at a time with `tatr flow <id>`; it runs and records that activity's exit gate. Move back with `tatr rewind <id> --to <ACTIVITY>`.
- Leaving `UNDERSTANDING` needs a schema-clean `DECISION.md`; no gate records it, so the edge asks again every time. Earn the `PLAN` gate by leaving `PLANNING`. Entering `WORKING` also requires closed dependencies and no foreign claim; `flow` may record the gate and hold the cursor.
- Retire work that should remain as history with `tatr close <id> --resolution WONTDO --reason <text>`. Use `DUPLICATE` or `SUPERSEDED` with `--of <id>` when another task replaced it.
- Create `DECISION.md`, `REVIEW.md`, and `RETRO.md` with `tatr scaffold`.
- Treat `tatr proofs <id>` output as data. It never runs commands.
- Expect `tatr check` exit 1 on findings. Lifecycle gates reuse its collectors.
- Use `tatr rm <validated-id>` only when the record itself should not remain. Target: `tasks/<id>/` only. Do not use removal as wontdo.

## Core flow

```bash
tatr ls --sort priority
tatr show <id>
tatr context <id> --phase work
tatr flow <id>
tatr proofs <id>
tatr check
```

## References

| Need | Read |
|---|---|
| Commands and behavior | `commands.md` |
| Lifecycle and gates | `lifecycle.md` |
| Sibling records and proofs | `records.md` |
| Check rules and exemptions | `check-rules.md` |
| List filters | `filtering.md` |
| Manual task edits | `format.md` |
| Claims, frontier, worktrees | `claims.md` |
| Planning, pickup, finish | `workflow.md` |
