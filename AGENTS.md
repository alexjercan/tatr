# AGENTS.md

Repository guidance. Global `~/AGENTS.md` applies.

## Project

- Offline C CLI for Markdown tasks under `tasks/`.
- Production: `tatr.c`. Vendored: `aids.h`, `argparse.h`.
- User docs: `README.md`. Formatting: `CONVENTIONS.md`.

## Agent workflow

- Tracker/epics: use this repository's `tasks/`; keep one requested change per
  task and link related task IDs in the body.
- Examples/retention: `checker.sh` is the executable behavior catalog; keep
  task-specific evidence with the task.
- Domain docs: `README.md` for behavior, `tatr.c` for implementation, and
  `RELEASE.md` for publishing.
- Research/network: work offline from local sources.
- Checks/records: run the suite below; keep durable decisions in task records.

## Rules

- Keep production code in `tatr.c`.
- Commands use `static int main_<cmd>`, a local parser, and `defer:` cleanup.
- Reuse task resolution, load, save, serialization, and parsing helpers.
- Validate all input before mutation. Never half-apply a write.
- Own memory explicitly. Keep memcheck at zero leaks.
- Errors require nonzero exit and a clear `aids_log(AIDS_ERROR, ...)` message.
- New rules need an exact-message refusal test first.
- Directory scans include `.` and `..`; skip both.

## Checks

```bash
nix develop -c make
nix develop -c ./checker.sh
nix develop -c ./checker.sh --memcheck
nix develop -c make clean all CC=gcc
nix develop -c make windows
```

Bare builds require `TATR_ALLOW_BARE_BUILD=1`.
