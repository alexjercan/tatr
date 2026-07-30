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

tatr new "Title" [-p <priority>] [-t tag1,tag2] [-b <file>] [<metadata options>]
tatr ls [-s created|priority|title] [-R] [-f '<query>']
tatr show <id>
tatr edit <id> [-T "New title"] [-p <priority>] [-t tag1,tag2] [<metadata options>]
tatr flow <id> [-o|--to <STEP>]
tatr rm <id>
tatr check [<id>] [-L|--ledger <file>]
```

`<metadata options>`, shared by `new` and `edit`:

```bash
-k|--kind TASK|EPIC|STORY|SPIKE
-P|--parent <id>
-d|--depends-on <id>
```

`STATUS`, `FLOW STEP` and `PLAN STATUS` are NOT among them. They are written
only by `tatr flow`; `new` and `edit` reject `-s`, `-f` and `-S` with a pointer
to it.

- `tatr new` creates the task directory and TASK.md, then prints the task ID. Defaults are `OPEN`, priority 0, `KIND: TASK`, `FLOW STEP: BACKLOG`, `PLAN STATUS: DRAFT`, and no relationships. `-b/--body-file <file>` seeds the description body from a file (`-` reads stdin) - prefer it over creating an empty task and editing the file afterwards. If the generated ID already exists (two `new` calls in the same second), tatr fails instead of overwriting; retry after the second changes.
- `tatr ls` prints one line per task: `<filepath>: [PRIORITY: N, KIND: K, FLOW STEP: F, TAGS: ...] Title`. `-s/--sort` orders by `created` (default), `priority` (descending), or `title`; `-R` recurses into nested `tasks/` dirs; `-f` filters with the query language below.
- `tatr show <id>` prints one task's full details: the whole record exactly as it would be written back, with a clickable file path.
- `tatr edit <id>` updates fields in place without opening an editor. Only passed flags change; the description body is preserved. `-t` and `-d` replace their lists, they do not merge; an empty value (`-P ""`, `-d ""`) clears an optional relationship field. Invalid values are rejected and the file is left untouched.
- `tatr flow <id>` moves the task one step along the lifecycle, or to `--to <STEP>`. It is the only writer of `STATUS`, `FLOW STEP` and `PLAN STATUS`, and it refuses any transition whose preconditions are unmet, reporting every one of them and leaving TASK.md byte-identical. See Lifecycle below.
- `tatr rm <id>` deletes the task's directory after validating the ID and resolving the existing `tasks/<id>/` path.
- `tatr check` lints task artifacts for process drift. Findings print as `<id>: <rule>: <detail>`, exit 1 on any finding, and exit 0 with no output when clean. There is no `--strict` flag: record-completeness checks are on by default. With `<id>`, it checks one task. With `-L/--ledger <file>`, it also checks the lessons ledger for stalled promotions.

## Check Rules

Default `tatr check` rules:

- `closed-unchecked`: a CLOSED task has unchecked `- [ ]` items under `## Steps`.
- `closed-missing-review`: a CLOSED task has no `REVIEW.md`.
- `closed-missing-retro`: a CLOSED task has no `RETRO.md`.
- `closed-not-approved`: a CLOSED task's latest `- VERDICT:` line in REVIEW.md is not APPROVE, or there is no verdict line.
- `bad-severity`: a REVIEW.md finding uses a severity outside BLOCKER|MAJOR|MINOR|NIT.
- `malformed-header`: TASK.md is missing, unreadable, or its title and metadata block do not parse. This covers every invalid metadata token - STATUS, KIND, FLOW STEP, PLAN STATUS, PARENT, DEPENDS ON - because the parser validates the exact bytes it consumes. Whitespace and line endings count.
- `unplanned-in-progress`: an IN_PROGRESS task lacks `- PLAN STATUS: APPROVED`. `KIND: EPIC` containers are exempt.
- `bad-decision-status`: a task's `DECISION.md`, when present, has a `- STATUS:` value that is not `ACCEPTED` nor `SUPERSEDED by <ref>`, or has no STATUS line.
- `dangling-supersede`: a `DECISION.md` supersede reference, either in `SUPERSEDED by <ref>` or `- Supersedes: <ref>`, does not resolve to an existing `tasks/<id>/DECISION.md`.
- `promotion-stalled`: with `--ledger <file>`, a lesson at `(x3)` or more appears outside the ledger's `## Pending promotions` section. Annotated counts such as `(x3, PROMOTED ...)`, `(x3, absorbed by ...)`, or `(x3, RETIRED ...)` are lifecycle markers and are exempt.

