# Add bad-decision-status and dangling-supersede checks to tatr check

- PRIORITY: 100
- TAGS: feature
- KIND: TASK
- ACTIVITY: COMPOUNDING
- GATES: REVIEW RETRO
- RESOLUTION: DONE
- PARENT: 20260722-151939

## Story

As a tatr user running the flow skills, I want `tatr check` to lint a task's
`DECISION.md` when one exists, so a malformed decision STATUS or a broken
supersede link is caught mechanically instead of rotting silently. The decision
record and its supersede chain are the one cross-file invariant in the flow
scheme and nothing guards them today.

Follows the established `bad-severity` shape: validate-if-present, DEFAULT rule
(not `--strict`), fires only when the sibling file exists. No presence rule, so
past tasks (which have no DECISION.md) need zero migration.

## Steps

- [x] STATUS validity is checked inline in `check_decision` (equals `ACCEPTED`
  or starts with `SUPERSEDED by ` + non-empty ref) rather than a separate
  predicate - the ref is needed for the dangling check in the same branch, so
  splitting it out would have re-parsed the value twice.
- [x] Added `check_supersede_ref_resolves`: tokenizes the ref on `/`, takes the
  first segment that passes the existing `ishuid` helper (so `tasks/<id>/DECISION.md`
  and a bare `<id>` both work), then `access`-checks
  `<tasks_dir>/<huid>/DECISION.md`. A ref with no HUID segment does not resolve.
- [x] `check_task` reads `DECISION.md` via `task_sibling_read` after the strict
  block (presence-gated, independent of TASK.md validity) and emits
  `bad-decision-status` on an invalid/absent STATUS.
- [x] STATUS `SUPERSEDED by <ref>` and every `- Supersedes: <ref>` header emit
  `dangling-supersede` when unresolved; the DECISION.md buffer is freed inline.
- [x] Added the five checker.sh tests and registered them. Added a
  `check_strip_inline_comment` helper so an inline `# ...` enum-hint comment on a
  STATUS/Supersedes line (the spike/decision template style) is tolerated.
- [x] Updated tatr `README.md` Default-rules list with both rules.
- [x] Updated tatr `AGENTS.md`: a one-line note that the DECISION.md rules are
  presence-gated and so need no historical/goal exemption.

## Definition of Done

- A DECISION.md with a bad STATUS token yields `bad-decision-status` on plain
  `tatr check` (test: `test_check_bad_decision_status`).
- A DECISION.md with `ACCEPTED` or a resolving `SUPERSEDED by` yields no
  `bad-decision-status` (test: `test_check_good_decision_status`).
- A `Supersedes:`/`SUPERSEDED by` ref to a nonexistent DECISION.md yields
  `dangling-supersede` (test: `test_check_dangling_supersede`).
- A resolving ref yields no `dangling-supersede`
  (test: `test_check_resolving_supersede`).
- Both rules are DEFAULT: they fire without `--strict`, and a task with no
  DECISION.md is untouched (cmd: `nix develop -c ./checker.sh`).
- Full suite green including memcheck (cmd: `nix develop -c ./checker.sh --memcheck`).
- tatr backlog stays clean (cmd: `cd ~/personal/tatr && ./tatr check`).
- README documents both default rules
  (cmd: `grep -n "bad-decision-status" README.md`).

## Notes

- Relevant code: `tatr.c` - `check_task` (~2729), `task_sibling_read` (~2670),
  `check_severity_is_known` (~2693), `STATUS_FORMAT` (line 65),
  `task_status_is_valid` (line 49). The RETRO-existence check at ~2900 shows the
  `access(F_OK)` + `snprintf` path pattern for existence tests.
- The canonical DECISION.md format is defined in the plan skill
  (`home/modules/agents/skills/plan/SKILL.md`): STATUS line, optional
  `- Supersedes: tasks/<id>/DECISION.md`, and `SUPERSEDED by tasks/<id>/DECISION.md`.
- Resolve refs by extracting the HUID and checking existence under the same
  `tasks_dir`, rather than resolving a filesystem-relative path - all tasks are
  flat under one tasks_dir, so this is robust to how the ref path is spelled.
- Keep it single-file, warning-clean under -Wall -Wextra, zero leaks. Build via
  `nix develop -c make`; test via `nix develop -c ./checker.sh`.
- Depends on: nothing (umbrella 20260722-151939).

## Close-out

- What changed: added three static helpers to `tatr.c`
  (`check_strip_inline_comment`, `check_supersede_ref_resolves`,
  `check_decision`) and a presence-gated block in `check_task` that lints a
  task's `DECISION.md` when it exists. Two new default findings:
  `bad-decision-status` and `dangling-supersede`. README + AGENTS.md documented.
  Five checker.sh tests added (70/70 green, memcheck clean).
- Alternatives considered: (1) a presence rule (`closed-missing-decision`) -
  rejected, since whether a task needed a decision record is a judgment tatr
  can't make and it would flag the majority of tasks and every past task. (2) a
  separate STATUS-validity predicate mirroring `check_severity_is_known` -
  folded inline because the SUPERSEDED-by branch needs the parsed ref anyway.
- Difficulty: first build failed - `aids_string_slice_starts_with` takes the
  prefix by value, not pointer (I passed `&superseded_by`). One-line fix. And
  the initial `dangling-supersede` test asserted the HUID in the message, but
  the finding prints the full ref (`tasks/<id>/DECISION.md`, more useful for
  locating the line); fixed the test's grep, not the message.
- Self-reflection: I wrote the implementation before the tests this round; the
  firing tests (`bad-decision-status`, `dangling-supersede`) do pin the code
  (they assert exit 1 + specific strings, impossible without it), but a
  stricter test-first pass would have surfaced the message-format mismatch
  before the code was written rather than after. The clean-case tests guard
  against false positives.
