# Replace the FLOW STEP chain with ACTIVITY, GATES and RESOLUTION

- STATUS: OPEN
- PRIORITY: 95
- TAGS: feature, lifecycle, breaking
- KIND: TASK
- FLOW STEP: PLANNED
- PLAN STATUS: APPROVED

## Story

As a flow driver, I want the lifecycle split into a freely moving `ACTIVITY`
cursor, an earned `GATES` fact set and a terminal `RESOLUTION`, so a record can
go back to planning without lying about what has been approved, a plan can be
blessed while its dependencies are still open, and a task can be abandoned with
a reason instead of a chain node.

`FLOW STEP` currently carries both jobs at once. Because position implies
proof, the machine can only afford one backward edge (`REVIEWING -> WORKING`,
tatr.c:5270) and none at all back to `PLANNING`; `PLAN STATUS` restates a fact
the position already implies and needs a `NOT_REQUIRED` escape hatch for
records that predate it; and every closure reason beyond DONE/DROPPED would
have to become another node with another row in every gate table. `SPIKE.md`
holds the analysis and the options that were rejected; `NOTES.md` holds the
target transcript this task is finished against.

## Steps

- [ ] Replace the metadata triple in `Task_Meta`: drop `Flow_Step`,
      `Plan_Status` and the stored `status` field; add `Activity` (nullable,
      `UNDERSTANDING PLANNING WORKING REVIEWING COMPOUNDING`), `gates` (a
      bitset over `PLAN REVIEW RETRO`) and `Resolution` (nullable,
      `DONE WONTDO DUPLICATE SUPERSEDED`) plus `duplicate_of`. Update
      `STATUS_FORMAT`/`FLOW_STEP_FORMAT`/`PLAN_STATUS_FORMAT` (tatr.c:202-207)
      to `- ACTIVITY: `, `- GATES: `, `- RESOLUTION: `, and rewrite
      `task_serialize` (tatr.c:342-392) and `task_deserialize`
      (tatr.c:460-565) around the new fixed field order. An unset value
      serializes as `-`; `GATES` serializes in gate order, space separated.
- [ ] Add `task_derived_status` returning CLOSED when `RESOLUTION` is set,
      OPEN when `ACTIVITY` is unset, IN_PROGRESS otherwise, and re-point every
      reader of `task->meta.status` at it. Delete
      `flow_step_implied_status` (tatr.c:5231). `- STATUS: ` stops being
      written to `TASK.md`; every command that reports still prints it.
- [ ] Add the two ordering helpers the machine runs on: the gate each activity
      produces on exit (`PLANNING -> PLAN`, `REVIEWING -> REVIEW`,
      `COMPOUNDING -> RETRO`; `UNDERSTANDING` and `WORKING` produce none), and
      the clear set for a rewind to activity A (every gate produced at or
      after A). Both are single tables next to the activity enum, read by
      `flow`, `rewind` and `check` alike, per the shared-collector invariant
      in AGENTS.md.
- [ ] Rewrite `main_flow` (tatr.c:5596) as forward-only single-activity
      advance with no `--to`: resolve the task, refuse when a `RESOLUTION` is
      set, run the current activity's exit gate through the existing
      `Record_Problems` collectors, and on success record the gate. Advance
      the cursor only when the world permits it - dependencies CLOSED, no
      foreign claim - and when it does not, keep the recorded gate, hold the
      cursor and report both halves. Both halves reach disk through one
      `task_save`. Keep `--dry-run` printing the edge and the gate it would
      run without writing.
- [ ] Add `main_rewind` for `tatr rewind <id> --to <ACTIVITY>`: backward moves
      only, refusing a forward or equal target by naming `tatr flow`. It runs
      no gate and clears the rewind set, printing each cleared gate by name.
      Require `--force` when the clear set is non-empty and the record carries
      that gate, so discarding an earned approval is never silent.
