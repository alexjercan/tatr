# Add an honest terminal state for retired tasks

- STATUS: OPEN
- PRIORITY: 60
- TAGS: feature, flow, lifecycle
- KIND: TASK
- FLOW STEP: BACKLOG
- PLAN STATUS: DRAFT

## Story

As a task author, I want a way to retire a task that should not be worked, so
that a superseded or duplicated record does not have to choose between staying
OPEN forever and having a review and retro fabricated for work nobody did.

## Steps

- [ ] Confirm the gap on the current build: from BACKLOG the only path to
      `STATUS: CLOSED` is the full walk to DONE, and DONE is gated behind a
      REVIEW.md whose latest verdict is APPROVE plus the COMPOUNDING step.
      `tatr edit` has no `--status`, so `tatr rm` is the only alternative and
      it destroys the record.
- [ ] Decide the shape with the user: a terminal `DROPPED`/`SUPERSEDED` flow
      step reachable from any non-DONE step and requiring a reason, versus a
      `tatr close <id> --superseded-by <id> --reason <text>` command, versus
      keeping `tatr rm` and treating removal as the answer.
- [ ] Implement the chosen shape in `tatr.c` with its own `checker.sh` test,
      mutation-tested per AGENTS.md before review.
- [ ] Decide whether `tatr check` should require a retired record to name what
      superseded it, and whether an Epic's child gate treats a retired child as
      settled.

## Definition of Done

- A task can reach a terminal retired state without a REVIEW.md or a RETRO.md
  (test: name the `checker.sh` test once the shape is chosen).
- The retired state records WHY and, when applicable, what superseded it
  (test: same).
- Full native and memory-check suites pass
  (cmd: `nix develop -c ./checker.sh && nix develop -c ./checker.sh --memcheck`).
- `tatr check` is clean (cmd: `nix develop -c ./dist/tatr check`).

## Notes

- Moved here from nix.dotfiles on 2026-07-31, where it was seeded by that
  repository's task 20260730-155003 and reworded for this repository. The
  mechanism was always going to land here; only the skill wording is theirs.
- Origin: 20260730-155003's plan said "close 20260731-104819 as superseded via
  `tatr edit`"; no such option exists, and the lifecycle offers no honest
  terminal state for a task whose work another task absorbed. `tatr rm` was
  used instead, with the rationale preserved in that task's DECISION.md,
  because the alternative was fabricating an APPROVE verdict for work that
  never happened. nix.dotfiles retired a second record the same way on
  2026-07-31 (20260731-010900), so the workaround has now been used twice.
- Cross-repository follow-up, NOT part of this task's Definition of Done: once
  the shape lands, teach the nix.dotfiles flow-family skills when to use the
  retire path instead of `tatr rm`
  (`grep -rn "supersed" /home/alex/personal/nix.dotfiles/home/modules/agents/skills`).
