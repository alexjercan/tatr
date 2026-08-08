# Update docs for TASK.md epic containers

- STATUS: CLOSED
- PRIORITY: 70
- TAGS: docs, flow

## Story

As a tatr maintainer, I want the repository guidance to describe explicit flow
containers as TASK.md records, so agents stop looking for or documenting a
separate sidecar record.

## Steps

- [x] Update `AGENTS.md` to describe explicit container tasks as `goal`-tagged
      tasks whose broader record lives in the container `TASK.md`.
- [x] Update `README.md` so `tatr check` docs explain the strict and
      `unplanned-in-progress` exemptions in terms of explicit containers, not a
      separate sidecar.
- [x] Sweep stale comments in the checker sources if they still describe the
      old container record shape.
- [x] Run the tatr checks and test suite.

## Definition of Done

- Live user-facing docs do not mention the removed sidecar
  (cmd: `rg -n "GOAL\\.md" AGENTS.md README.md` exits 1).
- AGENTS.md describes explicit containers as `goal`-tagged TASK.md records
  (cmd: `rg -n "container.*TASK\\.md|TASK\\.md.*container" AGENTS.md`).
- README.md documents the explicit container exemption model
  (cmd: `rg -n "container.*goal|goal.*container" README.md`).
- tatr conformance passes
  (cmd: `./tatr check` and `./tatr check --ledger LESSONS.md`).
- tatr test suite passes
  (cmd: `nix develop -c ./checker.sh`).

## Work Notes

- Updated `AGENTS.md` so strict check exemptions describe `goal`-tagged tasks as
  explicit flow containers whose aggregate state lives in the container
  `TASK.md`.
- Updated `README.md` so the check-rule documentation explains ordinary tasks,
  historical tasks, and explicit containers without pointing agents at a
  separate sidecar.
- Updated stale `tatr.c` comments so the implementation comments match the
  documented container model. No behavior changed.

## Verification

- `rg -n "GOAL\\.md" AGENTS.md README.md` exited 1 with no output.
- `rg -n "GOAL\\.md|umbrella|goal artifact|durable record" AGENTS.md README.md tatr.c checker.sh` exited 1 with no output.
- `rg -n "container.*TASK\\.md|TASK\\.md.*container" AGENTS.md` matched the
  container task guidance.
- `rg -n "container.*goal|goal.*container" README.md` matched the explicit
  container exemption wording.
- `./tatr check` passed.
- `./tatr check --ledger LESSONS.md` passed.
- `nix develop -c ./checker.sh` passed 73/73 tests.
- `nix develop -c ./checker.sh --memcheck` passed 73/73 tests.
- `nix flake check` passed.

## Reflection

- The right compromise was to keep the existing `goal` tag semantics while
  changing the documented meaning from a separate record to an explicit
  container `TASK.md`.
- The main risk was docs drifting from code comments; sweeping `tatr.c` as a
  doc surface removed that mismatch.