- [ ] Add `main_close` for
      `tatr close <id> --resolution <R> [--of <ID>]` and `main_reopen` for
      `tatr reopen <id>`. `DONE` runs the close gate (all three gates earned,
      no unchecked `## Steps`, valid `DECISION.md` when present); the other
      three resolutions run no gate and are legal from any activity.
      `DUPLICATE` and `SUPERSEDED` require `--of` and write
      `- DUPLICATE OF: `, validated to resolve like any relationship write.
      `reopen` clears `RESOLUTION` and leaves the cursor where it was. Fold
      the `DONE` close into `tatr flow` from `COMPOUNDING` so the happy path
      stays one motion.
- [ ] Re-point the lifecycle check rules at the new fields: the TASK/STORY
      plan sections become required once `PLAN` is in `GATES` rather than from
      `PLANNED` onward (tatr.c:4546, 6053); `unplanned-in-progress` reads
      `ACTIVITY >= WORKING` without the `PLAN` gate; the `closed-*` family
      keys on `RESOLUTION` being set. Add `inconsistent-gates` for a record
      whose cursor is past an activity whose gate it does not carry, which is
      the drift the chain used to make unrepresentable. Findings still leave
      through `check_report_problems` and `flow_unmet_add_problems` only.
- [ ] Retire `:flow_step` and `:plan_status` from the filter engine and add
      `:activity` and `:resolution` to `tatr_filter_enum_table`
      (tatr.c:2755-2795) and `:gates` to the `contains` arm alongside `:tags`
      and `:depends` (tatr.c:3031). No new operator: `contains` already covers
      set fields. Reject the two retired spellings by name with a pointer to
      the replacement, following the `-s`/`-f`/`-S` precedent.
- [ ] Re-point `main_frontier` (tatr.c:6958) at the derived ready query -
      `GATES` has `PLAN`, `ACTIVITY` below `WORKING`, dependencies CLOSED, no
      foreign claim - keeping the `READY`/`BLOCKED`/`CLAIMED` ordering and the
      `blocked-by` column byte for byte. Open work becomes "no `RESOLUTION`"
      rather than `STATUS: OPEN`, since `UNDERSTANDING` and `PLANNING` now
      derive as IN_PROGRESS. Print the row's gates next to the activity so the
      column still separates drafting from blessed.
- [ ] Generalize the exemption format to a whole-task entry. `- <task-id>:
      <reason>` with no rule token suppresses every rule for that task;
      `- <task-id> <rule>: <reason>` keeps its current meaning.
      `check_exemptions_load` (tatr.c:5844) currently tokenizes the HUID on a
      space, so a rule-less line fails `ishuid` and is skipped as prose - split
      on the first `:` before deciding which form the line is. A whole-task
      entry that never fires is still reported as `unused-exemption`, so the
      blunt form cannot rot either.
- [ ] Add `main_migrate` for `tatr migrate [--apply]`, dry-run by default,
      printing one line per record it would change and writing nothing without
      `--apply`. It is the only place in the v1 binary that knows the v0
      format: `- STATUS: ` is dropped; `BACKLOG` becomes an unset `ACTIVITY`;
      `PLANNED` becomes `ACTIVITY: PLANNING` plus the `PLAN` gate; `DONE`
      becomes `ACTIVITY: COMPOUNDING` plus `RESOLUTION: DONE`; `DROPPED`
      becomes `RESOLUTION: WONTDO` keeping its `- REASON: `; the other steps
      keep their names. `PLAN STATUS: APPROVED` becomes the `PLAN` gate, and
      the `REVIEW` and `RETRO` gates are written from the presence of an
      APPROVE-verdict `REVIEW.md` and a `RETRO.md`. The mapping is
      record-local, so the command works unchanged in any repository.
      `task_deserialize` refuses a record still carrying `- FLOW STEP: ` with a
      pointer to `tatr migrate`. Only metadata headers move: no `REVIEW.md` or
      `RETRO.md` body is rewritten, per the append-only principle
      `tasks/EXEMPTIONS.md` already states.
