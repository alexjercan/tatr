# Add flow-state lint for planned work

- STATUS: OPEN
- PRIORITY: 90
- TAGS: feature,flow

## Story

As a flow user, I want `tatr check` to catch a task that has been moved into
implementation without an approved plan marker, so an interrupted or careless
agent cannot treat unchecked Steps as proof that planning happened.

## Steps

- [ ] Add a small flow-state parser in `tatr.c` that scans `TASK.md` for a
      `## Flow State` section and recognizes `- FLOW STEP: <value>` plus
      `- PLAN STATUS: <value>` lines without trimming away the exact token being
      validated.
- [ ] Add default `tatr check` findings for flow-state drift:
      `bad-flow-state` for malformed recognized markers, and
      `unplanned-in-progress` for any non-historical, non-goal `IN_PROGRESS`
      task that lacks `PLAN STATUS: APPROVED`.
- [ ] Keep existing backlogs clean by making the new required planned marker
      apply only to `IN_PROGRESS` tasks, not to old CLOSED tasks or ordinary
      OPEN backlog items.
- [ ] Add checker.sh integration tests covering a clean planned task, an
      IN_PROGRESS task without `PLAN STATUS: APPROVED`, malformed marker
      values, and historical/goal exemptions where applicable.
- [ ] Update README.md and AGENTS.md from the real checker output and command
      behavior, including the new finding names and the exact marker spelling.

## Definition of Done

- `unplanned-in-progress` fires for an `IN_PROGRESS` task without
  `PLAN STATUS: APPROVED` (test: `test_check_unplanned_in_progress`).
- Malformed flow markers are linted as `bad-flow-state`
  (test: `test_check_bad_flow_state`).
- Historical and goal tasks remain exempt from the new in-progress planned
  marker requirement (test: `test_check_flow_state_history_exempt`).
- Full tatr suite passes (cmd: `nix develop -c ./checker.sh`).
- Full tatr suite is leak-free (cmd: `nix develop -c ./checker.sh --memcheck`).
- tatr docs name the real rule and marker spellings
  (cmd: `grep -n "unplanned-in-progress" README.md AGENTS.md`).

## Notes

- Repo: /home/alex/personal/tatr.
- Relevant files: `tatr.c`, `checker.sh`, `README.md`, `AGENTS.md`.
- Keep the rule tool-level rather than skill-prose-only: it is the guard that
  catches a resumed `IN_PROGRESS` task whose task text never proved it was
  planned.
- The marker contract planned for the skills is:
  `## Flow State`,
  `- FLOW STEP: UNDERSTANDING|PLANNING|PLANNED|WORKING|REVIEWING|COMPOUNDING|DONE`,
  and `- PLAN STATUS: APPROVED` once the user has accepted the plan.
