---
name: tatr
description: Track work items with the tatr CLI, which stores tasks as markdown files under a tasks/ directory so they are versioned with the code. Use this skill whenever a project has a tasks/ directory, the user mentions tasks, TODOs, backlog, TASK.md, or task tracking, or Codex needs to create, inspect, update, lint, or close project task records.
---

# Tatr - Task Tracking for Code Projects

Tatr stores tasks as `tasks/<YYYYMMDD-HHMMSS>/TASK.md` files. The timestamp
directory is the task ID. Tatr searches upward from the current directory to
find `tasks/`, so it works from anywhere inside the project; `-r ROOT` runs it
against another directory.

## Commands

```bash
tatr [-r ROOT] <subcommand> [options]

tatr new "Title" [-p <priority>] [-t tag1,tag2] [-s OPEN|IN_PROGRESS|CLOSED] [-b <file>]
tatr ls [-s created|priority|title] [-R] [-f '<query>']
tatr show <id>
tatr edit <id> [-T "New title"] [-p <priority>] [-t tag1,tag2] [-s <status>]
tatr rm <id>
tatr check [<id>] [-L|--ledger <file>]
```

- `tatr new` creates the task directory and TASK.md, then prints the task ID. Default status is OPEN, default priority 0. `-b/--body-file <file>` seeds the description body from a file (`-` reads stdin) - prefer it over creating an empty task and editing the file afterwards. If the generated ID already exists (two `new` calls in the same second), tatr fails instead of overwriting; retry after the second changes.
- `tatr ls` prints one line per task: `<filepath>: [PRIORITY: N, TAGS: ...] Title`. `-s/--sort` orders by `created` (default), `priority` (descending), or `title`; `-R` recurses into nested `tasks/` dirs; `-f` filters with the query language below.
- `tatr show <id>` prints one task's full details: title, status, priority, tags, and the whole description body, with a clickable file path.
- `tatr edit <id>` updates fields in place without opening an editor. Only passed flags change; the description body is preserved. `-t` replaces the tag set, it does not merge. Invalid status values are rejected and the file is left untouched.
- `tatr rm <id>` deletes the task's directory after validating the ID and resolving the existing `tasks/<id>/` path.
- `tatr check` lints task artifacts for process drift. Findings print as `<id>: <rule>: <detail>`, exit 1 on any finding, and exit 0 with no output when clean. There is no `--strict` flag: record-completeness checks are on by default. With `<id>`, it checks one task. With `-L/--ledger <file>`, it also checks the lessons ledger for stalled promotions.

## Check Rules

Default `tatr check` rules:

- `closed-unchecked`: a CLOSED task has unchecked `- [ ]` items under `## Steps`.
- `closed-missing-review`: a CLOSED task has no `REVIEW.md`.
- `closed-missing-retro`: a CLOSED task has no `RETRO.md`.
- `closed-not-approved`: a CLOSED task's latest `- VERDICT:` line in REVIEW.md is not APPROVE, or there is no verdict line.
- `bad-severity`: a REVIEW.md finding uses a severity outside BLOCKER|MAJOR|MINOR|NIT.
- `malformed-header`: TASK.md is missing, unreadable, does not parse, or has a STATUS token other than exactly OPEN, IN_PROGRESS, or CLOSED. Whitespace and line endings count.
- `bad-flow-state`: a `## Flow State` marker has an invalid exact value.
- `unplanned-in-progress`: an IN_PROGRESS task lacks `PLAN STATUS: APPROVED` under `## Flow State`.
- `bad-decision-status`: a task's `DECISION.md`, when present, has a `- STATUS:` value that is not `ACCEPTED` nor `SUPERSEDED by <ref>`, or has no STATUS line.
- `dangling-supersede`: a `DECISION.md` supersede reference, either in `SUPERSEDED by <ref>` or `- Supersedes: <ref>`, does not resolve to an existing `tasks/<id>/DECISION.md`.
- `promotion-stalled`: with `--ledger <file>`, a lesson at `(x3)` or more appears outside the ledger's `## Pending promotions` section. Annotated counts such as `(x3, PROMOTED ...)`, `(x3, absorbed by ...)`, or `(x3, RETIRED ...)` are lifecycle markers and are exempt.

The `DECISION.md` rules are presence-gated: a task with no `DECISION.md` is
not flagged by those rules. The approved-plan marker is required only for
IN_PROGRESS tasks, so CLOSED tasks and ordinary OPEN backlog items do not need
it.