- [ ] Run `tatr migrate --apply` over this repository's 39 records and collapse
      the 37 per-rule exemption lines over 21 tasks into whole-task entries for
      the historical records whose `REVIEW.md`/`RETRO.md` bodies still cannot
      satisfy the current schema. Rewrite the `tasks/EXEMPTIONS.md` preamble
      around both forms.
- [ ] Release as v1.0.0: bump `TATR_VERSION` (tatr.c:13) and both `version`
      strings in `flake.nix` (38, 61), which are currently drifted at 0.2.2 and
      0.2.1, and write the `CHANGELOG.md` entry as a major break - the record
      format, `flow --to`, `:flow_step`, `:plan_status`, and `PLANNED`,
      `DONE`, `DROPPED` and `NOT_REQUIRED` as values, against the added
      `rewind`, `close`, `reopen`, `migrate` and whole-task exemptions.
- [ ] Migrate `checker.sh`: rebuild `drive_task_to` (checker.sh:609) on the
      new commands and add the integration tests - each activity edge, the
      forward-only refusal from `rewind`, the rewind clear table including
      `PLAN` surviving a rewind to `WORKING`, the `--force` requirement, the
      half-succeeding `flow` on an open dependency, every resolution including
      `--of` validation, `reopen`, the close gate, the EPIC exemptions, the
      retired filter spellings, and byte-identical rollback on every refusal.
- [ ] Rewrite the docs against real output: `README.md` lifecycle sections
      (298-360, 490-510, 580-660, 758-770), `skills/tatr/lifecycle.md`,
      `format.md`, `check-rules.md`, `SKILL.md` and `workflow.md`, the
      `AGENTS.md` flow section, and a `CHANGELOG.md` entry marking the record
      format and the `flow --to` removal as breaking. Every transcript pasted
      from a real run, not paraphrased.

## Definition of Done

- A record round-trips through the new field set, and a record still carrying
  `- FLOW STEP: ` is refused with a pointer to the migration script
  (test: `test_record_v3_roundtrip`).
- `STATUS` is derived, not stored: no `TASK.md` contains a `- STATUS: ` line,
  and `show` still prints one
  (cmd: `! grep -rln '^- STATUS: ' tasks/*/TASK.md`;
  test: `test_status_is_derived`).
- `tatr flow` advances one activity, records the exit gate, and refuses to run
  at all on a task with a `RESOLUTION`
  (test: `test_flow_advances_and_records_gates`).
- `tatr flow` on a task with an open dependency records the `PLAN` gate, holds
  the cursor at `PLANNING`, and reports both halves; the write is one
  `task_save` and no other field changes
  (test: `test_flow_half_succeeds_when_blocked`).
- `tatr rewind` refuses a forward or equal target, and clears exactly the gates
  in the rewind table - a rewind to `WORKING` keeps `PLAN` and clears `REVIEW`
  (test: `test_rewind_clear_table`).
- Clearing an earned gate without `--force` is refused and leaves `TASK.md`
  byte-identical (test: `test_rewind_force_guard`).
- Every resolution closes from any activity, `DONE` alone runs the close gate,
  `DUPLICATE` without `--of` is refused, and `reopen` restores the cursor
  (test: `test_close_resolutions`; test: `test_reopen_restores_cursor`).
- `inconsistent-gates` fires on a cursor past an ungated activity and is silent
  on a consistent record (test: `test_check_inconsistent_gates`).
- `:activity`, `:resolution` and `:gates contains PLAN` filter correctly, and
  `:flow_step` and `:plan_status` fail by name with a pointer
  (test: `test_filter_lifecycle_fields`).
- `tatr frontier` still orders `READY` before `BLOCKED` before `CLAIMED` with
  the same `blocked-by` column, computed from the derived ready query
  (test: `test_frontier_ready_query`).
