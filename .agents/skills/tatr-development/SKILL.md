---
name: tatr-development
description: Change Tatr C behavior, Markdown storage, parsers, shell checks, builds, or memory ownership.
---

# Tatr development

Treat `README.md` as the CLI contract and `checker.sh` as the behavior catalog.

- Keep commands as `static int main_<cmd>` with a local parser and `defer:`
  cleanup. Reuse shared task helpers.
- Validate before mutation. Initialize owned resources before cleanup can run,
  release them under `defer:`, and use `return_defer(...)`.
- Use four spaces, one declaration per statement, `{0}` initialization, and
  designated initializers for non-trivial structs. Bind pointer stars to names.
- Use `lower_snake_case` for functions and locals, `Upper_Snake_Case` for types,
  and `UPPER_SNAKE_CASE` for macros.
- Directory scans include `.` and `..`; skip both.
- Start Bash scripts with `#!/usr/bin/env bash` and `set -eu`. Quote expansions
  and split declarations from command substitutions when status matters.
- Use tabs for Make recipe lines. Test exact output with `grep -qx`.

Use the narrowest relevant checks:

```bash
nix develop -c make
nix develop -c ./checker.sh
nix develop -c ./checker.sh --memcheck
nix develop -c make clean all CC=gcc
nix develop -c make windows
```

Run memcheck for ownership changes, GCC for portability changes, and the Windows
build for platform-sensitive changes. Do not run every variant by default.
