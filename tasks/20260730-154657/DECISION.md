# Decision: one guarded `tatr flow` verb owns every workflow field

- DATE: 20260730-182735
- STATUS: ACCEPTED
- TASK: 20260730-154657
- TAGS: decision,flow,lifecycle

## Context

The v2 schema (20260730-153325) made workflow state typed, but nothing
constrains how it moves. `tatr edit -f DONE -s CLOSED` writes a closed task
with no review, no retro and unchecked Steps; `tatr new -s CLOSED` seeds one
outright. `tatr check` catches the wreckage afterwards, which means the
process discipline still lives in the prompt: an agent that skips the plan
gate or the review loop produces a knowingly invalid record and only finds out
at the next lint.

Four load-bearing forks were confirmed with the user before planning:

1. The command surface for transitions.
2. What `tatr edit -s/-f/-S` does once the lifecycle exists.
3. Whether there is an in-tool escape hatch for a wrong state.
4. Whether `tatr new` may seed a task into an arbitrary flow state.

## Decision

**One verb.** `tatr flow <id> [--to <STEP>]` is the only writer of `STATUS`,
`FLOW STEP` and `PLAN STATUS`. Without `--to` it advances one step along the
chain; with `--to` it names the target explicitly, which is how the
`REVIEWING -> WORKING` fix loop is expressed.

**STATUS is derived, never chosen.** The flow step implies it:

| FLOW STEP                                  | STATUS      |
| ------------------------------------------ | ----------- |
| BACKLOG, UNDERSTANDING, PLANNING, PLANNED   | OPEN        |
| WORKING, REVIEWING, COMPOUNDING             | IN_PROGRESS |
| DONE                                        | CLOSED      |

A work task therefore stays IN_PROGRESS through review and compound and closes
atomically at DONE, in the same single write that sets `FLOW STEP: DONE`.

**The transition table.** Eight edges, and nothing else:

```
BACKLOG -> UNDERSTANDING -> PLANNING -> PLANNED -> WORKING -> REVIEWING
                                                     ^           |
                                                     |           v
                                                     +-- (fix) COMPOUNDING -> DONE
```

`REVIEWING` is the only step with two successors; its default (bare
`tatr flow`) is `COMPOUNDING`, and the fix loop back to `WORKING` must be
asked for by name. Every other step has exactly one successor, so the bare
form is unambiguous.

**Preconditions, per edge.**

- `PLANNING -> PLANNED` is the plan gate: it sets `PLAN STATUS: APPROVED` as
  its effect. It is the only way that value is ever written.
- `PLANNED -> WORKING` requires `PLAN STATUS: APPROVED` and every `DEPENDS ON`
  id to resolve to an existing task that is `CLOSED`.
- `REVIEWING -> COMPOUNDING` requires a `REVIEW.md` whose latest `- VERDICT:`
  is `APPROVE`, with no unchecked `BLOCKER` or `MAJOR` finding left open.
- `COMPOUNDING -> DONE` requires all of the above, plus zero unchecked items
  under `## Steps`, a `RETRO.md`, and a valid `- STATUS:` on a `DECISION.md`
  when the task carries one.
- `REVIEWING -> WORKING` and the three pre-plan edges carry no preconditions.

**`KIND: EPIC` is the only exempt kind**, and it is exempt from exactly what
it is already exempt from in `tatr check`: the plan-approval requirement, the
review requirement, the retro requirement and the unchecked-Steps requirement.
`SPIKE` and `STORY` get no exemption, because `tatr check` gives them none.

**The tying invariant: a transition may never produce a state `tatr check`
would flag.** The guards are built from the same primitives the lint reads
with - the Steps scan, the verdict scan, the DECISION status scan - extracted
into shared helpers rather than re-implemented, so the two cannot drift into
disagreeing about the same bytes.

**`new` and `edit` lose the workflow flags.** `-s/--status` is removed from
both, and `-f/--flow-step` and `-S/--plan-status` are removed from the shared
v2 metadata options. A new task is always born `BACKLOG` / `DRAFT` / `OPEN`.
`edit` keeps `-T`, `-p`, `-t`, `-k`, `-P` and `-d`: kind and relationships are
not lifecycle state. Because argparse rejects an unknown option generically,
`new` and `edit` scan for the three retired spellings first and fail with a
pointer to `tatr flow`.

**No escape hatch.** There is no `--force` and no `repair` command. A record
in the wrong state is corrected by hand in `TASK.md`, where the fix shows up
in the diff and a reviewer sees it.

**Failure is atomic.** Every precondition is evaluated before anything is
mutated, all unmet ones are reported (not just the first), and `task_save` is
reached only when the transition is legal and fully satisfied. A refused
transition leaves `TASK.md` byte-identical.

## Alternatives considered

- **Four verbs (`approve` / `start` / `advance` / `close`)** - the shape the
  task's Steps sketched, rejected by the user in favour of one verb. It reads
  better at a call site but multiplies help entries and splits one transition
  table across four commands, each needing its own argument plumbing.
- **A verb per edge (`understand`, `plan`, `review`, `rework`, ...)** -
  rejected: most discoverable, but eight commands for eight edges puts the
  state machine in the dispatch chain instead of in one table.
- **Keep `edit -s/-f/-S` and route it through the same guards** - the user's
  first answer, narrowed on the follow-up to removing them. Two entry points
  to one guard set means two places to test and two places to drift; and the
  paired-field ergonomics (`-f WORKING -s IN_PROGRESS`) exist only because
  STATUS was settable, which it no longer is.
- **`tatr new` may seed any state, guarded only for internal consistency** -
  rejected by the user. Consistency alone still lets a record be born at DONE
  with no review; and every state above BACKLOG is reachable by walking the
  chain, which is what fixtures should exercise anyway.
- **`--force` on the transition, or a `tatr repair` command** - rejected. It
  is a guard the agent can reach for unilaterally, which is the exact failure
  this task exists to prevent. Hand correction is consistent with the v2
  decision to ship no migration mode.

## Consequences

- Breaking: `tatr new -s`, `tatr edit -s`, and `-f`/`-S` on either command are
  gone. README, AGENTS.md and `skills/tatr/SKILL.md` all document
  `tatr edit <ID> -s IN_PROGRESS` as the way to claim a task and must be
  rewritten around `tatr flow`.
- `checker.sh` fixtures that seeded a state with `-s`/`-f`/`-S` now drive the
  lifecycle to get there. A fixture wanting a CLOSED task must scaffold the
  `REVIEW.md`, `RETRO.md` and ticked Steps a close requires, via a shared
  helper. That is a real cost and it is the point: the suite now exercises the
  guards on the way to every fixture.
- `PLAN STATUS: NOT_REQUIRED` becomes unreachable through the CLI. It stays a
  legal parsed value - it is how the 23 pre-flow records say their cycle
  predated plan state - and is written only by hand, which is what a record of
  history should require.
- The DoD item "`tatr edit -s` cannot bypass the lifecycle" is now satisfied by
  removal rather than by shared validation. The test keeps its planned name,
  `test_edit_status_uses_transition_guards`; what it pins is the absence of the
  bypass, not the mechanism.
- Relationship *semantics* stay out of scope, as they did in 20260730-153325:
  the dependency guard resolves and reads each `DEPENDS ON` task, but cycles,
  parent/child consistency and the Epic frontier remain 20260730-154740's job.
