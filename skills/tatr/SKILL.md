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
tatr scaffold <id> [SPIKE|DECISION|REVIEW|RETRO] [-l|--list] [-n|--dry-run]
tatr proofs <id> [-k|--kind test|cmd|manual]
tatr frontier <id>
tatr context <id> [-P|--phase understand|plan|work|review|compound|resume|landing]
tatr claim <id>
tatr release <id> [-F|--force]
tatr claims
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

- `tatr new` creates the task directory and TASK.md, then prints the task ID. Defaults are `OPEN`, priority 0, `KIND: TASK`, `FLOW STEP: BACKLOG`, `PLAN STATUS: DRAFT`, and no relationships. `-b/--body-file <file>` seeds the description body from a file (`-` reads stdin) - prefer it over creating an empty task and editing the file afterwards. If the generated ID already exists (two `new` calls in the same second), tatr fails instead of overwriting; retry after the second changes. Relationships are validated up front: `-k STORY` requires `-P`, and `-P`/`-d` must name tasks that exist, with `-P` naming a `KIND: EPIC`. A refused create creates nothing. `edit` applies the same rule to the references it is asked to set, so an edit that touches something else is not blocked by a dangling edge it did not create.
- `tatr ls` prints one line per task: `<filepath>: [PRIORITY: N, KIND: K, FLOW STEP: F, TAGS: ...] Title`. `-s/--sort` orders by `created` (default), `priority` (descending), or `title`; `-R` recurses into nested `tasks/` dirs; `-f` filters with the query language below.
- `tatr show <id>` prints one task's full details: the whole record exactly as it would be written back, with a clickable file path.
- `tatr edit <id>` updates fields in place without opening an editor. Only passed flags change; the description body is preserved. `-t` and `-d` replace their lists, they do not merge; an empty value (`-P ""`, `-d ""`) clears an optional relationship field. Invalid values are rejected and the file is left untouched.
- `tatr flow <id>` moves the task one step along the lifecycle, or to `--to <STEP>`. It is the only writer of `STATUS`, `FLOW STEP` and `PLAN STATUS`, and it refuses any transition whose preconditions are unmet, reporting every one of them and leaving TASK.md byte-identical. Those preconditions include the record rules below, read through the same functions `tatr check` uses, so a refusal names the same rule slug and no transition can mint a record the lint would flag: `PLANNING -> PLANNED` owes `## Steps`, `## Definition of Done` and a proof on every DoD item; `REVIEWING -> COMPOUNDING` a schema-clean REVIEW.md; `COMPOUNDING -> DONE` additionally a schema-clean RETRO.md and DECISION.md. `tatr scaffold` writes records that satisfy them. See Lifecycle below.
- `tatr scaffold <id> <RECORD>` writes a missing sibling record from the schema table in `tatr.c` - the same table `tatr check` validates against, so a scaffolded record passes the lint with its `TODO` placeholders still in place. Prints `<path><TAB><RECORD>`. Kinds are `SPIKE`, `DECISION`, `REVIEW` and `RETRO`; `TASK.md` is `tatr new`'s job. `--list` prints every kind with its path and `present`/`missing`; `--dry-run` prints the path it would write and writes nothing. It refuses to overwrite an existing record, and there is no `--force`: an existing record is edited by hand, in the diff.
- `tatr proofs <id>` prints each `## Definition of Done` proof as one `<n><TAB><kind><TAB><text>` line, where kind is `test`, `cmd` or `manual`. **Nothing is executed** - a `cmd:` proof's shell text round-trips verbatim, and running it is your decision, in your shell. A whitespace run collapses to a single space only when it holds a newline (a continued bullet's wrap) or a tab (the field separator), so every line is exactly three fields and intra-line spacing survives byte for byte. `-k/--kind` filters to one kind. Use it to get the exact contract to run during `/work` and `/review` instead of re-reading the DoD prose.
- `tatr frontier <id>` prints the open work under an Epic, one tab-separated row per child and never a task body: `<STATE><TAB><id><TAB>p<priority><TAB><flow step><TAB><title>`, plus `<TAB>blocked-by=<ids>` on a BLOCKED row. STATE is READY, BLOCKED or CLAIMED. The order is deterministic - READY, BLOCKED, CLAIMED, then priority descending, then id ascending - so it is safe to diff between runs. CLOSED children and non-children never appear. Use it to pick up work without reading every task.
- `tatr context <id> --phase <phase>` prints only the artifact paths that phase needs, as `<path><TAB>present|missing`. Paths only, never contents. Phases: `understand`, `plan`, `work`, `review`, `compound`, `resume` (the default, everything), `landing`. `understand`, `plan` and `resume` also list the parent Epic's TASK.md. A record the phase owns is listed even when missing, because you need the path to create it.
- `tatr claim <id>` / `tatr release <id>` / `tatr claims` divide work between parallel sessions. A claim is an atomic `O_CREAT|O_EXCL` file; exactly one of any number of racing sessions wins and the losers are told who holds it. `tatr flow <id> --to WORKING` refuses a task another session holds. Ownership is `TATR_SESSION` (default: the working directory), never a pid - tatr is one-shot, so the claiming process is gone before anything reads the claim. `TATR_CLAIMS_DIR` (default `<tasks dir>/.claims`) is where claims live: **set it to one shared directory across parallel worktrees**, or each tree gets its own and the guard can never fire. A session releases its own claim with no flag; `release --force` recovers one from a session that is gone. Nothing expires.
- `tatr rm <id>` deletes the task's directory after validating the ID and resolving the existing `tasks/<id>/` path.
- `tatr check` lints task artifacts for process drift. Findings print as `<id>: <rule>: <detail>`, exit 1 on any finding, and exit 0 with no output when clean. There is no `--strict` flag: record-completeness checks are on by default. With `<id>`, it checks one task. With `-L/--ledger <file>`, it also checks the lessons ledger for stalled promotions.

