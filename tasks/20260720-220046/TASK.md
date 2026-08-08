# tatr check: recognize historical/no-retro tasks so pre-flow tasks stop flagging

- STATUS: CLOSED
- PRIORITY: 70
- TAGS: feature

## Story

As a maintainer, I want `tatr check` to recognize pre-flow / historical tasks
(and umbrellas) so they stop being flagged for missing retros/reviews, so that
the retro-completeness backfills in other repos can mark old work "historical"
instead of fabricating fake retros for it.

## Steps

- [x] Decide the marker mechanism (tags `historical` and `goal`) (e.g. a tag like `historical`, or an explicit TASK.md annotation) that exempts a CLOSED task from `closed-missing-review`/`closed-missing-retro` under `-S`.
- [x] Implement the exemption in tatr.c's check logic, reusing the shared resolve/load spine.
- [x] Add integration tests in checker.sh: a historical-marked task is clean under `-S`; an unmarked one still flags.
- [x] Update AGENTS.md to document the marker.

## Definition of Done

- A CLOSED task carrying the historical marker passes `tatr check -S` (cmd: checker.sh case).
- An ordinary CLOSED task without review/retro still flags under `-S` (cmd: checker.sh case).
- Zero valgrind leaks (cmd: `tatr check` under --memcheck).

## Notes

- Soft dependency for backfill tasks in nix.dotfiles (#14), tatr (#15), bevy (#17). Do this first.
- Keep all code in tatr.c per repo conventions.
