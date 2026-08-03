# Make tatr flow --dry-run a real precondition probe

- PRIORITY: 70
- TAGS: tatr,flow,cli
- KIND: TASK
- ACTIVITY: -
- GATES: -
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
