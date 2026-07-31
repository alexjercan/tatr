---
name: tatr
description: Track work items with the tatr CLI, which stores tasks as markdown files under a tasks/ directory so they are versioned with the code. Use this skill whenever a project has a tasks/ directory, the user mentions tasks, TODOs, backlog, TASK.md, or task tracking, or Codex needs to create, inspect, update, lint, or close project task records.
---
# Tatr - Task Tracking for Code Projects
Tatr stores tasks as `tasks/<YYYYMMDD-HHMMSS>/TASK.md` files. The timestamp
directory is the task ID. Tatr searches upward to find `tasks/`; `-r ROOT`
runs against another tree.

## Critical Rules
- Prefer the CLI over hand edits: `tatr new`, `ls`, `show`, `edit`, `flow`,
  `scaffold`, `proofs`, `frontier`, `context`, `claim`, `release`, `check`,
  and `ledger`.
- `STATUS`, `FLOW STEP`, and `PLAN STATUS` are owned by `tatr flow`; `new` and
  `edit` reject direct writes to them.
- Planning approval is only `tatr flow <id> --to PLANNED`. Do not treat
  checked steps as approval.
- Start work with `tatr flow <id> --to WORKING`; it enforces approved plans,
  closed dependencies, and cross-session claims.
- Scaffold sibling records (`SPIKE.md`, `DECISION.md`, `REVIEW.md`,
  `RETRO.md`) with `tatr scaffold`; edit existing records by hand in the diff.
- `tatr proofs <id>` prints proof commands; it never executes them.
- `tatr check` exits 1 on findings and uses the same rule collectors as
  lifecycle gates.
- Ask the user before recording any lessons ledger disposition with
  `tatr ledger`; never infer PROMOTE, DEFER, RETIRE, or ABSORBED.
- Deletion must go through a validated HUID and only touch `tasks/<id>/`.

## Common Flow
```bash
tatr ls --sort priority
tatr show <id>
tatr context <id> --phase work
tatr flow <id> --to WORKING
tatr proofs <id>
tatr check --ledger LESSONS.md
```

## Load On Demand
Read one only when its condition holds.
- command syntax or subcommand behavior -> `commands.md`
- planning, lifecycle state, gates, or status meaning -> `lifecycle.md`
- sibling records, scaffolding, proofs, or ledger disposition -> `records.md`
- `tatr check` findings, rule slugs, or exemptions -> `check-rules.md`
- `tatr ls -f` filters -> `filtering.md`
- manual `TASK.md` body or metadata edits -> `format.md`
- claims, frontier, or parallel worktrees -> `claims.md`
- pickup, finish, docs, lessons, or project workflow -> `workflow.md`