The `DECISION.md` rules are presence-gated: a task with no `DECISION.md` is
not flagged by those rules. The approved plan is required only for IN_PROGRESS
tasks, so CLOSED tasks and ordinary OPEN backlog items do not need it, and
`KIND: EPIC` containers are exempt from the record-completeness rules,
`closed-unchecked` and `unplanned-in-progress` alike.

## Filtering

`tatr ls -f` takes a small query language over task fields (`:status`,
`:priority`, `:title`, `:tags`, `:kind`, `:flow_step`, `:plan_status`,
`:parent`, `:depends`), with operators `eq`, `contains`, `in` (with `[...]`
lists) and the connectives `and`, `or`, `not`, grouped with parentheses:

```bash
tatr ls -f '(:status eq OPEN)'
tatr ls -f ':tags contains feature'
tatr ls -f '(:status eq OPEN) and (:tags contains feature)'
tatr ls -f ':tags contains v0.8.0' --sort priority
tatr ls -f ':kind eq EPIC'
tatr ls -f '(:plan_status eq APPROVED) and (:flow_step in [BACKLOG, PLANNED])'
tatr ls -f ':parent eq 20260730-153122'
tatr ls -f ':depends contains 20260730-153325'
```

The enum-valued fields (`:status`, `:kind`, `:flow_step`, `:plan_status`) take
`eq` and `in`; `:parent` takes `eq`; `:tags` and `:depends` take `contains`.

Filtering composes with `-s/--sort` and `-R`. Prefer `-f` over piping
`tatr ls` through `grep`.

## Task File Format

Keep the metadata header exact; tatr owns it:

```markdown
# Task Title

- STATUS: OPEN
- PRIORITY: 100
- TAGS: feature, security
- KIND: TASK
- FLOW STEP: BACKLOG
- PLAN STATUS: DRAFT
- PARENT: 20260730-153122
- DEPENDS ON: 20260730-153325, 20260730-154745

<free-form description body>
```

The first six fields are required and always written back, in exactly this
order. `PARENT` and `DEPENDS ON` are optional and appear only when set. All
values are case-sensitive and validated on the exact token: `STATUS` is
`OPEN|IN_PROGRESS|CLOSED`, `KIND` is `TASK|EPIC|STORY|SPIKE`, `FLOW STEP` is
`BACKLOG|UNDERSTANDING|PLANNING|PLANNED|WORKING|REVIEWING|COMPOUNDING|DONE`,
and `PLAN STATUS` is `DRAFT|APPROVED|NOT_REQUIRED`. Priority is a non-negative
integer, higher = more important. Slot priority relative to the existing
backlog (`tatr ls --sort priority` first), not on an absolute scale. Projects
may define scheduling-tag conventions in AGENTS.md; check before tagging.

`PARENT` and `DEPENDS ON` hold task IDs from the same `tasks/` tree; a parent
in another repository belongs in the body prose instead. tatr validates them as
IDs only - existence and cycles are not checked.

There is no migration path from the older format that had no `KIND` line and
kept flow state under a `## Flow State` heading. Such a record is rejected with
a diagnostic naming the file and the field; correct it by hand.

Everything after the metadata block is body text: opaque, and preserved byte
for byte. A bullet is a bullet, even an uppercase one. Blank lines and leading
whitespace between fields are tolerated and normalized away on the next write,
but a key written with no value (`- PARENT:`) is an error, not body text.

tatr never writes a record it cannot read back: `new` and `edit` re-parse the
serialized bytes before writing and fail without touching disk if the result
would not parse (a newline in a title or tag is the usual cause).

