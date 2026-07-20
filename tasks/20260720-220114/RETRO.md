# Retro: retro-completeness - reconcile pre-flow task records

- TASK: 20260720-220114
- BRANCH: chore/retro-completeness
- REVIEW ROUNDS: 1 (out-of-context APPROVE, no findings)

## What went well

- Verified the retro-to-folder mapping from the retros' own headings (each old
  `docs/retros/*` retro names its task id in its `# Retro: ... (task <id>)`
  heading), rather than trusting the filenames. That turned an error-prone
  bulk-restore into a checkable one, and both the out-of-context reviewer and
  the in-session re-derivation confirmed zero misattribution.
- Restored the 7 retros byte-verbatim with `git show b2455e0^:docs/retros/...`
  instead of paraphrasing them into the current RETRO.md format. They are
  historical records; preserving them exactly is more honest than reformatting.
- Chose restore over blanket-historical where a real record exists: the 6 tasks
  with an in-folder REVIEW.md became fully compliant with NO historical tag, so
  `historical` stays reserved for genuinely record-absent tasks and does not
  paper over work that was actually reviewed.

## What went wrong

- Nothing blocking. The task spec's counts were stale ("the 5 whose retros
  lived in docs/retros/") - two more tasks (check-linter 152503, new-collision
  235158) had closed and retro'd between the task being filed and worked, so it
  was really 7. Root cause: a task filed against a moving backlog states a
  snapshot count. Re-derived the actual set from git history rather than
  trusting the number.

## What to improve next time

- When a housekeeping task quotes a count of items to reconcile, treat the
  number as a snapshot and re-enumerate the live set (here: `git log` for the
  removed files, `tatr check -S` for the current flags) before acting.

## Action items

- [x] `tatr check -S` clean on the branch after the task's own retro landed.
- No follow-up code work; `historical`/`goal` exemption mechanism (from
  20260720-220046) already covered every case needed here.
