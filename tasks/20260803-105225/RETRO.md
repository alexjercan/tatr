# Retro: Make tatr flow --dry-run a real precondition probe

- TASK: 20260803-105225
- BRANCH: feat/flow-dry-run-probe
- REVIEW ROUNDS: 1

## What went well

- The plan named the `fflush` concern, and the Steps demanded sabotage of every
  assertion. That pairing is what caught the vacuous ordering check: the test as
  first written stayed 113/113 green with the `fflush` deleted.
- Structuring the dry-run exit *before* `task.meta.gates |= ...` rather than
  sharing the real report's message code made "no probe reaches `task_save`" a
  property of control flow instead of a promise each branch keeps. Two duplicated
  log lines bought a guarantee that cannot silently rot.
- DECISION.md settled the exit-status shape before implementation, so the review
  had nothing to relitigate about the interface - both findings were about
  assertion strength, not design.

## What went wrong

- Both round-1 findings (R1.1, R1.2) were assertion-strength gaps in the *new*
  test, and both were the same failure: the test asserted that a substring
  appears somewhere, not that the exact line appears and nothing else does.
  `grep -q` for a header pins neither the `Task <id> ` prefix nor the two-space
  gate indent; four present-line greps do not mean "only these lines".
- The repo already encodes the fix. `AGENTS.md:38` says "write the exact-message
  assertion first; use `grep -qx`", and the neighbouring test at checker.sh:1587
  already did. The rule was available and was not applied to the new test.
- The review returned APPROVE with both items' `Response:` fields empty, so the
  branch reached COMPOUNDING carrying a known, named rule violation. A
  non-blocking verdict is not a disposition; nothing in the flow forced one
  before the retro.

## What to improve next time

- Breadth: the diff is small (six files, +238/-26) and every file traces to a
  Step. No split was missed; the doc and skill surfaces are the same change
  described three times, which is the cost of a user-facing contract change.
- Churn: one round, both findings mechanical. The plan-time question that would
  have prevented them is not the from-scratch challenge - the design was right -
  but a checklist question the plan skipped: for each new assertion, what would
  still pass that should not? Applied to the header greps it yields `grep -qx`
  immediately; applied to the clear case it yields the line count.
- Context: no measured pressure. No compaction warning, no checkpoint, no
  delegation, no handoff. One worktree, one focused pass.

## Action items

- Applied R1.1 and R1.2 during COMPOUNDING rather than deferring: three header
  grep pairs and two `head -1` ordering checks moved to `grep -qx` with the full
  expected line, and the clear case gained
  `[ "$(printf '%s\n' "$clear_out" | wc -l)" -eq 2 ]`. Sabotage confirms the
  count assertion is not vacuous (extra `printf` in the header block -> 112/113,
  new test alone red). Suite 113/113 under `--memcheck`.
- Scope note: checker.sh:1481, the dry-run assertion in the existing forward
  walk, still uses the loose `grep -q` form. It is outside R1.1's stated scope
  and its own Step only required capturing exit status. Left as-is deliberately
  rather than widened without review.
- Process signal, not yet reusable enough to promote: an APPROVE verdict with
  unresolved finding checkboxes lets a named rule violation pass a gate. Whether
  `tatr check` should refuse a record whose latest review round has an unticked
  finding is a real question, but it is one observation, not a pattern; it stays
  here until a second task hits it.
