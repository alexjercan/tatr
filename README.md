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
- **Guarded lifecycle**: `tatr flow` is the only writer of the workflow fields, and every transition is checked before it is written
- **One record schema**: `tatr scaffold` writes the sibling records (SPIKE, DECISION, REVIEW, RETRO) from the same in-code table `tatr check` validates them against
- **Structured DoD proofs**: `tatr proofs` prints each Definition of Done proof as data - tatr never executes any of it
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
skill folders. The skill lives at `skills/tatr/` and teaches agents how to use
the CLI, task file format, `tatr check`, and the project workflow around
TASK.md, REVIEW.md, RETRO.md, and lessons ledgers.

Downstream flakes can consume the skill through the exported `skills.tatr`
flake output and install it into their agent skills directory.

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

# Create a Story under an Epic, blocked on another task
tatr new "Add the frontier view" -k STORY -P 20260730-153122 -d 20260730-153325

# Create a task with the description body from a file (or '-' for stdin)
tatr new "Refactor the loader" -p 60 -t refactor -b body.md

# Create task from a different directory
tatr -r /path/to/project new "Task title"
```

**Options:**
- `-p, --priority <value>`: Set task priority (default: 0, higher values = higher priority)
- `-t, --tags <value>...`: Comma-separated tags
- `-b, --body-file <path>`: Read the description body from a file; `-` reads stdin
- `-k, --kind <value>`: Set kind (TASK, EPIC, STORY, SPIKE; default: TASK)
- `-P, --parent <id>`: Set the parent task ID
- `-d, --depends-on <id>...`: Set the dependency task IDs

`edit` takes the same options and replaces the field it is given. On `edit`, an
empty value clears an optional relationship field: `tatr edit <id> -P ""` drops
the parent and `-d ""` drops every dependency.

A task is always born `- STATUS: OPEN`, `- FLOW STEP: BACKLOG`,
`- PLAN STATUS: DRAFT`. Those three workflow fields cannot be set here: they
belong to [`tatr flow`](#moving-a-task-through-the-flow), which is the only
command that writes them.

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
The fields are `:status`, `:priority`, `:title`, `:tags`, `:kind`,
`:flow_step`, `:plan_status`, `:parent` and `:depends`:

```bash
# Only open tasks
tatr ls -f '(:status eq OPEN)'

# Tasks tagged feature
tatr ls -f ':tags contains feature'

# Open feature tasks, combining conditions
tatr ls -f '(:status eq OPEN) and (:tags contains feature)'

# Every Epic container
tatr ls -f ':kind eq EPIC'

# Approved work that has not been picked up yet
tatr ls -f '(:plan_status eq APPROVED) and (:flow_step in [BACKLOG, PLANNED])'

# The children of one Epic, and everything blocked on one task
tatr ls -f ':parent eq 20260730-153122'
tatr ls -f ':depends contains 20260730-153325'
```

The enum-valued fields (`:status`, `:kind`, `:flow_step`, `:plan_status`) take
`eq` and `in`; `:parent` takes `eq`; `:tags` and `:depends` take `contains`.

Supported operators include `eq`, `contains`, `in` (with `[...]` lists), and the
boolean connectives `and`, `or`, `not`. Literal values may contain `.` and `-`
(after an initial letter, digit or `_`), so version-style tags such as `v0.1.0`
or `release-candidate` can be filtered on. Filtering composes with sorting and
recursive mode, and applies per section in recursive mode.

**Output format:**
```
tasks/20260331-144635/TASK.md: [PRIORITY: 100, KIND: TASK, FLOW STEP: DONE, TAGS: feature] Implement filter system
tasks/20260330-202358/TASK.md: [PRIORITY: 80, KIND: STORY, FLOW STEP: WORKING, TAGS: testing, bug] Add unit tests
tasks/20260329-123700/TASK.md: [PRIORITY: 0, KIND: TASK, FLOW STEP: BACKLOG, TAGS: ] Fix memory leak in parser
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

