# Retro: Replace the FLOW STEP chain with ACTIVITY, GATES and RESOLUTION

- TASK: 20260802-201918
- BRANCH: feat/activity-gates-resolution
- REVIEW ROUNDS: 2

## What went well

- The spike did its job. `SPIKE.md` settled the three-field shape and killed
  three alternatives before any code existed, so implementation argued about
  edges and defaults rather than about the design.
- `DECISION.md` was written as choices were made, not reconstructed at the end.
  Every question the plan left open - the world check scoped to the `WORKING`
  edge, `BLOCKED` absorbing every non-READY non-CLAIMED row, the close gate
  running all-or-nothing - has a record a cold reader can follow.
- The plan's `cmd:` proofs were mechanical enough to re-run unchanged in both
  review rounds. Nothing in the DoD needed interpretation to check.
- The breadth argument in TASK.md's Notes held up under review. One breaking
  change, one commit, v1.0.0: the format, the commands that write it, the lint
  that reads it and the docs that describe it genuinely could not land
  separately without a throwaway shim or a binary that refuses its own backlog.

## What went wrong

- Both round-1 findings with teeth share one root cause: a v0 construct was
  carried forward and re-keyed onto the new fields instead of re-derived from
  them.
  - R1.1 (MAJOR): the plan specified `reopen` as the inverse of one field -
    "clears `RESOLUTION` and leaves the cursor where it was" - when `close`
    writes three things. The `## Dropped` block it left behind let a close,
    reopen and re-close accumulate two reasons, of which `artifact_field` reads
    the stale one. The pair was specified as a field toggle rather than as a
    command and its inverse, so the third thing had nowhere to be noticed.
  - R1.2 (MINOR): `unplanned-in-progress` was re-pointed at the gate set, and
    its predicate thereby became a strict subset of the new `inconsistent-gates`
    rule five lines below it. Nobody asked whether the re-keyed rule still
    described a distinct fact, so one fact drew two findings and would have
    needed two exemption lines.
- Treating `NOTES.md` as an acceptance target cost more than it returned. It was
  written before the code and did not survive contact: it renames `new`'s output
  line, drops the `  - ` bullet prefix every refusal uses, and shows a message
  the review gate deliberately does not emit. Reconciling it took real effort and
  ended in `DECISION.md` recording why the sketch loses to the Steps.
- The Close-out Evidence paragraph carried two numbers the tree does not produce
  (R2.1, still open as a NIT): `111/111` after the round-1 fix had made it
  112/112, and a 21+3 exemption split where the file holds 20+4. The prose was
  written once and not re-derived after the fix commit changed what it described.

## What to improve next time

- When a schema replaces another, re-derive every rule and command that touched
  the old one from the new fields, and delete what no longer describes a
  distinct fact. Re-keying a v0 predicate onto v1 fields preserves the old
  shape's assumptions silently - which is the same failure the whole task was
  fixing at the schema level.
- Specify a command and its inverse together, by asking what the forward command
  writes rather than what the inverse clears. "`reopen` clears `RESOLUTION`" and
  "`reopen` clears what `close` wrote" read alike in a plan and differ in the
  code; only the second is a rule the pair can be stated by.
- A target transcript is worth having - it caught the two-line half-success
  reporting shape - but regenerate it from the built binary once the code exists
  instead of treating the pre-code sketch as a fixture.
- Re-derive close-out numbers after the last commit that changes them. Evidence
  prose written before the review fixes describes a tree that no longer exists.

## Action items

- [ ] R2.1 is open as a NIT: correct TASK.md:283 to `112/112` and lines 287-288
      to "20 whole-task entries plus 4 narrow ones". Carry it into the landing
      commit or the next touch of the record.
- [ ] 20260802-203107 already exists to remove `tatr migrate` in v1.1.0, which
      retires the only v0-format knowledge left in the binary.
- [ ] File a task for the pre-existing argparse crash round 1 found and
      correctly excluded from its findings: any `tatr` subcommand given an
      unknown option aborts (`free(): invalid pointer` or SIGSEGV) after
      argparse prints `Error: unknown argument`. A `master` build fails
      identically, so it predates this diff.

## Process and context

- No `Process signal:` bullets were raised in either round, and no checkpoint
  was taken; the implementation ran to REVIEWING in one pass.
- Two observed context boundaries, both deliberate: round 1 was delegated to an
  out-of-context reviewer, and round 2 ran in a fresh `/flow` session that
  started at REVIEWING and read the branch off disk. The second re-derived R1.1,
  the rewind clear table and the README transcripts by hand rather than off the
  tests, which is what the fresh context was for. No compaction warning or
  threshold crossing was recorded in either.
