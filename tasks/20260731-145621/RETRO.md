# Retro: Cut v0.2.0: bump version and compact the CHANGELOG

- TASK: 20260731-145621
- BRANCH: chore/v0-2-0-release
- REVIEW ROUNDS: 2

## What went well

- Running the release workflow's own `Validate tag version` greps and its awk
  note extraction by hand against the bumped tree gave the same evidence a new
  local test would have, without a second copy of the rule to keep in sync.
- Diffing the backticked tokens of the old section against the new one turned
  "did the compaction lose anything?" into a list to judge instead of a
  re-read, and it is what surfaced the miscount.

## What went wrong

- The plan opened with a `checker.sh` version-consistency test and a DECISION
  record arguing for it. The user cut both as unnecessary. It seemed sound
  because the gate really does only fire in CI today, but the workflow already
  owns that gate, and duplicating it locally was more machinery than the risk
  justified.
- Round 1 found "Fourteen flow-artifact rules" above thirteen names. The
  miscount was inherited verbatim from master's `## Unreleased` prose and was
  copied forward without being checked, because compaction was framed as
  shortening rather than as re-verifying each claim.
- The line budget was hit three times: each fix reflowed the section back to 60
  lines and needed another bullet tightened. Editing prose to a hard line count
  by hand is a fiddly loop.

## What to improve next time

- Treat text carried from an Unreleased section into a release as new claims to
  verify, not as already-reviewed prose - a count, a version or a path in it is
  as checkable as code.
- Propose the smallest thing that meets the ask first, and let the extra guard
  be the thing the user opts into rather than the thing they have to cut.

## Action items

- None. Both lessons are recorded in `LESSONS.md`; the tag is created on the
  landed commit and left unpushed by the task's own definition of done.
