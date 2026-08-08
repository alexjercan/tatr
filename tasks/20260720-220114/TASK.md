# retro-completeness: mark pre-flow tasks historical, reconcile stray retros

- STATUS: CLOSED
- PRIORITY: 30
- TAGS: chore

## Story

As the maintainer, I want the pre-flow tasks (the 8 original tasks and the
5 whose retros were written to the old `docs/retros/` location) reconciled, so
that each CLOSED task's records live in its own folder and strict check is
clean.

## Steps

- [x] Identify the 8 pre-flow tasks (no REVIEW/RETRO) and mark them historical (per tatr task #5's mechanism).
- [x] For the 5 tasks whose retros lived in the old docs/retros/ (now removed; kept in git history), decide: leave historical-marked, or restore an in-folder RETRO.md from history if valuable. Decided: RESTORE. 7 retros (not 5 - two more closed since this task was filed) existed in git history and are valuable, so restored verbatim to their task folders. The 6 with an in-folder REVIEW.md are now fully compliant with NO historical tag; only 235158 (below) needs the tag for its absent review.
- [x] Confirm the one unreviewed shipped task (20260718-235158) is handled (historical or backfilled). Both: its retro was restored from history AND it is tagged historical (it genuinely shipped without a review, so the tag covers the absent REVIEW.md while the restored RETRO.md preserves the real record).
- [x] Confirm `tatr check -S` clean.

## Definition of Done

- Every CLOSED task passes `tatr check -S`, either with in-folder records or a historical marker (cmd: `tatr check -S`).

## Notes

- Depends on tatr task #5 (self; the mechanism this repo implements).