## Check Rules

Default `tatr check` rules:

- `bad-record-schema`: a record does not match its schema - wrong title prefix, a missing or empty required `- KEY:` header field, or a missing or empty required `## ` section. Covers `TASK.md`, `SPIKE.md`, `DECISION.md`, `REVIEW.md` and `RETRO.md`. Scaffold the record with `tatr scaffold` rather than guessing the shape.
- `bad-review-round`: a `REVIEW.md` has no `## Round 1` heading, or its rounds are not numbered from 1 without gaps.
- `bad-verdict`: a review round has no `- VERDICT:` line, or one outside APPROVE|REQUEST_CHANGES.
- `missing-reviewer`: a review round has no `- REVIEWER:` line, or an empty one.
- `bad-finding-id`: a finding ID is not `R<round>.<index>`, sits in a different round than its heading, or skips an index.
- `approve-with-open-findings`: a `REVIEW.md` whose latest verdict is APPROVE still has an unticked BLOCKER or MAJOR finding.
- `bad-proof-syntax`: a `## Definition of Done` item names no `test:`, `cmd:` or `manual:` proof. A wrapped bullet's continuation lines count as part of the item.
- `missing-spike-record`: a planned `KIND: SPIKE` task has no `SPIKE.md`.
- `bad-spike-status`: a `SPIKE.md` `- STATUS:` outside RECOMMENDED|INCONCLUSIVE|DROPPED.
- `dangling-seeded-task`: a task ID under a `SPIKE.md`'s `## Next steps` has no `TASK.md`.
- `dangling-decision-task`: a `DECISION.md`'s `- TASK:` pointer is not a task ID, or names a task that does not exist.
- `nonreciprocal-supersede`: a supersede link resolves one way only - A says `SUPERSEDED by B` but B has no `- Supersedes: A` line, or the reverse.
- `missing-parent` / `missing-dependency`: a `PARENT` or `DEPENDS ON` reference names a task that does not exist.
- `self-parent` / `self-dependency`: a task names itself.
- `duplicate-dependency`: the same ID twice in one `DEPENDS ON` list.
- `parent-cycle` / `dependency-cycle`: following the links from a task returns to it. Every member of a cycle is reported; a task downstream of one is not.
- `bad-epic-relationship`: a `PARENT` that is not a `KIND: EPIC`, or a `KIND: STORY` with no parent.
- `unused-exemption`: an entry in `tasks/EXEMPTIONS.md` never fired on a full scan.
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

The `DECISION.md` and `SPIKE.md` content rules are presence-gated: a task with
no such sibling is not flagged by them, and any SPIKE.md that does exist is
validated whatever the task's kind. Only `missing-spike-record` keys on
`KIND: SPIKE`. The approved plan is required only for
IN_PROGRESS tasks, so CLOSED tasks and ordinary OPEN backlog items do not need
it, and `KIND: EPIC` containers are exempt from the record-completeness rules,
`closed-unchecked` and `unplanned-in-progress` alike.

`## Steps` and `## Definition of Done` are the plan gate's output, so
`bad-record-schema` asks for them only from `- FLOW STEP: PLANNED` on. The
required TASK.md sections are kind-specific: `TASK`/`STORY` owe `## Steps` and
`## Definition of Done`, `EPIC` owes `## Done Means` and `## Child Tasks`, and
`SPIKE` owes `## Question` plus a `SPIKE.md` sibling.

Records written before a rule existed are classified in `tasks/EXEMPTIONS.md`,
one `- <task-id> <rule>: <reason>` line each, rather than rewritten - the flow
trail is append-only history. Any rule can be exempted the same way. Do NOT add
an exemption for new work: scaffold the record and it is clean from the start.
An exemption that never fires is reported as `unused-exemption`, so the list
cannot rot.

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