- `tatr migrate` writes nothing without `--apply`, and `--apply` maps every
  legacy step and plan status to the right activity, resolution and gate set,
  including gates read from `REVIEW.md` and `RETRO.md`
  (test: `test_migrate_dry_run_writes_nothing`;
  test: `test_migrate_maps_every_legacy_value`).
- A record still carrying `- FLOW STEP: ` is refused by every command that
  loads it, with a pointer to `tatr migrate`
  (test: `test_legacy_record_refused_with_pointer`).
- A whole-task exemption suppresses every rule for that task, a rule-scoped
  entry still suppresses only its own, and an unused entry of either form is
  reported (test: `test_exemption_whole_task`).
- Every record under `tasks/` is migrated and the whole backlog is clean under
  the new rules
  (cmd: `! grep -rl '^- FLOW STEP: ' tasks/*/TASK.md >/dev/null && tatr check`).
- The three version sites agree on 1.0.0
  (cmd: `test "$(tatr --version | grep -o '[0-9.]*')" = 1.0.0 && test "$(grep -c 'version = "1.0.0"' flake.nix)" = 2`).
- Full native and memory-check suites pass
  (cmd: `nix develop -c ./checker.sh && nix develop -c ./checker.sh --memcheck`).
- No doc or skill surface still names a retired token
  (cmd: `! grep -rn --exclude-dir=tasks --exclude-dir=.git --include='*.md' --include='*.sh' -E 'FLOW STEP|PLAN STATUS|NOT_REQUIRED|flow_step|plan_status' .`).
- `README.md` and `skills/tatr/lifecycle.md` document the activity edges, the
  gate table, the rewind clear table and the resolutions
  (manual: the transcripts match real output, pasted not paraphrased).
- The `NOTES.md` transcript runs verbatim against the built binary
  (manual: user replays it end to end).

## Notes

- `SPIKE.md` records why the three rejected options lose; `NOTES.md` records
  the target transcript, and its two closing observations are the behaviours
  most likely to be argued with in review.
- Breadth is deliberate and does not split. The record format, the commands
  that write it, the lint that reads it and the docs that describe it cannot
  land separately without either a throwaway compatibility shim or a released
  binary that refuses its own backlog. One breaking change, one commit, v1.0.0.
- Ignoring legacy records instead of migrating them was considered and does not
  work; `SPIKE.md` records why. `check` tolerates them, but `ls` exits non-zero
  and `show`/`flow`/`frontier`/`proofs` refuse them, so an exempted legacy
  record is unreadable rather than ignored.
- `tatr migrate` is a deliberate shim and the only v0-format knowledge in the
  v1 binary. 20260802-203107 removes it in v1.1.0.
- Confirmed from the code, not assumed: `contains` already serves set fields
  (tatr.c:3031), so `:gates` needs no new operator; `tatr context` phases
  (tatr.c:7121) are named independently of `FLOW STEP` and need no change;
  `check_task_is_container` (tatr.c:3606) is the shared EPIC predicate and
  stays the single exemption point.
- The 39 records are 37 `DONE`, 1 `PLANNING` and this one, so the migration is
  mechanical; 23 carry `PLAN STATUS: NOT_REQUIRED` and will migrate to no
  `PLAN` gate, which is what the historical exemptions already say about them.
- Open, and carried to the plan gate rather than assumed:
  `GATES` as one set line versus separate per-gate keys that could hold a
  value (`- REVIEW: APPROVE`); whether `flow` from `COMPOUNDING` should close
  or stop; and whether an EPIC should derive its gates from its children
  instead of keeping the four explicit exemptions. The Steps above assume the
  single set line, the folded close, and the existing exemptions.
- Half-succeeding `flow` is a real departure from the current all-or-nothing
  refusal contract. Atomicity is preserved at the write, not at the command.