# Re-home a Story and replace its dependencies
tatr edit 20260331-144635 -k STORY -P 20260730-153122 -d 20260730-153325
```

**Options:**
- `-T, --title <value>`: New task title
- `-p, --priority <value>`: New priority (non-negative integer)
- `-t, --tags <value>...`: New tags, replacing the existing set
- `-k, --kind <value>`: New kind (TASK, EPIC, STORY, SPIKE)
- `-P, --parent <id>`: New parent task ID (empty value clears it)
- `-d, --depends-on <id>...`: New dependency task IDs (empty value clears them)

`edit` cannot set `STATUS`, `FLOW STEP` or `PLAN STATUS`. The flags for them
were removed, and the retired spellings fail with a pointer to the lifecycle
rather than a generic unknown-argument error:

```console
$ tatr edit 20260730-185007 --status CLOSED
ERROR: '--status' was removed: STATUS is not settable through `new` or `edit`
  STATUS is derived from FLOW STEP; move the task with `tatr flow <ID> [--to <STEP>]`
```

(The transcripts here elide the `tatr.c:<line>:` source location every
`ERROR:` line carries, which moves with the code; everything after it is
verbatim.)

An invalid value for any option `edit` does accept is rejected before anything
is written, so the task file is left unchanged.

### Moving a Task Through the Flow

`tatr flow` is the only writer of the three workflow fields. Without `--to` it
advances one step along the chain; with `--to` it names the target explicitly:

```bash
tatr flow 20260331-144635              # advance one step
tatr flow 20260331-144635 --to WORKING # name the target (the review fix loop)
```

**The transition table.** Eight edges, and nothing else:

```
BACKLOG -> UNDERSTANDING -> PLANNING -> PLANNED -> WORKING -> REVIEWING
                                          ^                      |
                                          |                      v
                                          +------- (fix) --- COMPOUNDING -> DONE
```

`REVIEWING` is the only step with two successors. Its default - what a bare
`tatr flow` picks - is `COMPOUNDING`, so the fix loop back to `WORKING` must be
asked for by name with `--to WORKING`. Every other step has exactly one
successor, and `DONE` is terminal.

**STATUS is derived, never chosen.** The flow step implies it, so a work task
stays IN_PROGRESS through review and compound and closes atomically at DONE, in
the same write that sets `- FLOW STEP: DONE`:

| FLOW STEP                                 | STATUS      |
| ----------------------------------------- | ----------- |
| BACKLOG, UNDERSTANDING, PLANNING, PLANNED | OPEN        |
| WORKING, REVIEWING, COMPOUNDING           | IN_PROGRESS |
| DONE                                      | CLOSED      |

**Preconditions.** Three edges are gates:

- `PLANNING -> PLANNED` is the plan gate. It has no preconditions of its own;
  its effect is to write `- PLAN STATUS: APPROVED`, and it is the only way that
  value is ever written.
- `PLANNED -> WORKING` requires `- PLAN STATUS: APPROVED` and every
  `- DEPENDS ON` id to resolve to an existing task that is CLOSED.
- `REVIEWING -> COMPOUNDING` requires a `REVIEW.md` whose latest `- VERDICT:`
  is APPROVE, with no unticked BLOCKER or MAJOR finding left.
- `COMPOUNDING -> DONE` requires all of the above, plus zero unchecked items
  under `## Steps`, a `RETRO.md`, and a valid `- STATUS:` on a `DECISION.md`
  when the task carries one.

`REVIEWING -> WORKING` and the three pre-plan edges carry no preconditions:
going back to fix something is never harder than going forward.

`KIND: EPIC` containers are exempt from exactly what `tatr check` already
exempts them from - the plan-approval, review, retro and unchecked-Steps
requirements - because an Epic's record lives in its children. Their
dependencies and their `DECISION.md` are checked like anyone else's.

**Failure is atomic.** Every precondition is evaluated before anything is
mutated, all unmet ones are reported rather than one per round trip, and a
refused transition leaves `TASK.md` byte-identical:

