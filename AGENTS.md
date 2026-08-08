# AGENTS.md

Project guidance. Global `AGENTS.md` still applies.

## Project

- Offline C CLI for Markdown tasks under `tasks/`.
- Production: `tatr.c`; vendored: `aids.h`, `argparse.h`.
- Integration tests: `checker.sh`; user docs: `README.md`.
- Formatting: `CONVENTIONS.md`.

## Build and test

```bash
nix develop -c make
nix develop -c ./checker.sh
nix develop -c ./checker.sh --memcheck
nix develop -c make clean all CC=gcc
nix develop -c make windows
```

- Bare builds require `TATR_ALLOW_BARE_BUILD=1`.
- Keep clang, gcc, and MinGW warning-clean under `-Wall -Wextra`.
- Run the integration suite after each code change.
- Cover success and refusal paths.
- For a new rule, write the exact-message check first with `grep -qx`.
- Under `set -e`, capture expected failures with the existing split pattern.
- Mutation check: remove the new side effect, rebuild, confirm its test fails,
  then restore it immediately.

## Implementation

- Keep production code in `tatr.c`.
- Commands use `static int main_<cmd>`, a local parser, and `defer:` cleanup.
- Reuse task resolution, load, save, serialization, and deserialization helpers.
- Validate all input before mutation. Never half-apply a write.
- Own memory explicitly. Require full cleanup and zero memcheck leaks.
- Errors: non-zero exit and a clear `aids_log(AIDS_ERROR, ...)` message.
- New task bodies: `-b, --body PATH`; `-` means stdin. Read before creation.
- Directory scans include `.` and `..`; skip both.

## Agent workflow

- Tracker and usage: `tasks/`, `skills/tatr/SKILL.md`.
- Examples and test evidence: `checker.sh` or the task directory.
- Domain docs: `README.md`; implementation truth: `tatr.c`.
- Research: offline-first; keep durable decisions in the task body.
- Knowledge: `/home/alex/personal/agent-knowledge`; project `tatr`; tags
  `tasks,c,workflow,agents`. Advisory only.
