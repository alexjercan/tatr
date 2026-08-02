# Notes: a task through the ACTIVITY / GATES / RESOLUTION machine

The target behaviour, written as the transcript of one small task,
`20260802-090000`. Full record at the ends, header diff in between. This is the
acceptance shape: the implementation is right when this transcript is real.

## 0. Created

```console
$ tatr new "Add --json output to tatr ls" -p 60 -t feature,cli
Task 20260802-090000 created (STATUS: OPEN)
```

```markdown
# Add --json output to tatr ls

- PRIORITY: 60
- TAGS: feature, cli
- KIND: TASK
- ACTIVITY: -
- GATES: -
- RESOLUTION: -
```

No `STATUS` line. It is derived (`RESOLUTION` set -> CLOSED; no `ACTIVITY` ->
OPEN; else IN_PROGRESS) and printed by every command that reports. Storing it
is what lets it drift from the fields it summarizes.

## 1. Pick it up

```console
$ tatr flow 20260802-090000
20260802-090000  - -> UNDERSTANDING  (STATUS: IN_PROGRESS)
```

```diff
-- ACTIVITY: -
+- ACTIVITY: UNDERSTANDING
```

## 2. Start planning

```console
$ tatr flow 20260802-090000
20260802-090000  UNDERSTANDING -> PLANNING
```

```diff
-- ACTIVITY: UNDERSTANDING
+- ACTIVITY: PLANNING
```

No gate on this edge. The agent now writes `## Steps` and
`## Definition of Done`.

## 3. Plan approved

The user says the plan is good. That sentence is the whole trigger:

```console
$ tatr flow 20260802-090000
gate PLAN recorded
20260802-090000  PLANNING -> WORKING
```

```diff
-- ACTIVITY: PLANNING
+- ACTIVITY: WORKING
-- GATES: -
+- GATES: PLAN
```

The gate that ran is today's record gate: `## Steps` and
`## Definition of Done` present and well-formed, DoD proofs parseable, any
`SPIKE.md` schema-clean. Those sections become required by lint from the moment
`PLAN` is in `GATES`, not from a position in a chain.

## 4. Work, then hand off to review

The agent implements and ticks `## Steps`.

```console
$ tatr flow 20260802-090000
20260802-090000  WORKING -> REVIEWING
```

```diff
-- ACTIVITY: WORKING
+- ACTIVITY: REVIEWING
```

No gate. Going to review costs nothing; leaving it is what is expensive.

## 5. Review requests changes

The reviewer writes `REVIEW.md` with a MAJOR finding and
`- VERDICT: REQUEST_CHANGES`.

```console
$ tatr flow 20260802-090000
ERROR: Refusing to advance 20260802-090000 from REVIEWING: 2 precondition(s) not met
  the latest REVIEW.md verdict is REQUEST_CHANGES, not APPROVE
  1 open MAJOR finding
Record unchanged.
```

Back to fixing:

```console
$ tatr rewind 20260802-090000 --to WORKING
20260802-090000  REVIEWING -> WORKING  (no gates cleared)
```

```diff
-- ACTIVITY: REVIEWING
+- ACTIVITY: WORKING
```

`GATES` is untouched because `REVIEW` was never earned. `PLAN` survives:
rewinding to `WORKING` clears only `REVIEW` and `RETRO`, and there was nothing
to clear. Fix, then `tatr flow` back to `REVIEWING`.

## 6. Review approves

`REVIEW.md` now ends `- VERDICT: APPROVE` with no open BLOCKER or MAJOR.

```console
$ tatr flow 20260802-090000
gate REVIEW recorded
20260802-090000  REVIEWING -> COMPOUNDING
```

```diff
-- ACTIVITY: REVIEWING
+- ACTIVITY: COMPOUNDING
-- GATES: PLAN
+- GATES: PLAN REVIEW
```

## 7. Retro and close

The agent writes `RETRO.md`.

```console
$ tatr flow 20260802-090000
gate RETRO recorded
20260802-090000  COMPOUNDING -> CLOSED  (RESOLUTION: DONE)
```

Final record:

```markdown
# Add --json output to tatr ls

- PRIORITY: 60
- TAGS: feature, cli
- KIND: TASK
- ACTIVITY: COMPOUNDING
- GATES: PLAN REVIEW RETRO
- RESOLUTION: DONE

## Story

As a script author, I want `tatr ls --json`, so I can filter the backlog
without parsing tab-separated output.

## Steps

- [x] Add `--json` to the `ls` parser.
- [x] Emit one object per task with the metadata fields and huid.
- [x] Cover the flag in checker.sh.

## Definition of Done

- `tatr ls --json` emits valid JSON for an empty and a populated backlog
  (test: `test_ls_json_output`).
- Malformed records still land on stderr and still exit non-zero
  (test: `test_ls_json_skips_malformed`).
- README documents the flag from real output (manual: transcript pasted).
```

`ACTIVITY` stays at `COMPOUNDING` rather than resetting. The cursor records
where the work ended; `RESOLUTION` records that it ended. Reopening restores a
live cursor without having to invent a position.

## The blocked variant

The same task, but with `- DEPENDS ON: 20260802-085000` still open at step 3:

```console
$ tatr flow 20260802-090000
gate PLAN recorded
Not advancing to WORKING: 1 precondition(s) not met
  dependency 20260802-085000 is not CLOSED
```

```diff
-- GATES: -
+- GATES: PLAN
```

`ACTIVITY` stays `PLANNING`. This is the case that decides the whole design:
the plan is a fact about the record, the advance is a fact about the world, and
they are allowed to disagree. The chain had to refuse both together, which is
why approving a plan required its dependencies to be finished first.

This is also where `PLANNED` went. Not deleted, derived:

```
READY == GATES contains PLAN and ACTIVITY < WORKING and deps CLOSED and unclaimed
```

`tatr frontier` prints what it prints today:

```console
$ tatr frontier 20260802-080000
READY	20260802-090005	p90	PLANNING	Ready, high priority
BLOCKED	20260802-090000	p60	PLANNING	Add --json output to tatr ls	blocked-by=20260802-085000
CLAIMED	20260802-090002	p70	WORKING	Someone has it
```

The activity column is less informative than `PLANNED` was, since `PLANNING` no
longer distinguishes drafting from blessed. Either print the gates
(`PLANNING+PLAN`) or drop the column, since `READY`/`BLOCKED`/`CLAIMED` already
carries the decision-relevant bit. Open question in `SPIKE.md`.

## Abandoning

```console
$ tatr close 20260802-090000 --resolution DUPLICATE --of 20260802-091000
20260802-090000  CLOSED  (RESOLUTION: DUPLICATE, of 20260802-091000)
```

```diff
-- RESOLUTION: -
+- RESOLUTION: DUPLICATE
+- DUPLICATE OF: 20260802-091000
```

Available from any activity, no gates: anything can be abandoned at any time.
Only `--resolution DONE` runs the close gate (all three gates earned, no
unchecked Steps, valid `DECISION.md` when present), which is why the happy path
folds it into `tatr flow`.

## Two things the transcript exposed

1. `tatr flow` from `COMPOUNDING` both earns `RETRO` and closes. Convenient,
   and it matches today's `COMPOUNDING -> DONE` edge, but the one irreversible
   operation then has no dedicated verb on the happy path.
2. `tatr flow` can half-succeed (gate recorded, cursor held). That is a real
   departure from the current all-or-nothing refusal contract. The write is
   still one atomic `task_save`, so nothing is left inconsistent, but the
   output has to be unambiguous about which half happened, hence the two-line
   report rather than a single status line.