```console
$ tatr flow 20260730-185007
Task 20260730-185007 moved BACKLOG -> UNDERSTANDING (STATUS: OPEN)
$ tatr flow 20260730-185007
Task 20260730-185007 moved UNDERSTANDING -> PLANNING (STATUS: OPEN)
$ tatr flow 20260730-185007
Task 20260730-185007 moved PLANNING -> PLANNED (STATUS: OPEN)
$ tatr flow 20260730-185007 --to DONE
ERROR: Illegal transition for 20260730-185007: PLANNED -> DONE
  the legal move from PLANNED is WORKING
$ tatr flow 20260730-185007
Task 20260730-185007 moved PLANNED -> WORKING (STATUS: IN_PROGRESS)
$ tatr flow 20260730-185007
Task 20260730-185007 moved WORKING -> REVIEWING (STATUS: IN_PROGRESS)
$ tatr flow 20260730-185007
ERROR: Refusing to move 20260730-185007 from REVIEWING to COMPOUNDING: 1 precondition(s) not met
  - there is no REVIEW.md: the work has not been reviewed
$ printf -- '- VERDICT: APPROVE\n' > tasks/20260730-185007/REVIEW.md
$ tatr flow 20260730-185007
Task 20260730-185007 moved REVIEWING -> COMPOUNDING (STATUS: IN_PROGRESS)
$ tatr flow 20260730-185007
ERROR: Refusing to move 20260730-185007 from COMPOUNDING to DONE: 2 precondition(s) not met
  - 1 unchecked Steps item(s) remain
  - there is no RETRO.md: the retro has not been written
```

**Options:**
- `-o, --to <STEP>`: the target flow step (default: the current step's successor)

There is no `--force` and no repair command. A transition may never produce a
state `tatr check` would flag: both read the same artifacts through the same
helpers. A record that is already in the wrong state is corrected by hand in
`TASK.md`, where the fix shows up in the diff and a reviewer sees it.
`- PLAN STATUS: NOT_REQUIRED` is likewise unreachable through the CLI - it is
how a record says its cycle predated plan state, and it is written by hand.

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
tatr check --ledger LESSONS.md   # also lint a lessons ledger
```

**Default rules:**
- `bad-record-schema`: a record does not match its schema - the wrong title
  prefix, a missing or empty required `- KEY:` header field, or a missing or
  empty required `## ` section. Applies to `TASK.md` (kind-specific sections,
  see below), `SPIKE.md`, `DECISION.md`, `REVIEW.md` and `RETRO.md`.
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
- `missing-spike-record`: a planned `KIND: SPIKE` task has no `SPIKE.md`.
- `bad-spike-status`: a `SPIKE.md` `- STATUS:` value outside
  RECOMMENDED|INCONCLUSIVE|DROPPED.
- `dangling-seeded-task`: a task ID under a `SPIKE.md`'s `## Next steps`
  section has no `TASK.md`.
- `dangling-decision-task`: a `DECISION.md`'s `- TASK:` pointer is not a task
  ID, or names a task that does not exist.
- `nonreciprocal-supersede`: a supersede link resolves in only one direction -
  A says `SUPERSEDED by B` but B carries no `- Supersedes: A` line, or the
  reverse.
- `unused-exemption`: an entry in `tasks/EXEMPTIONS.md` never fired on a full
  scan (reported against the task it names).
- `closed-unchecked`: a CLOSED task still has unchecked `- [ ]` items under
  its `## Steps` section (other sections may keep open boxes).
- `closed-missing-review`: a CLOSED task has no `REVIEW.md`.
- `closed-missing-retro`: a CLOSED task has no `RETRO.md`.
- `closed-not-approved`: a CLOSED task's REVIEW.md exists but its latest
  `- VERDICT:` line is not APPROVE (or there is no verdict at all).
