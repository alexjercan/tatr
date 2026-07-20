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
- **Full CRUD**: Create, show, edit, and remove tasks entirely from the CLI
- **Flexible listing**: Sort by creation date, priority, or title, and filter with a query language
- **Automation-friendly**: Non-interactive commands make it easy for scripts and agents to drive
- **Terminal integration**: Clickable file paths in OSC 8-compliant terminals
- **Zero configuration**: Works out of the box with no setup required

## Installation

### Using Make

```bash
# Build the binary
make

# Install to /usr/local/bin (may require sudo)
make install

# Install to a custom location
make install PREFIX=$HOME/.local
```

### Using Nix Flakes

```bash
# Build with nix
nix build

# Enter development environment
nix develop
```

### Manual Build

```bash
clang -Wall -Wextra -O2 -g -o tatr tatr.c
```

## Usage

### Creating Tasks

```bash
# Create a basic task
tatr new "Fix memory leak in parser"

# Create a task with metadata
tatr new "Add unit tests" -p 80 -t testing,bug -s IN_PROGRESS

# Create a task with the description body from a file (or '-' for stdin)
tatr new "Refactor the loader" -p 60 -t refactor -b body.md

# Create task from a different directory
tatr -r /path/to/project new "Task title"
```

**Options:**
- `-p, --priority <value>`: Set task priority (default: 0, higher values = higher priority)
- `-t, --tags <value>...`: Comma-separated tags
- `-s, --status <value>`: Set status (OPEN, IN_PROGRESS, CLOSED)
- `-b, --body-file <path>`: Read the description body from a file; `-` reads stdin

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

**Filtering:**

The `-f` flag takes a small query language over task fields. Fields are written
with a leading colon (`:status`, `:priority`, `:tags`), combined with operators
and grouped with parentheses:

```bash
# Only open tasks
tatr ls -f '(:status eq OPEN)'

# Tasks tagged feature
tatr ls -f ':tags contains feature'

# Open feature tasks, combining conditions
tatr ls -f '(:status eq OPEN) and (:tags contains feature)'
```

Supported operators include `eq`, `contains`, `in` (with `[...]` lists), and the
boolean connectives `and`, `or`, `not`. Literal values may contain `.` and `-`
(after an initial letter, digit or `_`), so version-style tags such as `v0.1.0`
or `release-candidate` can be filtered on. Filtering composes with sorting and
recursive mode, and applies per section in recursive mode.

**Output format:**
```
tasks/20260331-144635/TASK.md: [PRIORITY: 100, TAGS: feature] Implement filter system
tasks/20260330-202358/TASK.md: [PRIORITY: 80, TAGS: testing,bug] Add unit tests
tasks/20260329-123700/TASK.md: [PRIORITY: 0, TAGS: ] Fix memory leak in parser
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

Update the metadata or title of an existing task without opening an editor. Only
the fields you pass are changed; everything else, including the description body,
is left untouched. This is the command automation and agents use to move a task
through its lifecycle (for example OPEN -> IN_PROGRESS -> CLOSED):

```bash
# Move a task to IN_PROGRESS
tatr edit 20260331-144635 -s IN_PROGRESS

# Bump priority and retitle
tatr edit 20260331-144635 -p 90 -T "Implement query filter language"

# Replace the tag set (edit replaces tags, it does not merge them)
tatr edit 20260331-144635 -t feature -t parser
```

**Options:**
- `-T, --title <value>`: New task title
- `-p, --priority <value>`: New priority (non-negative integer)
- `-t, --tags <value>...`: New tags, replacing the existing set
- `-s, --status <value>`: New status (OPEN, IN_PROGRESS, CLOSED)

An invalid status value is rejected and the task file is left unchanged.

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
tatr check --strict           # also require REVIEW.md/RETRO.md on CLOSED tasks
tatr check --ledger LESSONS.md   # also lint a lessons ledger
```

**Default rules:**
- `closed-unchecked`: a CLOSED task still has unchecked `- [ ]` items under
  its `## Steps` section (other sections may keep open boxes).
- `closed-not-approved`: a CLOSED task's REVIEW.md exists but its latest
  `- VERDICT:` line is not APPROVE (or there is no verdict at all).
- `bad-severity`: a REVIEW.md finding uses a severity outside
  BLOCKER|MAJOR|MINOR|NIT.
- `malformed-header`: TASK.md is missing/unreadable, its header does not
  parse, or its STATUS value is not OPEN/IN_PROGRESS/CLOSED.

**Options:**
- `-S, --strict`: add `closed-missing-review` and `closed-missing-retro` for
  CLOSED tasks lacking those files
- `-L, --ledger <FILE>`: add `promotion-stalled` for a ledger lesson at three
  or more occurrences (`(x3)` and up) outside the `## Pending promotions`
  section (path relative to the root)

### Global Options

- `-r, --root <DIR>`: Change working directory before running commands

## Task File Format

Tasks are stored as Markdown files with structured metadata. Each task file (`TASK.md`) follows this format:

```markdown
# Task Title

- STATUS: OPEN
- PRIORITY: 100
- TAGS: feature, enhancement

Detailed description of the task goes here.
Can span multiple lines and include any markdown formatting.
```

**Status values:**
- `OPEN`: Task not yet started
- `IN_PROGRESS`: Currently being worked on
- `CLOSED`: Task completed

## Project Structure

```
your-project/
├── tasks/
│   ├── 20260331-144635/
│   │   └── TASK.md
│   ├── 20260330-202358/
│   │   └── TASK.md
│   └── 20260329-123700/
│       └── TASK.md
└── ...
```

The tool searches for a `tasks/` directory starting from your current directory and walking up the tree. This allows you to run `tatr` from any subdirectory of your project.

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

The Makefile provides several targets:

```bash
make          # Build the binary
make install  # Install to PREFIX (default: /usr/local)
make clean    # Remove build artifacts
```

## Testing

tatr includes a comprehensive test suite to ensure functionality and prevent regressions:

```bash
# Run all tests
./checker.sh

# Run with verbose output
./checker.sh -v

# Run with memory leak checking (requires valgrind)
./checker.sh --memcheck
```

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

- No task dependencies or relationships
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
