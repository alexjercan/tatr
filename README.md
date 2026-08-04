# tatr

A lightweight, command-line task tracking tool written in C. Manage your TODOs
directly from the terminal with filesystem-based storage.

## Overview

tatr (Task Tracker) is a minimal yet practical tool for managing tasks in your
development workflow. Unlike traditional issue trackers, tatr stores tasks as
Markdown files in your project directory, making them easy to version control,
grep, and edit with your favorite text editor.

While not intended as a full replacement for GitHub Issues, tatr serves as a
fast, offline alternative for personal task management and small projects. It's
primarily a toy project inspired by Tsoding's streams.

## Features

- **Filesystem-based storage**: Tasks stored as Markdown files in a `tasks/` directory
- **Human-readable IDs**: Each task gets a timestamp-based HUID (format: `YYYYMMDD-HHMMSS`)
- **Metadata support**: Track status, priority, and tags for each task
- **Guarded lifecycle**: a freely moving `ACTIVITY` cursor, an earned `GATES` set that only `tatr flow` writes, and a `RESOLUTION` that records why work stopped
- **One record schema**: `tatr scaffold` writes the sibling records (DECISION, REVIEW, RETRO) from the same in-code table `tatr check` validates them against
- **Structured DoD proofs**: `tatr proofs` prints each Definition of Done proof as data - tatr never executes any of it
- **One kind of record**: every task is a task. An epic, a story or a spike is a title, not a type - the tool has no opinion about which is which
- **Task graph**: `PARENT` and `DEPENDS ON` are validated as a graph - dangling links, duplicates, self-links and cycles are findings, not silent waits
- **Parallel sessions**: `tatr frontier` shows the open work under any task, and `tatr claim` divides it between sessions with an atomic filesystem claim
- **Phase context**: `tatr context <id> --phase <phase>` prints only the artifact paths one phase of work needs
- **Full CRUD**: Create, show, edit, and remove tasks entirely from the CLI
- **Flexible listing**: Sort by creation date, priority, or title, and filter with a query language
- **Automation-friendly**: Non-interactive commands make it easy for scripts and agents to drive
- **AI-agent skill**: Includes a `tatr` skill for AI-capable tools that work with task records
- **Terminal integration**: Clickable file paths in OSC 8-compliant terminals
- **Zero configuration**: Works out of the box with no setup required

## Installation

### Using Make

The Makefile builds through the nix dev shell, whose canonical toolchain
(clang, valgrind, and the MinGW cross compiler) is the supported one. A bare
`make` outside nix fails with a build guard pointing you here:

```bash
# Build the Linux binary at dist/tatr
nix develop -c make

# Build a Windows executable at dist/tatr.exe
nix develop -c make windows

# Install to /usr/local/bin (may require sudo)
nix develop -c make install

# Install to a custom location
nix develop -c make install PREFIX=$HOME/.local
```

If you build without nix because you provisioned a C toolchain yourself, set
`TATR_ALLOW_BARE_BUILD=1` to opt out of the guard:

```bash
TATR_ALLOW_BARE_BUILD=1 make        # or: TATR_ALLOW_BARE_BUILD=1 make CC=gcc
```

### Using Nix Flakes

```bash
# Build with nix
nix build

# Build the Windows executable package
nix build .#windows

# Enter development environment
nix develop

# The agent skill is exported for downstream flakes
nix eval .#skills.tatr
```

## AI Agent Skill

This repository ships a `tatr` skill for AI-capable tools that understand
skill folders. The skill lives at `skills/tatr/`. Its `SKILL.md` is a short
entrypoint, with command syntax, task file format, `tatr check`, lifecycle,
records, claims, and workflow details split into referenced markdown files.

Downstream flakes can consume the skill through the exported `skills.tatr`
flake output, which exports the full skill directory, and install it into their
agent skills directory.

### Manual Build

Compiling `tatr.c` directly bypasses the Makefile (and its guard) entirely, so
it works in any environment with a C compiler:

```bash
clang -Wall -Wextra -O2 -g -o tatr tatr.c
```

## Usage

### Creating Tasks

```bash
# Create a basic task
tatr new "Fix memory leak in parser"

# Create a task with metadata
tatr new "Add unit tests" -p 80 -t testing,bug

# Create a child task under a container, blocked on another task
tatr new "Add the frontier view" -P 20260730-153122 -d 20260730-153325

# Create a task with the description body from a file (or '-' for stdin)
tatr new "Refactor the loader" -p 60 -t refactor -b body.md

# Create task from a different directory
tatr -r /path/to/project new "Task title"
```

**Options:**
- `-p, --priority <value>`: Set task priority (default: 0, higher values = higher priority)
- `-t, --tags <value>...`: Comma-separated tags
- `-b, --body-file <path>`: Read the description body from a file; `-` reads stdin
- `-P, --parent <id>`: Set the parent task ID
- `-d, --depends-on <id>...`: Set the dependency task IDs

`edit` takes the same options and replaces the field it is given. On `edit`, an
empty value clears an optional relationship field: `tatr edit <id> -P ""` drops
the parent and `-d ""` drops every dependency.

Relationships are checked before the record is written, so `new` cannot create a
task that `tatr check` would flag the moment it exists: every `-P`/`-d` must
name a task that exists. Any task may be any task's parent - being a container
is having children, not declaring a type. A refused create creates nothing at
all.

```console
$ tatr new "Add the frontier view" -P 20260101-999999
ERROR: Parent '20260101-999999' does not exist

$ tatr new "Add the frontier view" -k STORY
ERROR: '-k' was removed: KIND is not a field: every record under tasks/ is a task
  say what a task is in its title; a container is a task others name as their PARENT
```

`edit` applies the same rule to the references it is asked to set. An edit that
touches something else is not blocked by a dangling edge it did not create - a
reference can still break when its referent is removed, and `tatr check` is what
reports that.

