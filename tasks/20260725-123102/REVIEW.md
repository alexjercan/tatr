# Review: Update docs for TASK.md epic containers

- TASK: 20260725-123102
- BRANCH: master

## Round 1

- VERDICT: APPROVE
- REVIEWER: in-session (no explicit subagent delegation request)

Findings: none.

Verification:

- `AGENTS.md` now defines `goal`-tagged tasks as explicit flow containers whose
  aggregate record lives in the container `TASK.md`.
- `README.md` now documents `unplanned-in-progress`, strict
  record-completeness, and `closed-unchecked` exemptions in terms of historical
  tasks and explicit containers tagged `goal`.
- `tatr.c` comments no longer describe explicit containers as sidecar-backed
  records.
- `rg -n "GOAL\\.md" AGENTS.md README.md` exited 1 with no output.
- `rg -n "container.*TASK\\.md|TASK\\.md.*container" AGENTS.md` found the
  container `TASK.md` guidance.
- `rg -n "container.*goal|goal.*container" README.md` found the explicit
  container exemption guidance.
- `./tatr check` passed.
- `./tatr check --ledger LESSONS.md` passed.
- `nix develop -c ./checker.sh` passed 73/73 tests.
- `nix develop -c ./checker.sh --memcheck` passed 73/73 tests.
- `nix flake check` passed.
