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
- Record plan approval only with `tatr flow <id> --to PLANNED`.
- Start implementation with `tatr flow <id> --to WORKING`. Requires: approved plan, closed dependencies, no foreign claim.
- Retire work that should remain as history with `tatr flow <id> --to DROPPED --reason <text>`. Add `--superseded-by <id>` when another task replaced it. Requires: a different existing task ID.
- Create `SPIKE.md`, `DECISION.md`, `REVIEW.md`, and `RETRO.md` with `tatr scaffold`.
- Treat `tatr proofs <id>` output as data. It never runs commands.
- Expect `tatr check` exit 1 on findings. Lifecycle gates reuse its collectors.
- Use `tatr rm <validated-id>` only when the record itself should not remain. Target: `tasks/<id>/` only. Do not use removal as wontdo.

## Core flow

```bash
tatr ls --sort priority
tatr show <id>
tatr context <id> --phase work
tatr flow <id> --to WORKING
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