- `bad-severity`: a REVIEW.md finding uses a severity outside
  BLOCKER|MAJOR|MINOR|NIT.
- `malformed-header`: TASK.md is missing/unreadable, or its title and metadata
  block do not parse. This covers every invalid metadata value: the parser
  validates the exact token it consumes, so `- STATUS: DONE`,
  `- KIND: EPICS`, a trailing space after a value or a CRLF tail all land
  here rather than in a rule of their own.
- `unplanned-in-progress`: an ordinary IN_PROGRESS task lacks
  `- PLAN STATUS: APPROVED`. `KIND: EPIC` containers are exempt.
- `bad-decision-status`: a task's `DECISION.md` (when present) has a `- STATUS:`
  value that is not `ACCEPTED` nor `SUPERSEDED by <ref>`, or has no STATUS line.
- `dangling-supersede`: a `DECISION.md` supersede reference - in a
  `SUPERSEDED by <ref>` status or a `- Supersedes: <ref>` line - does not
  resolve to an existing `tasks/<id>/DECISION.md`.

The approved-plan requirement applies only when an ordinary task is moved to
IN_PROGRESS, so CLOSED tasks and OPEN backlog items are never asked for one.
`KIND: EPIC` marks an explicit /flow epic, sprint, version, release, or
multi-feature container. The container's broader record lives in that task's
own `TASK.md` sections, such as `## Epic`, `## Done Means`, `## Child Tasks`,
`## Decisions`, and `## Manual Acceptance`; child tasks carry the per-task
review and retro records. Do not create a container task for one requested
thing. Containers are exempt from the record-completeness rules
(`closed-missing-review`, `closed-missing-retro`), from `closed-unchecked`
(a frozen container's step boxes stay verbatim, since superseded or dropped
children are honest history), and from `unplanned-in-progress` (the plan gate
applies to the work tasks underneath it).

The `DECISION.md` and `SPIKE.md` rules are presence-gated: a task without such
a sibling is never flagged for its contents, so they need no migration of
existing tasks.

`## Steps` and `## Definition of Done` are the plan gate's output, so
`bad-record-schema` asks for them only from `- FLOW STEP: PLANNED` on - a task
`tatr new` just created is not a finding the moment it exists. The required
sections are kind-specific: `TASK`/`STORY` owe `## Steps` and
`## Definition of Done`, `EPIC` owes `## Done Means` and `## Child Tasks`, and
`SPIKE` owes `## Question` plus a `SPIKE.md` sibling.

The same rules gate the lifecycle. `tatr flow` reads them through the same
functions `check` does, so a transition can never mint a record the lint would
immediately flag: `PLANNING -> PLANNED` requires the plan sections and their
proofs, `REVIEWING -> COMPOUNDING` requires a schema-clean REVIEW.md, and
`COMPOUNDING -> DONE` requires all of that plus a schema-clean RETRO.md and
DECISION.md. A refusal names the same rule slug the lint would print:

```console
$ tatr flow 20260101-100000 --to PLANNED
ERROR: Refusing to move 20260101-100000 from PLANNING to PLANNED: 2 precondition(s) not met
  - bad-record-schema: TASK.md has no '## Steps' section
  - bad-record-schema: TASK.md has no '## Definition of Done' section
```

#### Historical exemptions

Records written before a rule existed are classified in `tasks/EXEMPTIONS.md`
rather than rewritten - the flow trail is append-only history. One line per
suppressed finding:

```markdown
- 20260329-123700 bad-review-round: pre-flow REVIEW.md, single verdict, no rounds
```

An entry suppresses that rule for that task alone. Any rule can be exempted the
same way, because every finding routes through the same reporter. An entry that
never fires is itself a finding (`unused-exemption`) on a full scan, so the list
cannot rot. New work does not get exemptions: scaffold the record and it is
schema-clean from the first byte.

**Options:**
- `-L, --ledger <FILE>`: add `promotion-stalled` for a ledger lesson at three
  or more occurrences (`(x3)` and up) outside the `## Pending promotions`
  section (path relative to the root)

