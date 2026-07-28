# AGENTS.md

Guidance for AI agents (and humans) working in the `tatr` repository.

## What tatr is

tatr (Task Tracker) is a small command-line tool, written in C, that stores
tasks as Markdown files under a `tasks/` directory so they version-control
alongside the code. Each task is a directory named with a timestamp HUID
(`YYYYMMDD-HHMMSS`) containing a `TASK.md`. It is a deliberately minimal,
offline, zero-config alternative to an issue tracker.

## Layout

```
tatr.c        # the entire program (single translation unit)
aids.h        # vendored header-only utility library (strings, arrays, IO, logging)
argparse.h    # vendored header-only CLI argument parser
Makefile      # build + install
checker.sh    # the test suite (integration tests that drive the built binary)
flake.nix     # nix dev shell (provides clang, valgrind, etc.)
skills/tatr/  # Codex/Claude skill exported as `skills.tatr` from the flake
README.md     # user-facing documentation
LESSONS.md    # lessons ledger (read before starting any task)
tasks/        # tatr's own task backlog (tatr dogfoods itself)
```

There is no separate build system beyond the Makefile; `tatr.c` includes the two
vendored headers and defines their implementations at the bottom of the file
(`ARGPARSE_IMPLEMENTATION` / `AIDS_IMPLEMENTATION`).

## Building

The canonical toolchain (clang, valgrind) comes from the nix dev shell, so all
builds run through it:

```bash
nix develop -c make            # builds dist/tatr with clang -Wall -Wextra -O2 -g
nix develop -c make windows    # builds dist/tatr.exe with MinGW
nix develop -c make clean
nix develop -c make install PREFIX=$HOME/.local
```

A Makefile build guard enforces this: a bare `make` (outside `nix develop`,
outside a nix build sandbox) fails with a pointer to `nix develop -c make`
rather than silently building against whatever gcc the login shell happens to
have. The guard passes automatically inside `nix develop` (`IN_NIX_SHELL`) and
inside a nix build sandbox such as `nix flake check` (`NIX_BUILD_TOP`).

If you must build outside nix because the toolchain is provisioned another way
(CI installs clang/gcc + valgrind via apt, or MinGW for `make windows`), set
`TATR_ALLOW_BARE_BUILD=1` to opt out of the guard. The code is portable POSIX C
and compiles cleanly under clang, gcc (`make CC=gcc`), and MinGW
(`make windows`); keep it warning-clean under `-Wall -Wextra`.

## Testing

`checker.sh` is the test suite. It rebuilds the binary and runs integration
tests that invoke `dist/tatr` against throwaway `tasks/` directories. Always run
it after a change:

```bash
nix develop -c ./checker.sh            # build + run all tests
nix develop -c ./checker.sh -v         # verbose (shows failure details)
nix develop -c ./checker.sh --memcheck  # run every test under valgrind (no leaks allowed)
```

The Windows artifact test in `checker.sh` skips when `x86_64-w64-mingw32-gcc`
is not on PATH. Release CI installs MinGW and therefore exercises that test.

Prefer integration tests here over unit tests: add a `test_*` function next to
the related ones and register it in the run list near the bottom of the file.

### checker.sh gotcha

The script runs under `set -e`. A bare command that exits non-zero aborts the
whole run, and `local out=$(cmd)` swallows the command's exit code (you get
`local`'s status, which is always 0). To assert that a command fails, use:

```sh
set +e
local output
output=$(run_tatr <args> 2>&1)
local exit_code=$?
set -e
```

To capture a generated task ID in a test, use the `new_task_id` helper.

## Code conventions

- Keep it a single file. New commands go in `tatr.c`; do not split into multiple
  translation units or add build complexity.
- Follow the existing command shape: each subcommand is a `static int main_<cmd>`
  that builds its own `Argparse_Parser`, parses `ctx->argc/argv`, does its work,
  and cleans up under a `defer:` label. Wire it into the dispatch chain and the
  `tatr_print_help` list in `main`.
