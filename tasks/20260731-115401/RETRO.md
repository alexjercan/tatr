# Retro: Condense tatr skill into referenced docs

- TASK: 20260731-115401
- BRANCH: docs/condense-tatr-skill
- REVIEW ROUNDS: 2

## What went well

- The short `SKILL.md` target stayed measurable throughout the work. Running
  `wc -w -l` early caught the line target before review, not after landing.
- The nix proof was checked against the actual exported store path rather than
  the flake expression alone. That exposed the untracked-source trap before
  review.
- The out-of-context review caught a real omission in the manual comparison:
  record schema shapes were promised by the DoD but not preserved in the first
  reference split.

## What went wrong

- The first split treated "record schemas" as satisfied by saying
  `tatr scaffold` writes from `RECORD_SCHEMAS[]`. That was not enough for a
  cold agent using the reference docs, and it broke the DoD's explicit
  preservation requirement.
- The root cause was comparing categories of content instead of each concrete
  load-bearing detail from the old skill and the task DoD. The schema table in
  `tatr.c` should have been checked while writing `records.md`, not during
  review repair.

## What to improve next time

- For a skill split, make a preservation checklist from the DoD nouns and the
  old headings before writing. Each item needs either a destination file and
  heading, or an explicit reason it is intentionally gone.
- When nix source exports depend on new files, stage those files before using
  `nix eval` output as proof of downstream contents.

## Action items

- Added `records.md` `## Schema Shapes` so the reference docs preserve the
  exact record schema shapes.
- Added the `stage-before-flake-source-proof` lesson to `LESSONS.md`.
