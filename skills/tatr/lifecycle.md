# Lifecycle

Three independent fields, not one chain.

| Field | Meaning |
|---|---|
| ACTIVITY | Where attention sits. Nullable; moves freely. Proves nothing. |
| GATES | What has been proven. Accumulating set. `flow` is its sole writer. |
| RESOLUTION | Why work stopped. Nullable; terminal until `reopen`. |

```text
ACTIVITY:   - -> UNDERSTANDING -> PLANNING -> WORKING -> REVIEWING -> COMPOUNDING
GATES:                                PLAN         REVIEW        RETRO
RESOLUTION: - | DONE | WONTDO | DUPLICATE | SUPERSEDED
```

## Commands

| Command | Does | Gates |
|---|---|---|
| `tatr flow <id>` | Advance one activity. `--dry-run` probes it: same preconditions, writes nothing, non-zero when the advance would not complete. | Runs and records the current activity's exit gate; forward only. |
| `tatr rewind <id> --to <ACTIVITY> [--force]` | Move backward. | None. Clears per the table below and names each one. |
| `tatr close <id> --resolution <R> [--of <ID>] [--reason <text>]` | Set RESOLUTION. | `DONE` runs the close gate; the others run nothing. |
| `tatr reopen <id>` | Clear RESOLUTION, `- DUPLICATE OF:` and a trailing `## Dropped` block. | None. Cursor and gates stay. |

- `flow` out of `COMPOUNDING` earns `RETRO` and closes as `DONE` in one motion.
- `flow` refuses to run at all once a RESOLUTION is set.
- `WONTDO` requires `--reason`; `DUPLICATE` and `SUPERSEDED` require `--of`.

## Derived status

| Condition | Status |
|---|---|
| RESOLUTION set | CLOSED |
| ACTIVITY unset | OPEN |
| otherwise | IN_PROGRESS |

Never stored. Every command that reports prints it.

## Exit gates

| Leaving | Earns | Requires |
|---|---|---|
| UNDERSTANDING | - | Schema-clean `DECISION.md`. No gate records it, so the edge asks again every time. |
| PLANNING | PLAN | `## Steps` and `## Definition of Done` well-formed, DoD proofs parseable, graph position resolvable. |
| WORKING | - | - |
| REVIEWING | REVIEW | Schema-clean `REVIEW.md`; latest verdict APPROVE; no open BLOCKER or MAJOR. |
| COMPOUNDING | RETRO | Schema-clean `RETRO.md`, plus the close gate below. |

Close gate (`DONE` only): all three gates earned, no unchecked `## Steps`, valid
`DECISION.md` when present, every child CLOSED.

## World preconditions

Entering `WORKING` also needs dependencies CLOSED and no foreign claim. These
are facts about the world, not the record, so `flow` may half-succeed: it
records the gate, holds the cursor, and reports both halves in one write. That
is what a planned-but-blocked task is.

## Rewind clear table

| Rewind to | Clears | Why |
|---|---|---|
| UNDERSTANDING | PLAN, REVIEW, RETRO | Everything is up for grabs. |
| PLANNING | PLAN, REVIEW, RETRO | Replanning invalidates the review. |
| WORKING | REVIEW, RETRO | The fix loop; PLAN survives. |
| REVIEWING | REVIEW, RETRO | Re-reviewing discards the old verdict. |

`--force` is required when the record actually carries a gate being cleared.

## Ready queue

`PLANNED` is a query, not a state:

```text
READY == GATES contains PLAN and ACTIVITY < WORKING and deps CLOSED and unclaimed
```

`tatr frontier <id>` answers it.

## Containers

No exemptions. A task others name as PARENT owes the same records as any task,
plus one more rule: it cannot close while a child is open. Waivers go in
`tasks/EXEMPTIONS.md`, per rule, per task.