`tatr ls` skips a record that does not parse, names it on stderr, and exits
non-zero - so one broken record cannot hide the rest of the backlog, and
listing is a safe way to find what still needs correcting.

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

`PLAN STATUS: APPROVED` is durable proof that the user accepted the plan gate
before work began. It is written by the plan gate alone - `tatr flow <id> --to
PLANNED` - and by nothing else. Do not treat `## Steps` checkboxes as proof
that planning was approved. `NOT_REQUIRED` is for a record whose cycle never
carried plan state, such as pre-flow history; it is unreachable through the CLI
and written by hand, so it says so honestly rather than back-dating an approval
that never happened.

## Lifecycle

Eight edges, and nothing else:

```
BACKLOG -> UNDERSTANDING -> PLANNING -> PLANNED -> WORKING -> REVIEWING
                                          ^                      |
                                          |                      v
                                          +------- (fix) --- COMPOUNDING -> DONE
```

A bare `tatr flow <id>` takes the single successor of the current step.
`REVIEWING` is the only step with two, defaulting to `COMPOUNDING`, so the fix
loop back to work is `tatr flow <id> --to WORKING`. `DONE` is terminal.

`STATUS` is derived from the step, never chosen: BACKLOG/UNDERSTANDING/
PLANNING/PLANNED are OPEN, WORKING/REVIEWING/COMPOUNDING are IN_PROGRESS, and
DONE is CLOSED. A task therefore stays IN_PROGRESS through review and compound
and closes atomically at DONE.

Three edges are gates:

- `PLANNING -> PLANNED` is the plan gate; its effect is `PLAN STATUS: APPROVED`.
- `PLANNED -> WORKING` needs `PLAN STATUS: APPROVED` and every `DEPENDS ON` id
  to resolve to a CLOSED task.
- `REVIEWING -> COMPOUNDING` needs a `REVIEW.md` whose latest `- VERDICT:` is
  APPROVE with no unticked BLOCKER or MAJOR finding.
- `COMPOUNDING -> DONE` needs all of the above, plus zero unchecked `## Steps`
  items, a `RETRO.md`, and a valid `- STATUS:` on a `DECISION.md` when the task
  carries one.

`KIND: EPIC` containers are exempt from the plan-approval, review, retro and
unchecked-Steps requirements, matching `tatr check`. Their dependencies and
`DECISION.md` are not exempt.

Sibling records live next to TASK.md in the same task folder: `SPIKE.md`,
`DECISION.md`, `REVIEW.md`, `RETRO.md`, and `NOTES.md`. Use the project's
AGENTS.md for conventions around explicit epic, sprint, version, release, or
multi-feature containers.

## Workflow

Picking up work:

1. Run `tatr ls --sort priority` or `tatr ls -f '(:status eq OPEN)' --sort priority`.
2. Run `tatr show <id>` and read the full task plus sibling records.
3. Claim it with `tatr flow <id> --to WORKING`. The command refuses a task whose plan is not approved or whose dependencies are still open, and names which; plan it first if so.
4. Append implementation notes to the task description as you go.

Finishing work:

1. Run the project's tests and `tatr check --ledger LESSONS.md` when the repo has a lessons ledger.
2. Record what changed and why, difficulties encountered and how they were diagnosed, and a short self-reflection in the task record or sibling retro, following the repo's AGENTS.md.
3. Walk the tail of the lifecycle: `tatr flow <id>` to REVIEWING, again to COMPOUNDING once `REVIEW.md` carries an APPROVE verdict, and again to DONE - which closes the task - once the Steps are ticked and `RETRO.md` is written. A refusal lists exactly what is missing.
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
- A record already in a wrong or impossible state is repaired by hand in `TASK.md`. There is no `--force` and no repair command: the hand correction shows up in the diff, where a reviewer sees it. Fix the fields to a consistent state (the STATUS a FLOW STEP implies) and let `tatr check` confirm.
- IDs are second-resolution. A same-second `tatr new` fails with "already exists" instead of overwriting; retry once the second changes. Run one `tatr new` per command rather than chaining several.
