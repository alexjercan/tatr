# Code conventions

Match neighboring code. No automatic formatter is required.

## C

- Keep production C in `tatr.c`. Avoid compiler-specific extensions.
- Four spaces. No tabs. Opening brace on the declaration or control line.
- One declaration per statement. Initialize structs with `{0}`.
- Pointer star binds to the name: `Task *task`.
- Functions and locals: `lower_snake_case`.
- Types: `Upper_Snake_Case`. Macros: `UPPER_SNAKE_CASE`.
- Keep functions `static` unless they form a required external interface.
- Prefer early validation and `return_defer(...)` for owned resources.
- Put all owned-resource cleanup under `defer:`. Initialize before possible
  cleanup.
- Wrap long calls by argument. Align continuation lines with four extra spaces.
- Use designated initializers for option tables and non-trivial structs.
- Comments explain constraints or non-obvious choices. Do not narrate code.

## Shell

- Bash scripts start with `#!/usr/bin/env bash` and `set -eu`.
- Four spaces. Quote expansions unless intentional splitting is required.
- Functions and locals: `lower_snake_case`; constants: `UPPER_SNAKE_CASE`.
- Use `local` inside functions. Split declaration from command substitution when
  exit status must be preserved.
- Use recorded PIDs for helper processes. Never use `pkill -f`.
- Test exact output with `grep -qx`. Add `|| return 1` where conditional
  invocation disables `set -e` behavior.

## Make and Markdown

- Make recipe lines use tabs.
- Keep Markdown concise. Use fenced `bash` blocks for runnable commands.
- Use ASCII punctuation in code, comments, and documentation.
