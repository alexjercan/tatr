# Decision: Replace the FLOW STEP chain with ACTIVITY, GATES and RESOLUTION

- DATE: 20260802-212128
- STATUS: ACCEPTED
- TASK: 20260802-201918
- TAGS: lifecycle, schema, breaking

## Context

`SPIKE.md` chose the three-field shape and `NOTES.md` sketched the target
transcript, but several load-bearing choices were left open and had to be made
during implementation. They are recorded here rather than in the Steps because
they are the parts a future reader would otherwise have to reverse-engineer
from the code.

## Decision

**The world check applies only to the edge into `WORKING`.** The plan says
"advance the cursor only when the world permits it - dependencies CLOSED, no
foreign claim". Read generally, a blocked task could not even move from
`UNDERSTANDING` to `PLANNING`, which is exactly the planning-around-a-blocker
case the redesign exists to enable. Entering `WORKING` is the edge that means
"start", so it is the edge that asks anything of anyone else.

**`READY` is only reachable after a hold.** An unblocked task earns `PLAN` and
advances to `WORKING` in the same command, so it is never `PLANNING+PLAN`. The
state the ready query names is produced by a `flow` that half-succeeded - an
open dependency or a foreign claim - and becomes `READY` once that clears. That
is what an Epic frontier is for, and it is why `checker.sh`'s `PLANNED` fixture
takes a foreign claim rather than trying to stop mid-edge.

**`BLOCKED` absorbs everything that is not `READY` or `CLAIMED`.** The three
frontier states are fixed and their order is byte-for-byte. A child that never
earned `PLAN`, or whose cursor already entered `WORKING`, is open work you
cannot pick up, so it sorts with the blocked rows; the `blocked-by` column is
omitted rather than printed empty when dependencies are not the reason.

**The close gate runs with the RETRO gate, all-or-nothing.** `flow` out of
`COMPOUNDING` runs both the `RETRO` gate and the close gate before writing
anything. Half-succeeding is reserved for world preconditions; a close gate
failure is a fact about the record, so it refuses flat and writes nothing.

**`WONTDO` still requires `--reason`.** The Story asks for a task to be
"abandoned with a reason", and the v0 `DROPPED` edge required one. `close`
keeps the requirement and keeps writing the same `## Dropped` / `- REASON: `
body, so `tatr migrate` moves headers only and the `dropped-missing-reason`
rule keeps its meaning.

**`rewind` and `close` refuse a record that is already closed.** Neither is
specified to, but moving the cursor of a closed record or closing it twice are
both meaningless, and `reopen` is the verb that exists for it. Silently
accepting either would let a record drift in a way nothing reports.

**`reopen` clears everything `close` wrote, including the `## Dropped` block.**
Added in review round 1. `close` writes three things: the resolution, the
`- DUPLICATE OF: ` pointer and, for `WONTDO`, a `## Dropped` / `- REASON: `
block. `reopen` already cleared the first two, so leaving the third behind let
a close, reopen and re-close accumulate two `## Dropped` sections, of which
`artifact_field` reads the stale one. Clearing it on the `reopen` side rather
than stripping it on the `close` side is what makes the rule statable - `close`
sets the closure facts, `reopen` clears them - and it also covers the
`WONTDO` -> `reopen` -> `DUPLICATE` path, where `close` never reaches its
append. Only a `## Dropped` block that runs to the end of the record is cut, so
an author's own mid-record section is untouched.

**`inconsistent-gates` owns the missing-plan fact alone.** Added in review
round 1. `unplanned-in-progress` was carried forward from v0 and re-keyed onto
the gate set, but its predicate became a strict subset of the new rule's, so
the two only ever fired together on the same fact and a repository accepting it
had to write two exemption lines. The v0 rule is deleted. Its one behavioral
difference - a `!has_resolution` guard `inconsistent-gates` lacks - was already
inert, because `inconsistent-gates` flagged closed records regardless.

**`SUPERSEDED` reuses `- DUPLICATE OF: `.** Confirmed in review round 1. One
pointer field serves both pointing resolutions and `RESOLUTION` says which
relation it records, so a second field would carry no information the pair does
not already. The spelling is frozen by v1.0.0 and `README.md` says so.

**`show` prints the derived STATUS on the path line, not in the body.**
`task_print_full` prints exactly what would be written back, which is what
keeps `show` from drifting from the serializer. A `- STATUS: ` line in that
block would be a field the format no longer has, so the derived value rides on
the header line as `(STATUS: ...)`.

## Alternatives considered

- **Applying the world check to every forward edge.** Rejected: it reproduces
  the exact coupling the redesign removes.
- **A fourth frontier state for "started" or "unplanned".** Rejected: the plan
  pins the three states and their ordering, and a fourth would change the
  output contract for every existing caller.
- **Matching `NOTES.md`'s transcript byte for byte.** Rejected as an
  implementation target. It is a sketch written before the code: it renames
  `new`'s output line, drops the `  - ` bullet prefix every refusal uses, and
  shows a separate "1 open MAJOR finding" message that the review gate
  deliberately does not emit (the verdict check already covers it, and the code
  says so). The Steps are authority; the transcript's shape - the gate line,
  the two-line half-success report, the move line - is implemented, and
  `README.md` carries transcripts pasted from real runs instead.
- **Leaving `NOT_REQUIRED` as a gate value.** Rejected by the spike, and
  nothing in implementation argued for it: a record that never earned a plan
  carries no `PLAN` gate, which is the same fact with one less spelling.

## Consequences

- 21 of this repository's 39 records trip `inconsistent-gates` after migration,
  because `PLAN STATUS: NOT_REQUIRED` correctly becomes no `PLAN` gate while
  the cursor sits at `COMPOUNDING`. The drift is real and historical, so it is
  classified in `tasks/EXEMPTIONS.md` rather than papered over - which is what
  motivated the whole-task exemption form in the first place.
- The task's doc-sweep proof was too broad as written: it swept `*.sh` and
  every `*.md`, so `checker.sh` (which must test the v0 refusal) and
  `CHANGELOG.md` (which must name the removal) could never satisfy it. It is
  narrowed to the doc and skill surfaces, with the `### Migrating v0 Records`
  section excluded, since naming the v0 tokens is that section's job.
- `tatr flow` can now exit non-zero after writing. Callers that treated a
  non-zero `flow` as "nothing happened" must re-read the record; the two-line
  report says which half landed.
