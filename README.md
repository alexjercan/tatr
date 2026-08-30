# tatr

Small offline task tracker. One C file. Markdown storage. No task lifecycle
beyond status.

## Build and test

```bash
nix develop -c make
nix develop -c ./checker.sh
```

The binary is `dist/tatr`.

## Task format

Each task is `tasks/YYYYMMDD-HHMMSS/TASK.md`:

```markdown
# Fix parser

- STATUS: OPEN
- PRIORITY: 100
- TAGS: bug, parser

Optional Markdown body.
```

Valid statuses: `OPEN`, `CLOSED`.

No other task records or metadata exist. Invalid task files make `tatr ls` and
`tatr edit` fail.

## Commands

```bash
tatr new "Fix parser" --priority 100 --tags bug --tags parser --body details.md
printf 'Body from stdin.\n' | tatr new "Fix parser" --body -
tatr ls
tatr ls --sort priority
tatr ls --filter ':status eq OPEN'
tatr edit 20260808-113322 --status CLOSED --priority 80
tatr help
tatr version
```

Global option: `-r DIR` or `--root DIR` selects a project directory.

### new

Creates a task. Options:

- `-s, --status`: default `OPEN`
- `-p, --priority`: default `0`; higher values sort first
- `-t, --tags`: repeat for multiple tags
- `-b, --body`: read the Markdown body from a file, or from stdin with `-`

The body is stored after the generated metadata without modification. Input
must be readable before tatr creates the task directory.

```bash
tatr new "Fix parser" -b details.md
tatr new "Fix parser" -b - < details.md
```

### ls

Loads and validates each selected `TASK.md`. Options:

- `-s, --sort`: `created`, `priority`, or `title`
- `-f, --filter`: query expression
- `-R, --recursive`: find nested projects

Query fields: `:status`, `:priority`, `:tags`, `:title`. Operators: `eq`, `in`,
`contains`, `and`, `or`, `not`. Bare literals start with a letter, digit, or
`_`. Later characters can also be `.` or `-`, so tags such as `v0.1.0` and
`release-candidate` work. Bare literals cannot start with `.` or `-`. Examples:

```bash
tatr ls -f ':tags contains bug'
tatr ls -f '(:status eq OPEN) and (:priority eq 100)'
tatr ls -f ':status in [OPEN, CLOSED]'
```

### edit

Updates one task by ID. Supports `--title`, `--status`, `--priority`, and
`--tags`. Unspecified fields and the body stay unchanged.

## Storage lookup

Without `--root`, tatr searches from the current directory toward `/` for a
`tasks/` directory. Data stays in plain Markdown suitable for Git and grep.
