# Spike: Replace the FLOW STEP chain with ACTIVITY, GATES and RESOLUTION

- DATE: 20260802-201921
- STATUS: RECOMMENDED
- TAGS: lifecycle, schema, breaking

## Question

`FLOW STEP` is a single linear chain
(`BACKLOG -> UNDERSTANDING -> PLANNING -> PLANNED -> WORKING -> REVIEWING ->
COMPOUNDING -> DONE`) that encodes both where attention currently sits and what
has been proven about the record. `PLANNED` reads as a status rather than a
phase, and `PLAN STATUS` duplicates a fact the chain position already implies.
Should the chain be reshaped, and into what?

## Context

The chain conflates two things with different natures:

| Job | Nature |
|---|---|
| where attention sits | a cursor; non-monotone; should move backward freely; no gates |
| what has been proven | an accumulating fact set; monotone within a pass; gated |

One linear order can carry both only while position implies proof. That holds
until the record must move backward, which is why the machine has exactly one
backward edge (`REVIEWING -> WORKING`, tatr.c:5270) and no edge back to
`PLANNING` at all. The absence is structural, not an oversight: a chain cannot
represent "we reviewed, found the plan wrong, replanned, and the old review no
longer counts."

Two more symptoms of the same conflation:

- `PLAN STATUS` is derivable (`flow_step >= PLANNED`) yet stored, so a
  hand-edited record can hold `WORKING` with `DRAFT`. Its `NOT_REQUIRED` value
  exists only for pre-flow history and documents itself as needing manual
  repair (tatr.c:180, lifecycle.md:34).
- `DONE` and `DROPPED` are chain nodes, so every closure reason a project might
  want (duplicate, superseded, obsolete) would have to become another node in a
  graph whose gate tables grow a row each time.

Jira's model separates the same axes: status (workflow node), status category
(fixed 3-valued rollup), and resolution (a field holding why it stopped). Its
default Scrum workflow contains tatr's `PLANNED` verbatim, as
`Selected for Development` in the To Do category, so the node is not itself an
error. A GitHub pull request is the closer analogue: state plus independently
accumulated reviews and checks, and no phase at all.

## Options considered

1. Keep the chain, rename `PLANNED` to `READY`, delete `PLAN STATUS`. Cheapest.
   Fixes the naming (every other node is a gerund naming an activity; `PLANNED`
   is a past participle naming a property) and the redundant field. Leaves the
   rework problem and the closure-reason problem untouched.
2. Collapse `PLANNING -> WORKING` and drop `PLANNED`. Smallest chain, but loses
   the ready queue `tatr frontier` is built on, and forces dependencies closed
   before a plan may be approved, since both checks would land on one edge.
3. Two axes, no chain: an `ACTIVITY` cursor plus independent boolean flags.
   Most expressive, but bare `tatr flow` needs a total order to advance along,
   so the policy table returns as an implicit chain, badly.
4. Cursor plus ordered gate set plus resolution. `ACTIVITY` is a freely moving
   cursor over five activities; `GATES` is an ordered set of earned facts;
   `RESOLUTION` records why work stopped; `STATUS` becomes derived and unstored.

## Recommendation

Option 4.

Fields, three stored and one derived:

```
- ACTIVITY:   - | UNDERSTANDING | PLANNING | WORKING | REVIEWING | COMPOUNDING
- GATES:      PLAN REVIEW RETRO           (ordered set, may be empty)
- RESOLUTION: - | DONE | WONTDO | DUPLICATE | SUPERSEDED
```

| Condition | Derived STATUS |
|---|---|
| `RESOLUTION` set | CLOSED |
| `ACTIVITY` unset | OPEN |
| otherwise | IN_PROGRESS |

Three of the eight chain nodes dissolve into other axes: `BACKLOG` is the
absence of an activity, `DONE` is `RESOLUTION: DONE`, `DROPPED` is
`RESOLUTION: WONTDO`. `PLANNED` becomes a query rather than a node:

```
READY == GATES contains PLAN and ACTIVITY < WORKING and deps CLOSED and unclaimed
```

Gate invalidation is the load-bearing rule. Each gate is produced by leaving a
specific activity, so moving the cursor backward to activity A clears every
gate produced at or after A:

| Rewind to | Clears | Why |
|---|---|---|
| UNDERSTANDING | PLAN, REVIEW, RETRO | everything is up for grabs |
| PLANNING | PLAN, REVIEW, RETRO | replanning invalidates the review |
| WORKING | REVIEW, RETRO | the current fix loop; `PLAN` survives |
| REVIEWING | REVIEW, RETRO | re-reviewing discards the old verdict |

Forward movement gets exactly one door so gates cannot be skipped:

| Command | Does | Gates |
|---|---|---|
| `tatr flow <id>` | advance one activity | runs the edge's gate, records it; sole writer of `GATES`; forward only |
| `tatr rewind <id> --to <ACTIVITY>` | move backward | no gate; clears per the table; prints what it cleared |
| `tatr close <id> --resolution <R>` | set `RESOLUTION` | `DONE` runs the close gate; the others skip it |
| `tatr reopen <id>` | clear `RESOLUTION` | restores the prior cursor |

