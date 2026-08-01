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
- Treat `STATUS`, `FLOW STEP`, and `PLAN STATUS` as `tatr flow`-owned.
- Record plan approval only with `tatr flow <id> --to PLANNED`.
- Start implementation with `tatr flow <id> --to WORKING`. Requires: approved plan, closed dependencies, no foreign claim.
- Create `SPIKE.md`, `DECISION.md`, `REVIEW.md`, and `RETRO.md` with `tatr scaffold`.
- Treat `tatr proofs <id>` output as data. It never runs commands.
- Expect `tatr check` exit 1 on findings. Lifecycle gates reuse its collectors.
- Ask the user before `tatr ledger --disposition ...`. Never infer a disposition.
- Delete only through `tatr rm <validated-id>`. Target: `tasks/<id>/` only.

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
| Sibling records, proofs, ledger | `records.md` |
| Check rules and exemptions | `check-rules.md` |
| List filters | `filtering.md` |
| Manual task edits | `format.md` |
| Claims, frontier, worktrees | `claims.md` |
| Planning, pickup, finish | `workflow.md` |
