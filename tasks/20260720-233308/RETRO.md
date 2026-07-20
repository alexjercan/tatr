# Retro: historical/goal tag also exempts closed-unchecked

## What went well

- Tiny, surgical extension: reused the existing `check_task_is_history_exempt`
  helper on the closed-unchecked block, symmetric with the strict rules. One
  guard line + doc + AGENTS.md + a both-directions checker.sh test. 65/65 incl.
  memcheck; reviewer built and ran it independently.
- Followed the regression discipline (neutralize -> 61/65 -> restore -> 65/65),
  proving the new test can fail.

## What went wrong

- Restored the neutralized guard with `git checkout tatr.c`, which reverts to
  HEAD - but the fix was UNCOMMITTED, so it wiped my working-tree change (only
  tatr.c; checker.sh/AGENTS.md survived). Had to re-apply both tatr.c edits.
  Lesson: commit the fix BEFORE a sabotage/regression check, or restore by
  re-applying the inverse edit, never `git checkout` an uncommitted file.

## What to improve next time

- The exemption still only covers tagged (historical/goal) tasks. A NON-historical
  completed flow task with deliberately-dropped steps (e.g. bevy 20260711-094942,
  steps annotated "(dropped, premise falsified)") still flags closed-unchecked -
  correctly, since it is not frozen pre-flow work. Its honest fix is flipping the
  dropped step boxes to `- [x]` (accounted-for, reason inline), not a tag. That is
  a per-task edit in bevy, out of scope here.

## Action items

- [x] Landed 3b22e7d; rebuilt binary; cleared 2 of 3 bevy closed-unchecked
      (20260704-102342, 20260705-140043 - both historical-tagged).
- Surfaced to user: bevy 20260711-094942 needs its dropped boxes ticked (honest,
  not a historical tag).
