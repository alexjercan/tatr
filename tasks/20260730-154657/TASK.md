# Add transactional flow lifecycle commands and guards

- STATUS: CLOSED
- PRIORITY: 90
- TAGS: feature, flow, lifecycle
- KIND: TASK
- FLOW STEP: DONE
- PLAN STATUS: APPROVED
- DEPENDS ON: 20260730-153325

## Story

As a flow driver, I want tatr to perform legal state transitions
transactionally, so an agent cannot bypass plan, review, retro, or closure
requirements and leave a knowingly invalid intermediate state.

## Steps

- [x] Extract the artifact scans `check_task` already performs into shared
      helpers - unchecked `## Steps` count, latest `- VERDICT:`, open
      BLOCKER/MAJOR finding count, `DECISION.md` status validity, sibling
      presence - and re-point `check_task` at them, so the lint and the
      lifecycle guards read the same bytes the same way.
- [x] Add the transition table over the v2 fields: `flow_step_implied_status`,
      the single successor per step, and the legality of each of the eight
      edges including the `REVIEWING -> WORKING` fix loop.
- [x] Add `main_flow` for `tatr flow <ID> [--to <STEP>]`: resolve the task,
      pick the target (explicit, or the successor when `--to` is absent),
      reject an illegal edge naming what the legal move from here is, then
      evaluate the edge's preconditions. Wire it into the dispatch chain and
      `tatr_print_help`.
- [x] Implement the preconditions: `PLANNED -> WORKING` needs
      `PLAN STATUS: APPROVED` and every `DEPENDS ON` id to resolve to a CLOSED
      task; `REVIEWING -> COMPOUNDING` needs a `REVIEW.md` whose latest verdict
      is APPROVE with no open BLOCKER/MAJOR finding; `COMPOUNDING -> DONE`
      needs all of that plus zero unchecked Steps, a `RETRO.md`, and a valid
      `DECISION.md` status when one is present. `KIND: EPIC` is exempt from
      exactly the four requirements it is already exempt from in `tatr check`.
- [x] Apply the effects in one write: `FLOW STEP`, the implied `STATUS`, and
      `PLAN STATUS: APPROVED` at the plan gate, through a single `task_save`.
      Report every unmet precondition, not just the first, and reach the write
      only when all of them hold.
- [x] Remove `-s/--status` from `new` and `edit`, and `-f/--flow-step` and
      `-S/--plan-status` from `argparse_add_v2_meta_arguments`. Scan for the
      three retired spellings before parsing and fail with a pointer to
      `tatr flow`, so the rejection is actionable rather than a generic
      unknown-argument error.
- [x] Migrate the `checker.sh` fixtures off `-s`/`-f`/`-S` onto a
      `drive_task_to <id> <STEP>` helper that walks `tatr flow` and scaffolds
      the `REVIEW.md`, `RETRO.md` and ticked Steps a target state requires.
- [x] Add the integration tests: every legal transition, each forbidden skip,
      the review fix loop, the EPIC exemptions, the dependency and plan-gate
      guards, and byte-identical rollback on every refusal.
- [x] Rewrite `README.md` around the lifecycle, from real command output: a
      `Moving a Task Through the Flow` section documenting `tatr flow` with the
      transition table, the preconditions and a refusal transcript; the
      workflow flags struck from the `Creating Tasks` (117-122) and
      `Editing a Task` (226-239) option lists; the `tatr edit ... -s
      IN_PROGRESS` example replaced; `flow` added to the subcommand list and
      Features.
- [x] Rewrite `skills/tatr/SKILL.md` around the lifecycle: `flow` in the
      Commands block, the workflow flags struck from `new`/`edit` and the
      shared metadata options (18-31), the claim-a-task and close-a-task steps
      of the Workflow section (175-190) rebuilt on `tatr flow`, the
      `tatr edit <id> -S APPROVED` instruction at 165 replaced by the plan
      gate, and a Gotchas entry for the hand-correction repair path.
- [x] Update `AGENTS.md` (the backlog cheat sheet and the flow section) and
      add a CHANGELOG entry marking the removals as breaking.

## Definition of Done

- A task cannot reach WORKING without `PLAN STATUS: APPROVED` and CLOSED
  dependencies, and the diagnostic names which precondition failed
  (test: `test_transition_start_guards`).
- Every legal edge is accepted in order, REVIEWING may return to WORKING, bare
  `tatr flow` walks the chain, and every other skip is refused
  (test: `test_transition_state_machine`).
- A refused close reports every unmet precondition and leaves `TASK.md`
  byte-identical; a satisfied close writes `DONE` and `CLOSED` together
  (test: `test_transition_close_is_atomic`).
- `STATUS`, `FLOW STEP` and `PLAN STATUS` are unsettable through `new` and
  `edit`; the retired flags fail with a pointer to `tatr flow`
  (test: `test_edit_status_uses_transition_guards`).
- `KIND: EPIC` is exempt from the plan-approval, review, retro and
  unchecked-Steps preconditions, matching `tatr check`
  (test: `test_transition_epic_exemptions`).
- No transition produces a state the lint flags: the repository backlog is
  clean (cmd: `tatr check --ledger LESSONS.md`).
- Full native and memory-check suites pass
  (cmd: `nix develop -c ./checker.sh && nix develop -c ./checker.sh --memcheck`).
- No doc surface still shows a workflow field being set through `new` or
  `edit` (cmd: `! grep -rn -- '-s IN_PROGRESS\|-s CLOSED\|-f WORKING\|-S APPROVED' README.md AGENTS.md skills/tatr/SKILL.md`).
- `README.md` and `skills/tatr/SKILL.md` both document `tatr flow`, its
  transition table and its preconditions, and their claim-a-task and
  close-a-task instructions run through it
  (cmd: `grep -c 'tatr flow' README.md skills/tatr/SKILL.md`; manual: the
  README transcripts match real `tatr flow` output, pasted not paraphrased).

## Notes

- Parent Epic: nix.dotfiles task 20260730-153122.
- Depends on: 20260730-153325.
- This replaces prompt-only transition discipline with a CLI guard.
- The command shape, the removal of the workflow flags from `new`/`edit`, the
  absence of an escape hatch, and the check-parity invariant were confirmed
  with the user before planning and are recorded in `DECISION.md`.
