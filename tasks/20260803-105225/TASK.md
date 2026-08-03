# Make tatr flow --dry-run a real precondition probe

- PRIORITY: 70
- TAGS: tatr, flow, cli
- KIND: TASK
- ACTIVITY: WORKING
- GATES: PLAN
- RESOLUTION: -

`tatr flow --dry-run` currently returns before any precondition is evaluated
(`tatr.c:5917`, right after the edge and gate are computed). It prints the edge
and the gate name and exits 0 even when the real call would refuse. That makes
it useless as a probe: a caller cannot ask "would this advance succeed?".

Make the exit status the whole contract. `--dry-run` must evaluate everything
the real call evaluates and write nothing:

- load the task graph
- run `flow_gate_preconditions` and `flow_close_preconditions`
- run `flow_world_preconditions` on the edge entering `WORKING`
- print the same `flow_unmet_print` output the real refusal prints
- exit non-zero when any precondition is unmet, 0 otherwise
- never call `task_save`, never record a gate

Keep the existing "would move X -> Y" / gate line so the output stays readable
on its own.

The half-success case needs a decision: the real command can record the gate
and hold the cursor when only the world preconditions fail. A dry run has to
report both halves rather than collapse them into one boolean, and its exit
status should reflect "the advance would not complete".

Consumer: the afk runner in nix.dotfiles wants to execute the three mechanical
flow gates itself and only wake an agent when the probe refuses. It needs an
exit status it can branch on and the unmet text to hand to that agent.

The half-success reporting choice is settled in `tasks/20260803-105225/DECISION.md`:
two tensed messages, one non-zero exit status for both.

## Steps

- [ ] `checker.sh`: add `test_flow_dry_run_probes_preconditions` next to
  `test_flow_half_succeeds_when_blocked` (~1545) and register it in the runner
  list at the tail, near `test_flow_advances_and_records_gates`. Three cases in
  one walk, each asserting exit status, exact message text, and that
  `tasks/<id>/TASK.md` is byte-identical afterwards (`cmp -s`):
  record half unmet (a `PLANNING` task with no `## Steps`/`## Definition of Done`)
  exits non-zero, prints `would move PLANNING -> WORKING`, `gate PLAN would run`,
  `Would refuse to advance <id> from PLANNING`, the two `bad-record-schema` lines
  and `Record unchanged.`; world half unmet (the blocker fixture from
  `test_flow_half_succeeds_when_blocked`: plan sections written, dependency still
  open) exits non-zero, prints the header, `Would not advance <id> to WORKING`,
  `<blocker> is not CLOSED` and `Cursor would be held at PLANNING`, and leaves
  `GATES: -`; both halves clear (blocker driven to DONE) exits 0 and prints only
  the header lines. Use the `set +e` / split-declaration pattern for every
  expected-failure call.
- [ ] `checker.sh`: tighten the existing dry-run assertion inside
  `test_flow_advances_and_records_gates` (~1473) to capture and require exit
  status 0, so the passing edge is covered by the same contract.
- [ ] `tatr.c` `main_flow`: delete the `if (dry_run) { ... return_defer(0); }`
  block at ~5913-5924 and re-emit the same header after `task_graph_load`
  succeeds (~5930), followed by `fflush(stdout)` so the header cannot land after
  stderr when stdout is piped.
- [ ] `tatr.c` `main_flow`: tense the record-half refusal at ~5938 to
  `dry_run ? "Would refuse" : "Refusing"` while keeping `flow_unmet_print`,
  `Record unchanged.` and `return_defer(1)` unchanged.
- [ ] `tatr.c` `main_flow`: after `flow_world_preconditions` runs (~5951) and
  before `task.meta.gates |= ...`, return for a dry run: `return_defer(0)` when
  `world_unmet.count == 0`, otherwise log
  `Would not advance <id> to <to_label>`, `flow_unmet_print(&world_unmet)`,
  `Cursor would be held at <from_label>.` and `return_defer(1)`. This is the
  structural guarantee that no dry run reaches `task_save`.
- [ ] `README.md`: replace the `--dry-run` one-liners at ~314 and ~485 with the
  probe contract (evaluates every precondition, writes nothing, exit status is
  the answer), and extend the flow transcript at ~379 with a refused dry run
  showing `Would refuse to advance` and its non-zero status.
- [ ] `skills/tatr/lifecycle.md:21`: the table cell still reads "`--dry-run`
  prints the edge"; restate it as the probe (evaluates the same preconditions,
  writes nothing, non-zero when the advance would not complete).
- [ ] `nix develop -c make` clean under `-Wall -Wextra`, then
  `nix develop -c ./checker.sh` and `nix develop -c ./checker.sh --memcheck`
  green. Sabotage each new assertion once (drop the `fflush`, drop the world-half
  dry return) and confirm the new test alone turns red.

## Definition of Done

- A dry run whose record half is unmet exits non-zero, prints the tensed refusal
  with the same `flow_unmet_print` lines the real call prints, and leaves
  `TASK.md` byte-identical
  (test: `test_flow_dry_run_probes_preconditions`).
- A dry run whose world half is unmet exits non-zero, reports the held cursor in
  the conditional, and records no gate - `GATES` stays `-`
  (test: `test_flow_dry_run_probes_preconditions`).
- A dry run on an edge the real call would complete exits 0 and prints only the
  edge header and gate line
  (test: `test_flow_dry_run_probes_preconditions`).
- The passing dry run inside the existing forward walk is asserted on exit
  status, not just output (test: `test_flow_advances_and_records_gates`).
- `README.md` documents the probe contract and shows a refused dry run
  (cmd: `grep -n "Would refuse to advance" README.md`).
- The skill lifecycle table describes `--dry-run` as the probe rather than an
  edge printer
  (cmd: `grep -n "would not complete" skills/tatr/lifecycle.md`).
- The suite is green and leak-free
  (cmd: `nix develop -c ./checker.sh --memcheck`).

## Notes

- Discovered: the dry-run early return is `tatr.c:5913-5924`, before
  `task_graph_init`/`task_graph_load` at 5926-5930. Both precondition passes and
  both report sites are inside `main_flow`; no collector signature changes.
- The four collectors write only into a `Flow_Unmet`, so this is a control-flow
  change, not a new evaluator.
- The real command already `fflush(stdout)` after `gate ... recorded` for the
  same stdout-block-buffering reason (`tatr.c:5975`); the dry run needs the same
  treatment for its header.
- Assumption: no in-tree or external caller relies on `--dry-run` exiting 0 on
  an unresolvable graph or on its current always-0 status. `grep -rn "dry-run"`
  over `tatr.c`, `checker.sh`, `README.md` and `skills/` finds only the flow
  test, the migrate/scaffold flags and the docs touched here.
- Proofs verified red on base: `grep -n "Would refuse to advance" README.md`
  exits 1, and `test_flow_dry_run_probes_preconditions` does not exist in
  `checker.sh`.
- Out of scope, still open: whether `tatr close --resolution DONE`
  (`tatr.c:6343`) should grow the same probe. The consumer only drives `flow`.
