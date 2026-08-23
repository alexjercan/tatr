# AGENTS.md

Global `~/AGENTS.md` applies. This file defines project-specific instructions.

## Project

- Offline C CLI for Markdown tasks under `tasks/`.
- Production code is `tatr.c`. `aids.h` and `argparse.h` are vendored.
- `README.md` defines behavior. `checker.sh` is the executable behavior catalog.
- `RELEASE.md` defines publishing.

## Workflow

- Work directly on `master` unless the user requests an isolated worktree.
- Use this repository's Tatr tracker for requested tracked work. Keep one task
  for one request and its follow-up work.
- Keep task records and evidence under `tasks/<id>/`. Link related task IDs in
  their bodies.
- Use Sprout only when the user requests an isolated worktree.
- Work offline from local sources.

## Conventions

- Keep production C in `tatr.c`. Avoid compiler-specific extensions.
- Reuse task resolution, loading, parsing, serialization, and saving helpers.
- Validate all input before mutation. Never half-apply a write.
- Own memory explicitly and keep memcheck at zero leaks.
- Return nonzero with a clear `aids_log(AIDS_ERROR, ...)` message on errors.
- Keep functions `static` unless an external interface requires them.
- Match neighboring C, shell, Make, and Markdown style. No formatter is required.
- Add an exact-message test before a new refusal rule.
- Run the cheapest relevant checker case. Use `./checker.sh --memcheck` for
  ownership changes and `nix flake check` for broad packaging integration.
- Run builds through `nix develop`; bare builds require
  `TATR_ALLOW_BARE_BUILD=1`.