`tatr flow` may half-succeed: when the record gate passes but the world is not
ready (an open dependency, a foreign claim), it records `PLAN` and holds the
cursor at `PLANNING`, reporting both halves. This is the case the redesign
exists for. Under the chain, approving a plan and starting the work were one
edge, so a task could not have its plan blessed until its dependencies closed.
Separating the axes lets a fact about the record and a fact about the world
disagree, which is what a blocked-but-planned task is.

`PLAN STATUS` returns as the `PLAN` member of `GATES`, and this time it is
load-bearing rather than redundant: once the cursor moves freely, approval is
no longer derivable from position. `NOT_REQUIRED` does not return; a legacy
record simply carries no `PLAN` gate.

`tatr frontier` keeps its output. `READY`/`BLOCKED`/`CLAIMED` become the query
above rather than a filter over chain positions.

## Open questions

- `GATES` as one ordered set line, or separate `- PLAN:` / `- REVIEW:` /
  `- RETRO:` keys? The set matches the invalidation rule, which operates on it
  as a set; separate keys are more greppable and could carry a value
  (`- REVIEW: APPROVE`) rather than bare presence. Either way the filter needs
  no new operator: `contains` already serves set fields (tatr.c:3031).
- Should `tatr flow` from `COMPOUNDING` earn `RETRO` and close with
  `RESOLUTION: DONE` in one motion, or earn `RETRO` and stop, leaving closure
  to an explicit `tatr close`? Folding it matches today's
  `COMPOUNDING -> DONE` edge; splitting it gives the one irreversible operation
  its own verb.
- Should an EPIC derive its gates from its children, or keep the four explicit
  exemptions in `check_task_is_container`? Deriving deletes the exemption block
  but makes an Epic record non-self-describing.
- Does `frontier`'s activity column stay useful once `PLANNING` no longer
  distinguishes drafting from blessed, or should it print gates
  (`PLANNING+PLAN`) or drop the column?
- Half-succeeding `tatr flow` departs from the current all-or-nothing refusal
  contract. The write stays a single `task_save`, so no state is left
  inconsistent, but the reporting contract in `flow_unmet_add_problems` needs a
  shape for "recorded this, held that".

## Migration and release

The break ships as v1.0.0. Ignoring legacy records instead of migrating them
was considered and does not work: `check_task` degrades gracefully and reports
`malformed-header` through `check_finding` (tatr.c:6007), so an unparseable
record is already exemptable, but `ls` skips malformed records and exits
non-zero, and `show`, `flow`, `frontier` and `proofs` refuse them outright. An
exempted legacy record is not ignored, it is unreadable, and `tatr ls` would be
permanently non-zero in any repository holding one.

The migration is therefore total, but it moves metadata headers only. No
`REVIEW.md` or `RETRO.md` body is rewritten, which keeps the append-only
principle `tasks/EXEMPTIONS.md` already states: a record is not rewritten to
satisfy a rule invented after it landed. A schema version bump is a different
thing from backfilling history.

The mapping is record-local - no cross-record or cross-repository knowledge -
so one implementation serves every backlog:

| v0 | v1 |
|---|---|
| `- STATUS: <any>` | dropped, derived |
| `- FLOW STEP: BACKLOG` | `ACTIVITY: -` |
| `- FLOW STEP: PLANNED` | `ACTIVITY: PLANNING` + `PLAN` gate |
| `- FLOW STEP: DONE` | `ACTIVITY: COMPOUNDING` + `RESOLUTION: DONE` |
| `- FLOW STEP: DROPPED` | `RESOLUTION: WONTDO`, keeping `- REASON: ` |
| other steps | same name |
| `- PLAN STATUS: APPROVED` | `PLAN` gate |
| `REVIEW.md` present, latest APPROVE | `REVIEW` gate |
| `RETRO.md` present | `RETRO` gate |

It ships as `tatr migrate [--apply]` rather than a shell script: it is at hand
in every repository without a tatr checkout, it reuses the binary's HUID and
path handling instead of running sed over Markdown, and it earns real coverage
in `checker.sh` alongside the rest of the suite. The cost is that v1.0.0
carries v0 format knowledge in one quarantined command, which 20260802-203107
removes in v1.1.0.

Exemptions gain a whole-task form, `- <task-id>: <reason>`, which is
independently useful: it collapses this repository's 37 per-rule lines over 21
tasks and covers historical records whose record bodies cannot satisfy the
current schema without fabrication.

## Next steps

- 20260802-201918 carries the implementation; see its `## Steps`.
- 20260802-203107 removes `tatr migrate` in v1.1.0 once every backlog is
  migrated.
- The worked example of a record moving through the new machine is in
  `NOTES.md` beside this file.
