# Render close-out diff stats from a tool instead of retyping them

- STATUS: OPEN
- PRIORITY: 60
- TAGS: feature, lessons, records
- KIND: TASK
- FLOW STEP: BACKLOG
- PLAN STATUS: DRAFT

## Story

As a record author, I want the diff's numbers rendered by a tool rather than
retyped, so a work report or close-out cannot cite a triple no diff produces.

## Steps

- [ ] Add `tatr stat <id> [--since <base>]`, emitting the
      `git diff --shortstat` line for the task's branch.
- [ ] Decide the fallback with the user: if the verb is more tool than it is
      worth, drop it and let the nix.dotfiles close-out template carry the
      COMMAND instead of its result, and say so here rather than leaving this
      step unticked.
- [ ] Give the verb its own `checker.sh` test, mutation-tested per AGENTS.md
      before review.

## Definition of Done

- `tatr stat <id>` prints the task branch's `git diff --shortstat` line, or
  this record states why the verb was dropped in favour of the template
  (test: name the `checker.sh` test once the shape is chosen).
- Full native and memory-check suites pass
  (cmd: `nix develop -c ./checker.sh && nix develop -c ./checker.sh --memcheck`).
- `tatr check` is clean (cmd: `nix develop -c ./dist/tatr check`).

## Notes

- Moved here from nix.dotfiles on 2026-07-31 and reworded for this repository.
  The tool was always going to land here; the ledger entry that promoted it
  lives in that repository's LESSONS.md and still points at this ID.
- Promoted from the `counts-come-from-the-diff` (x6) lesson, disposition
  PROMOTE recorded 2026-07-31. The lesson: work reports and close records must
  cite what the diff SHOWS - numbers and identifiers alike - not what a summary
  of it says. Recurrences: 20260720-171843, 20260720-171836, 20260730-142540
  (R1.6, R2.1), and a sixth that was a commit id rather than a count.
- Promotion order says tool > template; the fallback is the template form.
- Cross-repository follow-ups, NOT part of this task's Definition of Done:
  update nix.dotfiles' work and compound close-out guidance to cite the
  generated output, then record the absorption there with
  `tatr ledger -s counts-come-from-the-diff -D ABSORBED -T <target>`.