A task is always born with `- ACTIVITY: -`, `- GATES: -` and
`- RESOLUTION: -`: nothing started, nothing proven, nothing decided. Those
three workflow fields cannot be set here; they belong to
[the lifecycle commands](#moving-a-task-through-the-lifecycle).

Task IDs have second resolution (`YYYYMMDD-HHMMSS`). If a task with the
generated ID already exists (two `new` calls in the same second), `tatr new`
fails instead of overwriting it; retry to get a fresh ID.

### Listing Tasks

```bash
# List all tasks (sorted by creation date)
tatr ls

# Sort by priority (highest first)
tatr ls -s priority

# Sort by title (alphabetically)
tatr ls -s title

# Recursively list tasks from all subdirectories
tatr ls -R
```

**Options:**
- `-s, --sort <value>`: Sort by (created, priority, title; default: created)
- `-R, --recursive`: Recursively search for tasks directories in all subdirectories
- `-f, --filter <value>`: Filter tasks with a query expression

A record that does not parse is skipped and named on stderr rather than
aborting the listing, and `ls` then exits non-zero - so one broken record
cannot hide the rest of the backlog, and listing is a safe way to find what
needs correcting. Scripts chaining `tatr ls && ...` should account for that.

**Filtering:**

The `-f` flag takes a small query language over task fields. Fields are written
with a leading colon, combined with operators and grouped with parentheses.
The fields are `:status`, `:priority`, `:title`, `:tags`, `:activity`,
`:gates`, `:resolution`, `:parent` and `:depends`:

```bash
# Only open tasks
tatr ls -f '(:status eq OPEN)'

# Tasks tagged feature
tatr ls -f ':tags contains feature'

# Open feature tasks, combining conditions
tatr ls -f '(:status eq OPEN) and (:tags contains feature)'

# Approved work that has not been picked up yet
tatr ls -f '(:gates contains PLAN) and (:activity in [UNDERSTANDING, PLANNING])'

# Everything that was abandoned rather than finished
tatr ls -f ':resolution in [WONTDO, DUPLICATE, SUPERSEDED]'

# The children of one container, and everything blocked on one task
tatr ls -f ':parent eq 20260730-153122'
tatr ls -f ':depends contains 20260730-153325'
```

The enum-valued fields (`:status`, `:activity`, `:resolution`) take `eq` and
`in`; `:parent` takes `eq`; `:tags`, `:depends` and `:gates` take `contains`.
`:activity` and `:resolution` are nullable, so a record that leaves one unset
matches no value of it. The retired `:kind`, `:flow_step` and `:plan_status`
are refused by name with a pointer at the replacement:

```console
$ tatr ls -f ':kind eq EPIC'
ERROR: Filter error: line 1, col 1: field ':kind' was retired; KIND was removed: every task is a task; match the title with ':title contains <text>' or the hierarchy with ':parent'
```

Supported operators include `eq`, `contains`, `in` (with `[...]` lists), and the
boolean connectives `and`, `or`, `not`. Literal values may contain `.` and `-`
(after an initial letter, digit or `_`), so version-style tags such as `v0.1.0`
or `release-candidate` can be filtered on. Filtering composes with sorting and
recursive mode, and applies per section in recursive mode.

**Output format:**
```
tasks/20260331-144635/TASK.md: [PRIORITY: 100, STATUS: CLOSED, ACTIVITY: COMPOUNDING, TAGS: feature] Implement filter system
tasks/20260330-202358/TASK.md: [PRIORITY: 80, STATUS: IN_PROGRESS, ACTIVITY: WORKING, TAGS: testing, bug] Add unit tests
tasks/20260329-123700/TASK.md: [PRIORITY: 0, STATUS: OPEN, ACTIVITY: -, TAGS: ] Fix memory leak in parser
```

### Showing a Task

Print the full details of a single task by its ID, including the description
body and a clickable file path:

```bash
tatr show 20260331-144635
```

The argument is the task's HUID (the `tasks/<ID>/` directory name). `show` exits
non-zero if the ID is malformed or the task does not exist.

### Editing a Task

Update the descriptive metadata or title of an existing task without opening an
editor. Only the fields you pass are changed; everything else, including the
description body and every workflow field, is left untouched:

```bash
# Bump priority and retitle
tatr edit 20260331-144635 -p 90 -T "Implement query filter language"

# Replace the tag set (edit replaces tags, it does not merge them)
tatr edit 20260331-144635 -t feature -t parser

# Re-home a child task and replace its dependencies
tatr edit 20260331-144635 -P 20260730-153122 -d 20260730-153325
```

**Options:**
- `-T, --title <value>`: New task title
- `-p, --priority <value>`: New priority (non-negative integer)
- `-t, --tags <value>...`: New tags, replacing the existing set
- `-P, --parent <id>`: New parent task ID (empty value clears it)
- `-d, --depends-on <id>...`: New dependency task IDs (empty value clears them)

`edit` cannot set `ACTIVITY`, `GATES` or `RESOLUTION`, and `STATUS` is not a
settable field at all. The flags for them were removed, and both the current
and the retired spellings fail with a pointer to the command that does own the
field, rather than a generic unknown-argument error:

```console
$ tatr edit 20260730-185007 --status CLOSED
ERROR: '--status' was removed: STATUS is not settable through `new` or `edit`
  STATUS is derived from ACTIVITY and RESOLUTION; it is not stored at all
$ tatr edit 20260730-185007 --activity WORKING
ERROR: '--activity' was removed: ACTIVITY is not settable through `new` or `edit`
  move the cursor with `tatr flow <ID>` or `tatr rewind <ID> --to <ACTIVITY>`
$ tatr edit 20260730-185007 --kind EPIC
ERROR: '--kind' was removed: KIND is not a field: every record under tasks/ is a task
  say what a task is in its title; a container is a task others name as their PARENT
```

(The transcripts here elide the `tatr.c:<line>:` source location every
`ERROR:` line carries, which moves with the code; everything after it is
verbatim.)

An invalid value for any option `edit` does accept is rejected before anything
is written, so the task file is left unchanged.

### Moving a Task Through the Lifecycle

The lifecycle is three independent fields, not one chain:

| Field | Job | Nature |
| ------------ | ------------------------ | ------------------------------------------------ |
| `ACTIVITY`   | where attention sits     | a nullable cursor; moves backward as freely as forward; proves nothing |
| `GATES`      | what has been proven     | an accumulating set over `PLAN`, `REVIEW`, `RETRO` |
| `RESOLUTION` | why the work stopped     | nullable and terminal until `tatr reopen`         |

```
ACTIVITY:   - -> UNDERSTANDING -> PLANNING -> WORKING -> REVIEWING -> COMPOUNDING
                                       |                     |             |
GATES:                              +PLAN                +REVIEW       +RETRO
```

Four commands write them, and between them they are the only writers:

```bash
tatr flow 20260331-144635                  # advance one activity, run its exit gate
tatr flow 20260331-144635 --dry-run        # would the line above succeed? exit status answers
tatr rewind 20260331-144635 --to WORKING   # move back; clears the gates that invalidates
tatr close 20260331-144635 --resolution WONTDO --reason "Duplicate work"
tatr reopen 20260331-144635                # clear the resolution, keep the cursor
```

`tatr flow` has no `--to`. A command that can only move forward one activity
cannot skip a gate, which is what makes the gates unavoidable; going backward
is `tatr rewind`, which is a different thing with different consequences.

**STATUS is derived, never chosen, and never stored.** It is a rollup of the
other two nullable fields, so it cannot drift from them:

| Condition          | STATUS      |
| ------------------ | ----------- |
| `RESOLUTION` set   | CLOSED      |
| `ACTIVITY` unset   | OPEN        |
| otherwise          | IN_PROGRESS |

No `TASK.md` contains a `- STATUS: ` line. Every command that reports still
prints one.

**Exit gates.** Each gate is the requirement for LEAVING one activity:

| Leaving       | Earns    | Requires                                                                                       |
| ------------- | -------- | ---------------------------------------------------------------------------------------------- |
| UNDERSTANDING | -        | a schema-clean `DECISION.md`: what the work is for, and which direction was chosen               |
| PLANNING      | `PLAN`   | `## Steps` and `## Definition of Done` well-formed, every DoD item carrying a proof, and a graph position whose references resolve |
| WORKING       | -        | nothing: handing work to review costs nothing; leaving review is what is expensive               |
| REVIEWING     | `REVIEW` | a `REVIEW.md` whose latest `- VERDICT:` is APPROVE, with no unticked BLOCKER or MAJOR finding    |
| COMPOUNDING   | `RETRO`  | a schema-clean `RETRO.md`, plus the close gate below                                             |

Leaving UNDERSTANDING is the one edge with a requirement and no gate. Nothing
is recorded about having met it, so the edge asks again every time it is walked
- a rewind to `UNDERSTANDING` and a fresh advance re-reads the `DECISION.md`
rather than trusting a memory nothing wrote down.

The close gate, which only `--resolution DONE` runs, asks what "done" means
beyond the individual gates: all three gates actually earned, zero unchecked
items under `## Steps`, a valid `- STATUS:` on any `DECISION.md` the task
carries, and every child of the task CLOSED. `tatr flow` out of `COMPOUNDING`
earns `RETRO` and closes as `DONE` in one motion, so the happy path stays one
verb; `tatr close --resolution DONE` does the same thing for a caller who wants
to name it.

The other three resolutions run no gate at all and are legal from any activity.
Anything can be abandoned at any time, and demanding proof before letting go of
a task is exactly backward.

Nothing is exempt from any of this. A container - a task others name as their
`PARENT` - is still a task: it owes the same sections, the same review and the
same retro as anything else, and on top of that it cannot close while a child
of its own is open. A record that genuinely should not be held to a rule says so
in `tasks/EXEMPTIONS.md`, where the waiver is visible and reviewable.

**A refused gate writes nothing.** Every precondition is evaluated before
anything is mutated, and all unmet ones are reported rather than one per round
trip. `--dry-run` runs the same evaluation and stops before the write, so its
exit status is the answer to "would this advance succeed?" and its output is the
refusal it predicts, tensed:

```console
$ tatr flow 20260802-211009
Task 20260802-211009 moved - -> UNDERSTANDING (STATUS: IN_PROGRESS)
$ tatr flow 20260802-211009
ERROR: Refusing to advance 20260802-211009 from UNDERSTANDING: 1 precondition(s) not met
  - there is no DECISION.md: the understanding has not been recorded; write one with `tatr scaffold 20260802-211009 DECISION`
  Record unchanged.
$ tatr scaffold 20260802-211009 DECISION
tasks/20260802-211009/DECISION.md	DECISION
$ tatr flow 20260802-211009
Task 20260802-211009 moved UNDERSTANDING -> PLANNING (STATUS: IN_PROGRESS)
$ tatr flow 20260802-211009
ERROR: Refusing to advance 20260802-211009 from PLANNING: 2 precondition(s) not met
  - bad-record-schema: TASK.md has no '## Steps' section
  - bad-record-schema: TASK.md has no '## Definition of Done' section
  Record unchanged.
$ tatr flow 20260802-211009 --dry-run
Task 20260802-211009 would move PLANNING -> WORKING
  gate PLAN would run
ERROR: Would refuse to advance 20260802-211009 from PLANNING: 2 precondition(s) not met
  - bad-record-schema: TASK.md has no '## Steps' section
  - bad-record-schema: TASK.md has no '## Definition of Done' section
  Record unchanged.
$ echo $?
1
$ $EDITOR tasks/20260802-211009/TASK.md   # write the two missing sections
$ tatr flow 20260802-211009 --dry-run
Task 20260802-211009 would move PLANNING -> WORKING
  gate PLAN would run
$ echo $?
0
$ tatr flow 20260802-211009
gate PLAN recorded
Task 20260802-211009 moved PLANNING -> WORKING (STATUS: IN_PROGRESS)
$ tatr flow 20260802-211009
Task 20260802-211009 moved WORKING -> REVIEWING (STATUS: IN_PROGRESS)
$ tatr flow 20260802-211009
ERROR: Refusing to advance 20260802-211009 from REVIEWING: 1 precondition(s) not met
  - there is no REVIEW.md: the work has not been reviewed
  Record unchanged.
```

(The transcripts here elide the `tatr.c:<line>:` source location every
`ERROR:` line carries, which moves with the code; everything after it is
verbatim.)

**Rewinding costs the gates it invalidates.** Going back to fix something must
never be harder than going forward, so `rewind` runs no gate and asks nothing
of the world. What it does cost is the gates the move discards - replanning
invalidates the review that judged the old plan - and those are named, and
behind `--force` so an earned approval is never discarded silently:

| Rewind to     | Clears               | Why                                       |
| ------------- | -------------------- | ----------------------------------------- |
| UNDERSTANDING | PLAN, REVIEW, RETRO  | everything is up for grabs                |
| PLANNING      | PLAN, REVIEW, RETRO  | replanning invalidates the review         |
| WORKING       | REVIEW, RETRO        | the fix loop; `PLAN` survives             |
| REVIEWING     | REVIEW, RETRO        | re-reviewing discards the old verdict     |

```console
$ tatr rewind 20260802-211009 --to WORKING
Task 20260802-211009 rewound REVIEWING -> WORKING (no gates cleared)
$ tatr rewind 20260802-211009 --to PLANNING
ERROR: Rewinding 20260802-211009 to PLANNING would discard an earned gate; pass --force to clear it
  - the PLAN gate
  Record unchanged.
```

A forward or equal target is refused by name rather than silently accepted: a
rewind that moved forward would be a way to reach an activity without running
its gate.

**Closing, and reopening.** The last `tatr flow` earns `RETRO` and sets the
resolution in the same write. The cursor stays where the work ended rather than
resetting, so reopening restores a live position without having to invent one:

```console
$ tatr flow 20260802-211009
gate REVIEW recorded
Task 20260802-211009 moved REVIEWING -> COMPOUNDING (STATUS: IN_PROGRESS)
$ tatr flow 20260802-211009
ERROR: Refusing to advance 20260802-211009 from COMPOUNDING: 2 precondition(s) not met
  - there is no RETRO.md: the retro has not been written
  - 1 unchecked Steps item(s) remain
  Record unchanged.
$ tatr flow 20260802-211009
gate RETRO recorded
Task 20260802-211009 moved COMPOUNDING -> CLOSED (RESOLUTION: DONE)
$ tatr flow 20260802-211009
ERROR: Task 20260802-211009 is CLOSED (RESOLUTION: DONE): reopen it with `tatr reopen 20260802-211009` before moving it
$ tatr reopen 20260802-211009
Task 20260802-211009 reopened at COMPOUNDING (STATUS: IN_PROGRESS)
```

`WONTDO` requires `--reason <text>` and appends a `## Dropped` record to
`TASK.md`. `DUPLICATE` and `SUPERSEDED` require `--of <ID>`, which is validated
to resolve to another existing task and written as `- DUPLICATE OF: `.
`reopen` clears everything `close` wrote - the resolution, the `- DUPLICATE OF: `
pointer and a trailing `## Dropped` block - so a record cannot accumulate two
closure reasons across a close, reopen and re-close:

```console
$ tatr close 20260802-211009 --resolution DUPLICATE --of 20260101-000000
ERROR: --of task '20260101-000000' does not resolve to another task
```

**`flow` may half-succeed.** Entering `WORKING` is the one edge that asks
anything of anyone else: every `- DEPENDS ON` id must resolve to a CLOSED task,
and no other session may hold a claim. Those are facts about the world, not
about the record, and the two are allowed to disagree. When the gate passes but
the world does not permit the advance, `flow` records the gate, holds the
cursor, and reports both halves - still one atomic `task_save`, so nothing is
left inconsistent:

```console
$ tatr flow 20260802-210549
gate PLAN recorded
ERROR: Not advancing 20260802-210549 to WORKING: 1 precondition(s) not met
  - dependency 20260802-210548 is not CLOSED (STATUS: OPEN)
  Cursor held at PLANNING.
```

This is the case the three-field design exists for. Under a single chain,
approving a plan and starting the work were one edge, so a task could not have
its plan blessed until its dependencies closed. It is also where `PLANNED`
went: not deleted, derived.

```
READY == GATES contains PLAN and ACTIVITY < WORKING and deps CLOSED and unclaimed
```

[`tatr frontier`](#working-a-container-in-parallel) answers that query.

**Options:**
- `flow`: `-n, --dry-run` probes the advance - it evaluates every precondition
  the real call evaluates, prints the edge, the gate and the same unmet report,
  writes nothing, and exits non-zero when the advance would not complete
- `rewind`: `-o, --to <ACTIVITY>` (required), `-F, --force`
- `close`: `-x, --resolution <R>` (required), `-O, --of <ID>`, `-R, --reason <TEXT>`

There is no repair command. A transition may never produce a state `tatr check`
would flag: both read the same artifacts through the same helpers. A record that
is already in the wrong state is corrected by hand in `TASK.md`, where the fix
shows up in the diff and a reviewer sees it.

### Migrating Legacy Records

The record format changed twice: v1.0.0 replaced the `FLOW STEP` chain, and
v1.1.0 removed `KIND`. Every command refuses a record that still carries
`- STATUS: `, `- FLOW STEP: ` or `- KIND: `, so each migration is total rather
than opt-in - an unmigrated record is not ignored, it is unreadable:

```console
$ tatr show 20260101-090000
ERROR: task_deserialize: this record is in the v0 format (it still carries '- STATUS: '); run `tatr migrate --apply` to convert it
$ tatr show 20260101-091500
ERROR: task_deserialize: this record still carries '- KIND: ' (KIND was removed: every task is a task); run `tatr migrate --apply` to convert it
```

`tatr migrate` is dry-run by default and prints one line per record it would
change:

```console
$ tatr migrate
20260101-090000	FLOW STEP: PLANNED, PLAN STATUS: APPROVED -> ACTIVITY: PLANNING, GATES: PLAN, RESOLUTION: -
20260101-091500	KIND: EPIC -> dropped
2 record(s) would change; nothing was written. Re-run with --apply.
$ tatr migrate --apply
20260101-090000	FLOW STEP: PLANNED, PLAN STATUS: APPROVED -> ACTIVITY: PLANNING, GATES: PLAN, RESOLUTION: -
20260101-091500	KIND: EPIC -> dropped
2 record(s) migrated
```

The mapping is record-local, so the command works unchanged in any repository:

| v0                              | v1                                          |
| ------------------------------- | ------------------------------------------- |
| `- STATUS: <any>`               | dropped; derived                            |
| `- FLOW STEP: BACKLOG`          | `ACTIVITY: -`                               |
| `- FLOW STEP: PLANNED`          | `ACTIVITY: PLANNING` + the `PLAN` gate      |
| `- FLOW STEP: DONE`             | `ACTIVITY: COMPOUNDING` + `RESOLUTION: DONE`|
| `- FLOW STEP: DROPPED`          | `RESOLUTION: WONTDO`, keeping `- REASON: `  |
| other steps                     | the same name                               |
| `- PLAN STATUS: APPROVED`       | the `PLAN` gate                             |
| `- PLAN STATUS: DRAFT` or `NOT_REQUIRED` | no `PLAN` gate                     |
| `REVIEW.md` with a latest APPROVE | the `REVIEW` gate                         |
| `RETRO.md` present              | the `RETRO` gate                            |
| `- KIND: <any>`                 | dropped; there is one kind of record        |

Only metadata headers move: no `REVIEW.md` or `RETRO.md` body is rewritten. A
schema version bump is a different thing from backfilling history. A v1 record
is rewritten line-wise - the `- KIND: ` line is deleted and every other byte is
kept - so a migration cannot quietly reformat a body.

`tatr migrate` is the only legacy-format knowledge in the binary, and it is
quarantined there on purpose.

### Removing a Task

Delete a task's directory (its `TASK.md` and anything else inside it) by ID:

```bash
tatr rm 20260331-144635
```

`rm` only ever touches the validated `tasks/<ID>/` directory and exits non-zero
if the ID is malformed or the task does not exist.

### Checking Task Artifacts

Lint the backlog for process drift. `check` walks every task (or one, by ID)
and prints findings one per line as `<id>: <rule>: <detail>`, exiting 1 if
anything was found and 0 (silently) when clean:

```bash
tatr check                    # lint every task
tatr check 20260331-144635    # lint one task
```

**Default rules:**
- `bad-record-schema`: a record does not match its schema - the wrong title
  prefix, a missing or empty required `- KEY:` header field, or a missing or
  empty required `## ` section. Applies to `TASK.md` (see below),
  `DECISION.md`, `REVIEW.md` and `RETRO.md`.
- `bad-review-round`: a `REVIEW.md` has no `## Round 1` heading, or its rounds
  are not numbered from 1 without gaps.
- `bad-verdict`: a review round has no `- VERDICT:` line, or one outside
  APPROVE|REQUEST_CHANGES.
- `missing-reviewer`: a review round has no `- REVIEWER:` line, or an empty one.
- `bad-finding-id`: a finding's ID is not `R<round>.<index>`, sits in a
  different round than its heading, or skips an index.
- `approve-with-open-findings`: a `REVIEW.md` whose latest verdict is APPROVE
  still has an unticked BLOCKER or MAJOR finding.
- `bad-proof-syntax`: a `## Definition of Done` item names no `test:`, `cmd:`
  or `manual:` proof. A wrapped bullet's continuation lines count as part of
  the item.
- `dangling-decision-task`: a `DECISION.md`'s `- TASK:` pointer is not a task
  ID, or names a task that does not exist.
- `nonreciprocal-supersede`: a supersede link resolves in only one direction -
  A says `SUPERSEDED by B` but B carries no `- Supersedes: A` line, or the
  reverse.
- `missing-parent` / `missing-dependency`: a `PARENT` or `DEPENDS ON` reference
  names a task that does not exist. A dangling dependency is a broken graph, not
  a blocker to wait for.
- `self-parent` / `self-dependency`: a task names itself as its own parent or
  its own dependency.
- `duplicate-dependency`: the same ID appears twice in one `DEPENDS ON` list.
- `parent-cycle` / `dependency-cycle`: following `PARENT` or `DEPENDS ON` from
  the task returns to it. Every member of a cycle is reported; a task merely
  downstream of one is not.
- `unused-exemption`: an entry in `tasks/EXEMPTIONS.md` never fired on a full
  scan (reported against the task it names).
- `closed-unchecked`: a `RESOLUTION: DONE` task still has unchecked `- [ ]`
  items under its `## Steps` section (other sections may keep open boxes).
- `closed-missing-review`: a `RESOLUTION: DONE` task has no `REVIEW.md`.
- `closed-missing-retro`: a `RESOLUTION: DONE` task has no `RETRO.md`.
- `closed-not-approved`: a `RESOLUTION: DONE` task's REVIEW.md exists but its
  latest `- VERDICT:` line is not APPROVE (or there is no verdict at all).
- `inconsistent-gates`: the cursor is past an activity whose gate the record
  does not carry - `ACTIVITY: REVIEWING` with no `PLAN` in `GATES`, say. A
  single chain made this unrepresentable by conflating position with proof;
  two free axes can disagree, so a rule has to say so. Work started without an
  approved plan is the `PLANNING` case of this rule and has no rule of its own.
- `dangling-duplicate-of`: a `- DUPLICATE OF: ` that names the task itself or
  a task that does not exist.
- `dropped-missing-reason`: a `RESOLUTION: WONTDO` task has no non-empty
  `- REASON:` line.
- `dropped-bad-superseder`: a `- SUPERSEDED BY: ` that does not resolve to
  another task.
- `bad-severity`: a REVIEW.md finding uses a severity outside
  BLOCKER|MAJOR|MINOR|NIT.
- `malformed-header`: TASK.md is missing/unreadable, or its title and metadata
  block do not parse. This covers every invalid metadata value: the parser
  validates the exact token it consumes, so `- ACTIVITY: PLANNED`, a trailing
  space after a value or a CRLF tail all land here rather than in a rule of
  their own. A record still carrying `- FLOW STEP: ` or `- KIND: ` is a legacy
  record and lands here too; `tatr migrate` is the fix.
- `bad-decision-status`: a task's `DECISION.md` (when present) has a `- STATUS:`
  value that is not `ACCEPTED` nor `SUPERSEDED by <ref>`, or has no STATUS line.
- `dangling-supersede`: a `DECISION.md` supersede reference - in a
  `SUPERSEDED by <ref>` status or a `- Supersedes: <ref>` line - does not
  resolve to an existing `tasks/<id>/DECISION.md`.

The plan-gate requirement applies only once a task's cursor is past
`PLANNING`, so unstarted backlog items are never asked for one.

There is no container type and no container exemption. A task other tasks name
as their `PARENT` is a task like any other, held to every rule here, and its
children are simply the records that point at it. A record that genuinely
should not be held to a rule says so in `tasks/EXEMPTIONS.md`.

The `DECISION.md` rules are presence-gated for `check`: a task without one is
never flagged for its contents. (`tatr flow` does ask for one on the way out of
`UNDERSTANDING`, which is a lifecycle precondition rather than a lint rule.)

`## Steps` and `## Definition of Done` are the plan gate's output, so
`bad-record-schema` asks for them only once `PLAN` is in `GATES` - a task
`tatr new` just created is not a finding the moment it exists. Every task owes
the same two sections: whatever a task is called, a plan is steps and a
definition of what done means.

The same rules gate the lifecycle. `tatr flow` reads them through the same
functions `check` does, so a transition can never mint a record the lint would
immediately flag: the `PLAN` gate requires the plan sections, their proofs and
a well-formed place in the graph, the `REVIEW` gate requires a schema-clean
REVIEW.md, and closing as `DONE` requires all three gates earned plus a
schema-clean RETRO.md and DECISION.md. Two guards are the graph's alone: a task
another session has claimed cannot be entered into `WORKING`, and a task cannot
close while any of its children is not CLOSED. A refusal
names the same rule slug the lint would print:

```console
$ tatr flow 20260101-100000
ERROR: Refusing to advance 20260101-100000 from PLANNING: 2 precondition(s) not met
  - bad-record-schema: TASK.md has no '## Steps' section
  - bad-record-schema: TASK.md has no '## Definition of Done' section
  Record unchanged.
```

#### Historical exemptions

Records written before a rule existed are classified in `tasks/EXEMPTIONS.md`
rather than rewritten - the record trail is append-only history. One line per
suppressed finding:

```markdown
- 20260329-123700 bad-review-round: legacy REVIEW.md, single verdict, no rounds
```

An entry suppresses that rule for that task alone. Any rule can be exempted the
same way, because every finding routes through the same reporter. An entry that
never fires is itself a finding (`unused-exemption`) on a full scan, so the list
cannot rot. New work does not get exemptions: scaffold the record and it is
schema-clean from the first byte.

### Scaffolding Sibling Records

`tatr scaffold` writes a task's sibling records from the schema table `check`
validates against, so a scaffolded record passes the lint with its placeholders
still in place:

```bash
tatr scaffold 20260331-144635 REVIEW      # write REVIEW.md
tatr scaffold 20260331-144635 --list      # every kind, its path and presence
tatr scaffold 20260331-144635 RETRO -n    # print the path, write nothing
```

Kinds are `DECISION`, `REVIEW` and `RETRO`. `TASK.md` is created by
`tatr new` instead - it is typed metadata rather than a prose record.

There is no `--force`: scaffolding refuses to overwrite an existing record, and
an existing record is edited by hand, in the diff, where a reviewer sees it.

```console
$ tatr scaffold 20260730-153325 --list
TASK	/repo/tasks/20260730-153325/TASK.md	present
DECISION	/repo/tasks/20260730-153325/DECISION.md	present
REVIEW	/repo/tasks/20260730-153325/REVIEW.md	present
RETRO	/repo/tasks/20260730-153325/RETRO.md	present
```

**Options:**
- `-l, --list`: list every record kind with its path and `present`/`missing`
- `-n, --dry-run`: print the path and record kind that would be written

### Listing Definition of Done Proofs

`tatr proofs` prints each `## Definition of Done` proof as one
`<n><TAB><kind><TAB><text>` line. **tatr never executes anything**: a `cmd:`
proof's shell text round-trips verbatim, and running it is the caller's
decision, made in the caller's shell where the user can see the command.

```console
$ tatr proofs 20260730-154745
1	test	`test_record_scaffolds`
2	test	`test_check_record_schemas`
3	test	`test_check_review_approval_consistency`
4	test	`test_check_reciprocal_supersede`
5	test	`test_proof_listing_does_not_execute`
6	test	`test_existing_artifacts_are_classified`
7	cmd	`nix develop -c ./checker.sh && nix develop -c ./checker.sh --memcheck`

$ tatr proofs 20260730-154745 --kind cmd
1	cmd	`nix develop -c ./checker.sh && nix develop -c ./checker.sh --memcheck`
```

A proof may wrap across lines in the source bullet. A whitespace run collapses
to a single space only when it contains a byte that would break the record
format - a newline (the wrap) or a tab (the field separator). Everything else
is passed through byte for byte, so intra-line spacing a shell command may
depend on (`grep -q "a  b"`) survives, and every line is always exactly three
tab-separated fields.

**Options:**
- `-k, --kind <KIND>`: only list proofs of one kind (`test`, `cmd` or `manual`)

### Working a Container in Parallel

`tatr frontier <id>` prints the open work under a task, one row per child
and never a task body - the point is to decide what to pick up without loading
the whole map:

```console
$ tatr frontier 20260101-410000
READY	20260101-410005	p90	PLANNING+PLAN	Ready, high priority
READY	20260101-410004	p10	PLANNING+PLAN	Ready, low priority
BLOCKED	20260101-410003	p80	PLANNING+PLAN	Blocked on one of two	blocked-by=20260101-410005
CLAIMED	20260101-410002	p70	PLANNING+PLAN	Someone has it	claimed-by=agent-a
```

`READY` is the derived query the old `PLANNED` node used to stand for: the
`PLAN` gate earned, the cursor still below `WORKING`, every dependency CLOSED,
and nobody holding a claim. Anything else under the parent is open work you
cannot pick up, which is what `BLOCKED` means here; `blocked-by` names the
dependencies when dependencies are the reason, and is omitted when they are
not. The fourth column prints the row's activity and its gates, because
`PLANNING` alone no longer separates a plan being drafted from one that was
approved.

The order is deterministic: `READY` before `BLOCKED` before `CLAIMED`, then
priority descending, then ID ascending. A closed child is not open work and
does not appear; neither does a task that is not a child of this one. Any task
may have children, so any task may have a frontier; one with no children prints
nothing at all.

`tatr claim <id>` takes a task for the current session, `tatr release <id>`
gives it back, and `tatr claims` lists what is held:

```console
$ tatr claim 20260101-410002
Claimed 20260101-410002

$ tatr claims
claims in /repo/tasks/.claims
20260101-410002	/repo	alex@nixos	20260730-214809
```

A claim is a file created with `O_CREAT|O_EXCL` under `tasks/.claims/<id>`,
which the kernel makes atomic: of any number of racing sessions exactly one
wins and the rest are told who holds it. The winner writes its identity in the
same call that wins the race, so a claim is never anonymous. A `tatr flow`
that would enter `WORKING` refuses to advance a task another session holds -
though it still records the gate it earned on the way, and says so.

Two environment variables control who holds a claim and where claims live:

| Variable | Default | What it does |
| --- | --- | --- |
| `TATR_SESSION` | the working directory | The identity ownership is decided on. |
| `TATR_CLAIMS_DIR` | `<tasks dir>/.claims` | Where claims are read and written. |

`TATR_SESSION` exists because tatr is a one-shot CLI: the process that ran
`tatr claim` has exited by the time anything else runs, so ownership **cannot**
be a process id. It is whatever names the session across invocations. The
default - the working directory - distinguishes parallel sessions that each
work in their own tree; set it explicitly when a session moves around.

`TATR_CLAIMS_DIR` exists because separate worktrees have separate `tasks/`
trees, so a claim written in one would be invisible in the other. Point both
sessions at one claims directory and the guard works across trees, while each
session still edits its own checkout:

```bash
export TATR_CLAIMS_DIR=/path/to/main/checkout/tasks/.claims
export TATR_SESSION=agent-a

tatr claim 20260101-410002                 # taken in the shared claims dir
tatr flow 20260101-410002                  # edits THIS worktree's TASK.md
```

Another session with a different `TATR_SESSION` is then refused that start:

```console
$ TATR_SESSION=agent-b tatr flow 20260101-410002
ERROR: Not advancing 20260101-410002 to WORKING: 1 precondition(s) not met
  - the task is claimed by session 'agent-a' (alex@nixos, since 20260730-222012); set TATR_SESSION to that id if it is yours, or release it with `tatr release 20260101-410002 --force` if that session is gone
  Cursor held at PLANNING.
```

There is no timeout and nothing steals a claim automatically: "the owner is
slow" and "the owner is dead" are indistinguishable to tatr. A session releases
its own claim with no flag; recovering someone else's is deliberate:

```bash
tatr release 20260101-410002            # your own claim
tatr release 20260101-410002 --force    # a session that is gone
```

`tasks/.claims/` is machine-local state rather than versioned history, so it
belongs in `.gitignore` (this repository ignores it).

**Options:**
- `release -F, --force`: release a claim held by another session

### Listing a Phase's Context

`tatr context <id> --phase <phase>` prints the artifact paths one phase
needs, and nothing else - paths only, never contents:

```console
$ tatr context 20260101-410003 --phase review
/repo/tasks/20260101-410003/TASK.md	present
/repo/tasks/20260101-410003/REVIEW.md	missing

$ tatr context 20260101-410003 --phase understand
/repo/tasks/20260101-410003/TASK.md	present
/repo/tasks/20260101-410003/DECISION.md	missing
/repo/tasks/20260101-410000/TASK.md	present
```

Phases are `understand`, `plan`, `work`, `review`, `compound`, `resume` and
`landing` (default `resume`, which lists everything because it has no idea
where it is picking up from). `understand`, `plan` and `resume` also list the
`PARENT`'s `TASK.md`: a child task cannot be understood without the task that
gave it its shape.

A record the phase owns is listed whether or not it exists yet, because the
caller needs the path in order to create it - `review` names `REVIEW.md` before
there is one.

**Options:**
- `-P, --phase <PHASE>`: the phase (default: `resume`)

### Global Options

- `-r, --root <DIR>`: Change working directory before running commands

## Task File Format

Tasks are stored as Markdown files with structured metadata. Each task file (`TASK.md`) follows this format:

```markdown
# Task Title

- PRIORITY: 100
- TAGS: feature, enhancement
- ACTIVITY: -
- GATES: -
- RESOLUTION: -
- PARENT: 20260730-153122
- DEPENDS ON: 20260730-153325, 20260730-154745

Detailed description of the task goes here.
Can span multiple lines and include any markdown formatting.
```

The metadata is one flat block directly under the title, read in exactly this
order. The first five fields are required and always written back; `-` is how
each of the three nullable ones says it is unset, so the field set a record
carries never depends on the values in it. There is no `- KIND: ` line: every
record under `tasks/` is a task, and what kind of work it is belongs in the
title. `DUPLICATE OF`, `PARENT` and
`DEPENDS ON` are optional and appear only when set, so a task with no
relationships carries no empty relationship lines. There is no `- STATUS: `
line: STATUS is derived from `ACTIVITY` and `RESOLUTION` at every read.

Blank lines and leading whitespace between fields are tolerated when reading
and normalized away on the next write. Values are validated on the exact token
the parser consumes, so a trailing space or a CRLF tail fails the load rather
than silently defaulting, and a key written with no value (`- PARENT:`) is
reported as such rather than becoming body text.

Everything after the block is the description body: opaque, and preserved byte
for byte through any `edit`. A bullet is a bullet, even an uppercase one.

tatr never writes a record it cannot read back. `new` and `edit` re-parse the
serialized bytes before touching disk and fail without writing if the result
would not parse - a newline in a title or tag is the usual cause - and a failed
`new` leaves no task directory behind.

**Derived status values:** `OPEN` (no `ACTIVITY`), `IN_PROGRESS` (an
`ACTIVITY` and no `RESOLUTION`), `CLOSED` (a `RESOLUTION`). Reported by every
command, stored by none.

**Activity values:** `-`, `UNDERSTANDING`, `PLANNING`, `WORKING`, `REVIEWING`,
`COMPOUNDING`.

**Gate values:** `-`, or a space-separated subset of `PLAN REVIEW RETRO`,
always written in that order whatever order they were earned in.

**Resolution values:** `-`, `DONE`, `WONTDO` (abandoned, with a `- REASON: `),
`DUPLICATE` and `SUPERSEDED` (both with a `- DUPLICATE OF: <ID>`).
`SUPERSEDED` reuses the `- DUPLICATE OF: ` spelling deliberately: one pointer
field serves both resolutions, and `RESOLUTION` is what says which relation it
records. The spelling is frozen by v1.0.0.

`PARENT` and `DEPENDS ON` hold task IDs from the same `tasks/` tree, and are
validated as IDs only: whether the graph they form is acyclic and whether a
dependency is listed twice are not checked. The one place a reference is
resolved is the advance into `WORKING`, which requires every `DEPENDS ON` id
to name an existing task that is CLOSED. A parent in another repository is not expressible as a `PARENT` and
belongs in the body prose.

A record still carrying `- STATUS: `, `- FLOW STEP: ` or `- KIND: ` is a
legacy record: it is rejected with a diagnostic naming the file and the field
it stopped at, and `tatr migrate --apply` is the one path forward.

## Project Structure

```
your-project/
├── tasks/
│   ├── EXEMPTIONS.md          # optional: historical check exemptions
│   ├── 20260331-144635/
│   │   ├── TASK.md
│   │   ├── REVIEW.md          # optional sibling records, written by
│   │   ├── RETRO.md           # `tatr scaffold` and validated by `tatr check`
│   │   └── DECISION.md
│   ├── 20260330-202358/
│   │   └── TASK.md
│   └── 20260329-123700/
│       └── TASK.md
└── ...
```

The tool searches for a `tasks/` directory starting from your current directory and walking up the tree. This allows you to run `tatr` from any subdirectory of your project.

Only `TASK.md` is required. The sibling records are optional per task; which ones
a task owes depends on where it is in its lifecycle (see [Checking Task
Artifacts](#checking-task-artifacts)). `EXEMPTIONS.md` sits beside the task
directories, not inside one, because it classifies records across the backlog.

## Architecture

tatr is built with simplicity in mind:

- **Single-file implementation**: all logic lives in `tatr.c`
- **Header-only dependencies**: aids.h for utilities, argparse.h for CLI parsing
- **POSIX-compliant**: Uses standard filesystem and time APIs
- **No database**: All data stored as plain Markdown files

### Core Components

1. **Task Management**: Serialization/deserialization of tasks to/from Markdown
2. **HUID Generation**: Timestamp-based unique identifiers (YYYYMMDD-HHMMSS)
3. **Directory Search**: Recursive upward search for `tasks/` directory
4. **CLI Parser**: Subcommand-based interface with argument validation

## Building from Source

### Requirements

- C compiler (clang or gcc)
- POSIX-compliant system
- make (optional, for using Makefile)

### Dependencies

All dependencies are vendored as header-only libraries:

- **aids.h**: Utility library for data structures and I/O
- **argparse.h**: Command-line argument parsing

### Build Configuration

The Makefile provides several targets (run through the nix dev shell; see
[Using Make](#using-make) for the build guard and the `TATR_ALLOW_BARE_BUILD`
opt-out):

```bash
nix develop -c make          # Build the Linux binary at dist/tatr
nix develop -c make windows  # Build the Windows binary at dist/tatr.exe
nix develop -c make install  # Install to PREFIX (default: /usr/local)
nix develop -c make clean    # Remove build artifacts
```

## Testing

tatr includes a comprehensive test suite to ensure functionality and prevent
regressions. `checker.sh` rebuilds the binary with `make`, so run it through
the nix dev shell (or set `TATR_ALLOW_BARE_BUILD=1`):

```bash
# Run all tests
nix develop -c ./checker.sh

# Run with verbose output
nix develop -c ./checker.sh -v

# Run with memory leak checking (requires valgrind)
nix develop -c ./checker.sh --memcheck
```

The Windows artifact test is skipped when `x86_64-w64-mingw32-gcc` is not
available. Release builds install MinGW and always exercise `dist/tatr.exe`.

The test suite covers:
- Basic task creation and listing
- Task metadata (priority, tags, status)
- Sorting functionality
- Recursive directory search
- Error handling
- Memory leak detection

## Version Control

Tasks are designed to be version-controlled alongside your code:

```bash
# Add tasks to git
git add tasks/

# Track task changes
git log tasks/

# Search tasks
grep -r "TODO" tasks/
```

## Limitations

- No remote synchronization
- Maximum 256 arguments for CLI parsing

`tatr edit` covers non-interactive metadata and title changes; open the `TASK.md`
file directly to edit the free-form description body (`tatr new -b` seeds it at
creation time). Filtering by status and tags is available through `tatr ls -f`
(see Filtering under Listing Tasks).

## License

MIT License. Copyright 2026 Alexandru Jercan.

## Acknowledgments

Inspired by Tsoding's programming streams. Built as a learning project and
practical tool for personal task management.
