# Add bad-decision-status and dangling-supersede checks to tatr check

- STATUS: OPEN
- PRIORITY: 100
- TAGS: feature

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

- [ ] Add a `check_decision_status_is_valid` helper (mirrors
  `check_severity_is_known`): a STATUS value is valid iff it equals `ACCEPTED`
  or starts with `SUPERSEDED by ` followed by a non-empty ref.
- [ ] Add a supersede-ref resolver: extract the `YYYYMMDD-HHMMSS` HUID from a
  ref string (canonical form `tasks/<id>/DECISION.md`, tolerant of a bare
  `<id>`), then confirm `<tasks_dir>/<huid>/DECISION.md` exists via `access`.
  Reuse the existing HUID-validation helper if there is one; a ref with no
  parseable/existing HUID does not resolve.
- [ ] In `check_task`, after the review-checks block, read `DECISION.md` via
  `task_sibling_read`; when present, scan its first `- STATUS:` line and emit
  `bad-decision-status` on an invalid/absent STATUS value.
- [ ] When STATUS is `SUPERSEDED by <ref>`, and for any `- Supersedes: <ref>`
  header line, emit `dangling-supersede` when `<ref>` does not resolve. Free the
  DECISION.md buffer (match the review-buffer cleanup discipline).
- [ ] Add checker.sh tests, registered in the run list:
  `test_check_bad_decision_status`, `test_check_good_decision_status`,
  `test_check_dangling_supersede`, `test_check_resolving_supersede`, and one
  asserting a task with no DECISION.md is unaffected. Should-fail assertions use
  the `set +e`/split-declaration pattern (checker.sh gotcha).
- [ ] Update tatr `README.md` Default-rules list with both rules.
- [ ] Update tatr `AGENTS.md` only if the check discussion there needs it
  (it currently covers strict rules + exemptions; keep the edit minimal or skip
  if nothing is stale).

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