## Filtering

`tatr ls -f` takes a small query language over task fields (`:status`,
`:priority`, `:tags`), with operators `eq`, `contains`, `in` (with `[...]`
lists) and the connectives `and`, `or`, `not`, grouped with parentheses:

```bash
tatr ls -f '(:status eq OPEN)'
tatr ls -f ':tags contains feature'
tatr ls -f '(:status eq OPEN) and (:tags contains feature)'
tatr ls -f ':tags contains v0.8.0' --sort priority
```

Filtering composes with `-s/--sort` and `-R`. Prefer `-f` over piping
`tatr ls` through `grep`.

## Task File Format

Keep the metadata header exact; tatr owns it:

```markdown
# Task Title

- STATUS: OPEN
- PRIORITY: 100
- TAGS: feature, security

<free-form description body>
```

Status values are case-sensitive: `OPEN`, `IN_PROGRESS`, `CLOSED`. Priority is
a non-negative integer, higher = more important. Slot priority relative to the
existing backlog (`tatr ls --sort priority` first), not on an absolute scale.
Projects may define scheduling-tag conventions in AGENTS.md; check before
tagging.

For non-trivial tasks, structure the body as a story:

```markdown
## Story

As a <who>, I want <what>, so that <why>. Include the context a cold session
needs to start.

## Steps

- [ ] Concrete, verifiable actions.

## Definition of Done

- Observable outcomes. Each item names its proof: `(test: <name>)`,
  `(cmd: <command>)`, or `(manual: <what the user confirms>)`.

## Notes

- Constraints, file pointers, Depends on: <task-id>, sequencing.
```

Trivial tasks may use a plain paragraph. Prefer `tatr edit` for metadata fields
and `tatr new -b` for the initial body; hand-edit the file only for later body
updates.

Flow-managed work may also include:

```markdown
## Flow State

- FLOW STEP: UNDERSTANDING|PLANNING|PLANNED|WORKING|REVIEWING|COMPOUNDING|DONE
- PLAN STATUS: APPROVED
```

`PLAN STATUS: APPROVED` is durable proof that the user accepted the plan gate
before work began. Do not treat `## Steps` checkboxes as proof that planning
was approved.

Sibling records live next to TASK.md in the same task folder: `SPIKE.md`,
`DECISION.md`, `REVIEW.md`, `RETRO.md`, and `NOTES.md`. Use the project's
AGENTS.md for conventions around explicit epic, sprint, version, release, or
multi-feature containers.

## Workflow

Picking up work:

1. Run `tatr ls --sort priority` or `tatr ls -f '(:status eq OPEN)' --sort priority`.
2. Run `tatr show <id>` and read the full task plus sibling records.
3. If the task is flow-managed, confirm it has `PLAN STATUS: APPROVED` before `tatr edit <id> -s IN_PROGRESS`; otherwise plan it first.
4. Append implementation notes to the task description as you go.

Finishing work:

1. Run the project's tests and `tatr check --ledger LESSONS.md` when the repo has a lessons ledger.
2. Record what changed and why, difficulties encountered and how they were diagnosed, and a short self-reflection in the task record or sibling retro, following the repo's AGENTS.md.
3. Run `tatr edit <id> -s CLOSED` only when the work is truly done and the task has the records required by `tatr check`.
4. Commit the task changes together with the code changes.

Planning work:

- Keep one cohesive requested thing in one task. Split only when pieces are independently implementable or the user explicitly asks for an epic, sprint, version, release, or multi-feature container.
- Use consistent tags within the project, such as `feature`, `bug`, `refactor`, `testing`, `docs`, `security`, and `performance`, plus any project scheduling tags.
- Create tasks for non-trivial follow-up work discovered mid-session instead of leaving TODO comments in code.

With worktrees: when work will happen in a sprout worktree, sprout first and
run `tatr new` inside the worktree so the task file is born on the branch. If a
task stub was unavoidably created in the shared main checkout, copy it into the
worktree and remove the main-checkout stub as the first worktree act.

## Gotchas

- "No 'tasks' directory found": create `tasks/` at the project root first.
- A task only shows in `tatr ls` if its directory matches `YYYYMMDD-HHMMSS` and contains a well-formed TASK.md.
- Timestamps are local time.
- IDs are second-resolution. A same-second `tatr new` fails with "already exists" instead of overwriting; retry once the second changes. Run one `tatr new` per command rather than chaining several.