### Scaffolding Sibling Records

`tatr scaffold` writes a task's sibling records from the schema table `check`
validates against, so a scaffolded record passes the lint with its placeholders
still in place:

```bash
tatr scaffold 20260331-144635 REVIEW      # write REVIEW.md
tatr scaffold 20260331-144635 --list      # every kind, its path and presence
tatr scaffold 20260331-144635 RETRO -n    # print the path, write nothing
```

Kinds are `SPIKE`, `DECISION`, `REVIEW` and `RETRO`. `TASK.md` is created by
`tatr new` instead - it is typed metadata rather than a prose record.

There is no `--force`: scaffolding refuses to overwrite an existing record, and
an existing record is edited by hand, in the diff, where a reviewer sees it.

```console
$ tatr scaffold 20260730-153325 --list
TASK	/repo/tasks/20260730-153325/TASK.md	present
SPIKE	/repo/tasks/20260730-153325/SPIKE.md	missing
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

### Global Options

- `-r, --root <DIR>`: Change working directory before running commands

## Task File Format

Tasks are stored as Markdown files with structured metadata. Each task file (`TASK.md`) follows this format:

```markdown
# Task Title

- STATUS: OPEN
- PRIORITY: 100
- TAGS: feature, enhancement
- KIND: TASK
- FLOW STEP: BACKLOG
- PLAN STATUS: DRAFT
- PARENT: 20260730-153122
- DEPENDS ON: 20260730-153325, 20260730-154745

Detailed description of the task goes here.
Can span multiple lines and include any markdown formatting.
```

The metadata is one flat block directly under the title, read in exactly this
order. The first six fields are required and always written back. `PARENT` and
`DEPENDS ON` are optional and appear only when set, so a task with no
relationships carries no empty relationship lines.

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

**Status values:**
- `OPEN`: Task not yet started
- `IN_PROGRESS`: Currently being worked on
- `CLOSED`: Task completed

**Kind values:** `TASK` (the default), `EPIC` (an explicit /flow container),
`STORY` (a unit of work under an Epic), `SPIKE` (an exploration).

**Flow step values:** `BACKLOG`, `UNDERSTANDING`, `PLANNING`, `PLANNED`,
`WORKING`, `REVIEWING`, `COMPOUNDING`, `DONE`.

**Plan status values:** `DRAFT`, `APPROVED` (the user accepted the plan at the
/flow gate), `NOT_REQUIRED` (the record's cycle never carried plan state, as
with pre-flow history).

`PARENT` and `DEPENDS ON` hold task IDs from the same `tasks/` tree, and are
validated as IDs only: whether the graph they form is acyclic and whether a
dependency is listed twice are not checked. The one place a reference is
resolved is the `PLANNED -> WORKING` gate, which requires every `DEPENDS ON` id
to name an existing task that is CLOSED. A parent in another repository is not expressible as a `PARENT` and
belongs in the body prose.

There is no migration path from the pre-v2 format and no compatibility mode.
A record missing these fields is rejected with a diagnostic naming the file and
the field it stopped at; correct such a record by hand.

## Project Structure

```
your-project/
├── tasks/
│   ├── EXEMPTIONS.md          # optional: historical check exemptions
│   ├── 20260331-144635/
│   │   ├── TASK.md
│   │   ├── REVIEW.md          # optional sibling records, written by
│   │   ├── RETRO.md           # `tatr scaffold` and validated by `tatr check`
│   │   ├── DECISION.md
│   │   └── SPIKE.md
│   ├── 20260330-202358/
│   │   └── TASK.md
│   └── 20260329-123700/
│       └── TASK.md
└── ...
```

The tool searches for a `tasks/` directory starting from your current directory and walking up the tree. This allows you to run `tatr` from any subdirectory of your project.

Only `TASK.md` is required. The sibling records are optional per task; which ones
a task owes depends on where it is in the flow (see [Checking Task
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
