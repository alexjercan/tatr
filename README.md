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
- **Flexible listing**: Sort tasks by creation date, priority, or title
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

# Create task from a different directory
tatr -r /path/to/project new "Task title"
```

**Options:**
- `-p, --priority <value>`: Set task priority (default: 0, higher values = higher priority)
- `-t, --tags <value>...`: Comma-separated tags
- `-s, --status <value>`: Set status (OPEN, IN_PROGRESS, CLOSED)

### Listing Tasks

```bash
# List all tasks (sorted by creation date)
tatr ls

# Sort by priority (highest first)
tatr ls -s priority

# Sort by title (alphabetically)
tatr ls -s title
```

**Output format:**
```
tasks/20260331-144635/TASK.md: [PRIORITY: 100, TAGS: feature] Implement filter system
tasks/20260330-202358/TASK.md: [PRIORITY: 80, TAGS: testing,bug] Add unit tests
tasks/20260329-123700/TASK.md: [PRIORITY: 0, TAGS: ] Fix memory leak in parser
```

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

- **Single-file implementation**: ~930 lines of C code
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

- No built-in task editing command (use your text editor)
- No filtering by status or tags yet (use grep or ls filters)
- No task dependencies or relationships
- No remote synchronization
- Maximum 256 arguments for CLI parsing

## License

MIT License. Copyright 2026 Alexandru Jercan.

## Acknowledgments

Inspired by Tsoding's programming streams. Built as a learning project and
practical tool for personal task management.
