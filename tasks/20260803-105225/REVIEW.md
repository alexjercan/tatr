# Review: Make tatr flow --dry-run a real precondition probe

- TASK: 20260803-105225
- BRANCH: feat/flow-dry-run-probe

## Round 1

- REVIEWER: out-of-context
- VERDICT: APPROVE

- [x] R1.1 (MINOR) checker.sh:1660 - the header assertions in
  `test_flow_dry_run_probes_preconditions` use
  `grep -q "would move PLANNING -> WORKING"` and `grep -q "gate PLAN would run"`,
  but `AGENTS.md:38` requires `grep -qx` for exact-message checks and the
  neighbouring test already does so (`grep -qx "gate PLAN recorded"`,
  checker.sh:1587). The loose form pins neither the `Task <id> ` prefix nor the
  two-space indent, so a header that dropped either still passes. Change the
  three header grep pairs (1660-1661, 1677-1678, 1698-1699) to
  `grep -qx "Task $id would move PLANNING -> WORKING"` and
  `grep -qx "  gate PLAN would run"`, and use the same exact form in the
  `head -1` ordering checks (1672, 1685).
  - Response: Applied. All three header pairs and both `head -1` ordering checks
    in `test_flow_dry_run_probes_preconditions` now use `grep -qx` with the
    `Task $id ` prefix and the two-space gate indent. The forward-walk
    assertion at checker.sh:1481 was left loose - outside the finding's stated
    scope and covered by its own task record.
- [x] R1.2 (NIT) checker.sh:1697 - the DoD clause is "prints only the edge
  header and gate line", but the clear case asserts two present lines plus two
  negative greps, so an extra printed line would pass. Add
  `[ "$(printf '%s\n' "$clear_out" | wc -l)" -eq 2 ] || ok=0` after line 1701.
  - Response: Applied. The finding was not vacuous: sabotaging `main_flow` with
    an extra `printf` in the dry-run header block turns
    `test_flow_dry_run_probes_preconditions` alone red (112/113), where before
    the count assertion it stayed green. `tatr.c` restored, suite 113/113 under
    `--memcheck`.

Verified independently of the round-1 reviewer:

- `nix develop -c make` clean under `-Wall -Wextra`; `./checker.sh` 113/113 and
  `./checker.sh --memcheck` 113/113 (the third `cmd:` proof).
- Both grep `cmd:` proofs exit 0: `README.md:384` carries
  `Would refuse to advance`, `skills/tatr/lifecycle.md:21` carries
  `would not complete`.
- Sabotage of the `fflush(stdout)` after the dry-run header: suite drops to
  112/113, so the ordering clause is not vacuous. `tatr.c` restored, tree clean.
- Structural claim re-derived from the diff: both dry-run returns
  (`tatr.c:5934`, `tatr.c:5964-5975`) precede `task.meta.gates |= ...`, so no
  probe path reaches `task_save`.
- Every Step's literal text matches the diff; no tick outruns the change.
- Close-out claims in TASK.md reproduce - no honesty gap.

No open `manual:` proofs.
