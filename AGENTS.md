# AGENTS.md

Project-specific guidance. Global `AGENTS.md` still applies.

## Project

- tatr: offline C CLI for Markdown tasks under `tasks/`.
- Task ID: timestamp HUID `YYYYMMDD-HHMMSS`.
- Program: single translation unit, `tatr.c`.
- Vendored headers: `aids.h`, `argparse.h`.
- Build: `Makefile`, Nix toolchain.
- Tests: `checker.sh` integration suite.
- User docs: `README.md`.
- Durable lessons: `LESSONS.md`; read before work.

## Build

```bash
nix develop -c make
nix develop -c make windows
nix develop -c make clean
```

- Bare builds blocked by default.
- Provisioned non-Nix toolchain: `TATR_ALLOW_BARE_BUILD=1`.
- Keep clang, gcc, and MinGW warning-clean under `-Wall -Wextra`.

## Test

```bash
nix develop -c ./checker.sh
nix develop -c ./checker.sh -v
nix develop -c ./checker.sh --memcheck
```

- Run after every code change.
- Prefer integration tests and runnable CLI paths.
- Cover refusal and success paths.
- New check rule: write the exact-message assertion first; use `grep -qx`.
- Expected failure under `set -e`: follow the existing split-declaration pattern in `checker.sh`.
- Generated task ID: use `new_task_id`.
- Negative assertions: avoid fixture names containing another fixture's slug.
- New guard: remove its side effect, rebuild, and confirm its own test fails.
- Mutant crash, hang, or corruption: add the invariant at the use site.
- Restore each mutation immediately after observing failure.

## Code

- Keep production code in `tatr.c`.
- Command shape: `static int main_<cmd>` with its own parser and `defer:` cleanup.
- Wire commands into dispatch and `tatr_print_help`.
- Resolution: reuse `task_resolve`.
- Persistence: reuse `task_load`, `task_save`, `task_serialize`, `task_deserialize`.
- Memory: explicit ownership, full `defer:` cleanup, zero memcheck leaks.
- Errors: non-zero exit plus clear `aids_log(AIDS_ERROR, ...)` message.
- Writes: validate fully before mutation; never half-apply.
- Deletion: validated HUID only; target only `tasks/<id>/`.
- Directory scans: `aids_io_listdir` includes `.` and `..`; skip both.

## Workflow invariants

- Tracker, lifecycle, records, claims, and proofs: load `skills/tatr/SKILL.md`.
- Rule ownership: collectors return problems as data.
- Check output: only `check_report_problems`.
- Flow findings: only `flow_unmet_add_problems`.
- Cross-cutting rule: shared collector used by both `check` and `flow`.

## Agent workflow

- Tracker/epics: `tasks/`; usage in `skills/tatr/SKILL.md`.
- Examples/retention: `checker.sh`; retain task-specific evidence in `tasks/<id>/`.
- Domain docs: `README.md`; implementation truth in `tatr.c`.
- Research/network: offline-first; task research belongs in its scaffolded `SPIKE.md`.
- Checks/records: `checker.sh`, `tatr check --ledger LESSONS.md`, and scaffolded task records.
