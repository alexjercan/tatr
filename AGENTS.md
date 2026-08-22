# AGENTS.md

Global `~/AGENTS.md` applies.

## Project

- Offline C CLI for Markdown tasks under `tasks/`.
- Production code: `tatr.c`. Vendored code: `aids.h` and `argparse.h`.
- `README.md` defines user behavior. `RELEASE.md` defines publishing.

## Agent workflow

- Work directly on `master` unless the user requests an isolated worktree.
- Use this repository's tatr tracker for tracked work. Create a task only when
  the user requests one.
- Use one task for one user request and its follow-up work. Create dependent
  tasks only when the user requests decomposition.
- Store task records under `tasks/` and link related task IDs in the body.
- Treat `checker.sh` as the executable behavior catalog. Keep task-specific
  evidence with the task.
- Work offline from local sources.

## Conventions

### C

- Keep production C in `tatr.c`. Avoid compiler-specific extensions.
- Commands use `static int main_<cmd>`, a local parser, and `defer:` cleanup.
- Reuse task resolution, load, save, serialization, and parsing helpers.
- Validate all input before mutation. Never half-apply a write.
- Own memory explicitly. Keep memcheck at zero leaks.
- Errors require a nonzero exit and a clear `aids_log(AIDS_ERROR, ...)` message.
- Directory scans include `.` and `..`; skip both.
- Use four spaces and no tabs. Put opening braces on declaration or control
  lines.
- Use one declaration per statement and initialize structs with `{0}`.
- Bind pointer stars to names: `Task *task`.
- Use `lower_snake_case` for functions and locals, `Upper_Snake_Case` for
  types, and `UPPER_SNAKE_CASE` for macros.
- Keep functions `static` unless an external interface requires them.
- Validate early and use `return_defer(...)` for owned resources. Initialize
  resources before cleanup can run and release them under `defer:`.
- Use designated initializers for option tables and non-trivial structs.
- Match neighboring wrapping. Indent continuation lines by four extra spaces.

### Shell, Make, and Markdown

- Start Bash scripts with `#!/usr/bin/env bash` and `set -eu`.
- Use four spaces. Quote expansions unless intentional splitting is required.
- Use `lower_snake_case` for functions and locals and `UPPER_SNAKE_CASE` for
  constants.
- Split declaration from command substitution when exit status matters.
- Stop helper processes by recorded PID. Never use `pkill -f`.
- Test exact output with `grep -qx`. Add `|| return 1` where conditional
  invocation disables `set -e`.
- Use tabs for Make recipe lines.
- Keep Markdown concise and use fenced `bash` blocks for runnable commands.
- Match neighboring code. No automatic formatter is required.

## Verification

- New refusal rules need an exact-message test first.
- Run the relevant checks:

```bash
nix develop -c make
nix develop -c ./checker.sh
nix develop -c ./checker.sh --memcheck
nix develop -c make clean all CC=gcc
nix develop -c make windows
```

Bare builds require `TATR_ALLOW_BARE_BUILD=1`.