- Reuse the shared spine rather than reimplementing it:
  - `task_resolve(cwd, huid, &tasks_dir, &task_file_path)` validates a HUID,
    locates the `tasks/` dir (upward search, honoring `-r ROOT`), checks the
    task exists, and hands back owned paths. `show`, `edit` and `rm` all use it.
  - `task_load` / `task_save` and `task_serialize` / `task_deserialize` are the
    only path to and from `TASK.md`. Editing a task means load, mutate fields,
    save; this preserves the description body automatically.
- Memory: match the ownership discipline in the existing code and free
  everything in `defer:`. Every change must pass `--memcheck` with zero leaks.
- Anything that deletes on disk must be gated behind a validated HUID and only
  ever touch `tasks/<id>/`. Never build a destructive path from raw user input.
- Error out non-zero with a clear `aids_log(AIDS_ERROR, ...)` message; do not
  half-apply a change (validate before writing).

## Working with the backlog

tatr tracks its own work in `tasks/`. Use the tool itself:

```bash
tatr ls -s priority          # see the backlog
tatr show <ID>               # read a task's full description and steps
tatr edit <ID> -s IN_PROGRESS  # claim a task
tatr edit <ID> -s CLOSED     # finish it
tatr new "..." -p 80 -t feature  # add newly discovered work
tatr new "..." -b body.md    # seed the description body from a file ('-' = stdin)
tatr check                   # lint the backlog for process drift (exit 1 on findings)
```

Per-task records live in the task's own folder: `RETRO.md` (and `REVIEW.md`)
next to its `TASK.md`, per the flow skills. The pre-flow `docs/retros/` were
distilled into the ledger and removed (git history keeps them). Durable
lessons go to `LESSONS.md` at the root; there is no scratch drawer.

`tatr check` is always strict: there is no `--strict` flag to opt in or out. It
requires a `REVIEW.md` and `RETRO.md` on every CLOSED task, EXCEPT tasks tagged
`goal`: a `goal` task is an explicit /flow epic, sprint, version, release, or
multi-feature container. The container's broader done definition, child-task
list, decisions index, and manual acceptance live in the container's own
`TASK.md`; child tasks carry the per-task review and retro records. Containers
are also exempt from the `closed-unchecked` rule - a frozen container's step
boxes stay verbatim (superseded / dropped / premise-falsified steps are honest
history) rather than being ticked to silence the lint.

Flow-state rules protect planned work from checklist-shaped drift.
`bad-flow-state` validates exact marker values under `## Flow State`:
`- FLOW STEP: UNDERSTANDING|PLANNING|PLANNED|WORKING|REVIEWING|COMPOUNDING|DONE`
and `- PLAN STATUS: APPROVED`. `unplanned-in-progress` fires when an ordinary
IN_PROGRESS task lacks `PLAN STATUS: APPROVED`. Explicit containers tagged
`goal` are exempt; OPEN backlog and CLOSED tasks do not need the approved-plan
marker.

The `DECISION.md` rules (`bad-decision-status`, `dangling-supersede`) need no
such exemption: they are presence-gated, firing only when a task carries a
`DECISION.md`.

## Development flow

/flow drives development here: work is planned into tatr tasks, implemented in
sprout worktrees, reviewed out-of-context in round 1, and closed with DoD
proofs in test:/cmd:/manual: notation. `LESSONS.md` at the repo root is the
lessons ledger - read it before starting any task. `tatr check` (plus
`tatr check --ledger LESSONS.md`) is the conformance gate; keep both clean.

## Commits

- Plain commit messages, no AI attribution or co-author trailers.
- Use plain ASCII punctuation only: `-`, `--`, `...`, `->`, straight quotes. No
  em dashes, smart quotes, ellipsis characters, or arrows, in code, comments,
  docs, or commit messages.
