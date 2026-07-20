# Retro: Adopt flow v2 (tatr)

- TASK: 20260720-171902
- BRANCH: chore/flow-v2-adoption (landed as b2455e0 via sprout land)
- REVIEW ROUNDS: 2 (R1 out-of-context APPROVE zero findings; R2 for the
  user-directed docs/ wipe, APPROVE with the authorization-provenance
  MINOR resolved in-session)

## What went well

- Distil-then-wipe in the right order: the wipe only happened after the
  reviewer had independently verified all twelve ledger entries against
  all seven retros, so nothing durable was at risk.
- The work agent read check_ledger's parser before authoring the ledger,
  wording the preamble to avoid false promotion-stalled fires.
- The reviewer's authorization-provenance finding is the out-of-context
  model working correctly: it cannot see the driving session, so it
  refused to take "user-directed" on faith and asked for the record -
  which the in-session pass holds.

## What went wrong

- The plan assumed move-a-ledger; this repo needed create-from-retros.
  Honest step amendment covered it, but per-repo scouting could have
  specialized the task body up front.
- The blanket DoD grep self-matched again (fourth repo today).

## Action items

- [x] Ledger + wipe landed; residue: none.
