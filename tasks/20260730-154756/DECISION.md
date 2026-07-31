# Decision: Require user disposition for lesson promotions

- DATE: 20260730-233245
- STATUS: ACCEPTED
- TASK: 20260730-154756
- TAGS: flow, lessons, ledger

## Context

`check_ledger` enforced one rule: `promotion-stalled`, a lesson at `(x3)` or
more sitting outside the ledger's `## Pending promotions` section. Moving a
lesson into Pending therefore silenced it permanently - the section was a
terminus, not a queue. Nothing ever asked the user what should happen to the
lesson, and nothing connected an approved promotion to the reviewed task
lifecycle, so a promotion could rewrite AGENTS.md or a skill with no plan gate,
no review and no retro behind it.

Four forks had to be settled before any code was written, because each one
constrains the others and the wrong pick would have had to be undone:

1. Where a disposition is written in `LESSONS.md`.
2. What a PROMOTE points at, given that the three already-applied entries in
   this ledger point at docs (`PROMOTED 2026-07-05 -> AGENTS.md Code
   conventions`) and no task exists for any of them.
3. What stops a DEFER from becoming a permanent silence - the same failure this
   task exists to close, one keyword deeper.
4. The shape of the command that records a decision, given tatr has no nested
   subcommands.

## Decision

**1. Inline count annotation.** A disposition is written inside the count
parens of the entry, extending the annotation vocabulary the ledger header
already documents:

```
- `slug` (x3, PROMOTE 2026-07-30 -> 20260731-101010): <lesson text>
- `slug` (x3, DEFER 2026-07-30 at x3: <reason>): <lesson text>
- `slug` (x3, RETIRE 2026-07-30: <reason>): <lesson text>
- `slug` (x3, ABSORBED 2026-07-30 by <target>): <lesson text>
```

The ledger already carries `PROMOTED`, `absorbed by` and `RETIRED` in exactly
this position, and `promotion-stalled` already reads them there. One line per
lesson survives.

**2. A task id, validated only under `## Pending promotions`.** PROMOTE names
an existing tatr task; the promoted change is then that task's business and
inherits the ordinary plan, review, retro and close guards. Validation is
scoped to the Pending section, so entries that were decided and moved back to
their own section keep the applied markers `promotion-stalled` already exempts.
No ledger history is rewritten.

**3. A DEFER records the count it was taken at** (`DEFER <date> at x3:
<reason>`). When the lesson recurs and `/compound` bumps it to x4, the recorded
count no longer matches the live one, the deferral stops covering the entry,
and `promotion-awaiting-decision` fires again.

**4. One flat verb, `tatr ledger`.** With no `--slug` it lists every Pending
entry as `<slug><TAB>x<count><TAB><state>`. With `--slug` plus `--disposition`
and the payload that disposition requires, it records the decision. It writes
the ledger file and nothing else.

## Alternatives considered

**A separate `- DISPOSITION:` bullet under each entry** parses more easily and
extends without touching the count grammar, but it introduces a second shape
alongside annotations that already live in the parens, and breaks the ledger's
one-or-two-lines-per-lesson rule. Rejected: the ledger is read by humans far
more often than by the parser.

**Validating the new grammar everywhere in the ledger** would have forced the
three historical entries into it. None has a task to point at - they predate
the task tree - so they would have needed invented references or a dishonest
re-classification to ABSORBED. Rejected for the same reason `tasks/` records
are never rewritten: applied history is history.

**PROMOTE naming both a task and a doc target** carries more information per
line, but the doc target is exactly what the promotion task's own record is
for, and duplicating it in the ledger creates two places to drift.

**A DEFER that silences until a human edits it** was rejected as recreating the
stall behind one keyword. **A date-based revisit** (`until 2026-09-01`) would
put wall-clock time into `tatr check`, which currently has none, making every
fixture time-sensitive; the count is the better clock because the next real
occurrence is exactly when the question deserves re-asking.

**A dedicated `tatr promote` verb** reads well for PROMOTE and badly for the
other three dispositions. **`tatr ledger ls` / `tatr ledger decide`** would need
a positional mode selector, a pattern tatr has nowhere else.

## Consequences

- Pending promotions becomes a queue with an exit, not a terminus. Every entry
  in it either carries a disposition or is a finding.
- A DEFER buys quiet only until the next occurrence, so the ledger cannot
  accumulate silently-parked lessons.
- `tatr ledger` never edits the promoted tool, template, doc or skill. Applying
  a promotion is the referenced task's job and goes through the normal gates,
  which is the point: a promotion is a policy change and gets a policy
  change's review.
- tatr's own ledger fails conformance the moment this ships - its two bare
  Pending entries need real dispositions from the user, which is the intended
  first exercise of the feature rather than a regression.
- The legacy applied markers stay valid forever as an unvalidated form outside
  Pending. That asymmetry is deliberate and documented; a future ledger rewrite
  is the only thing that would remove it.
