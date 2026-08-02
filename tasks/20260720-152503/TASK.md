# Add check subcommand: lint task artifacts for process drift

- PRIORITY: 90
- TAGS: feature
- KIND: TASK
- ACTIVITY: COMPOUNDING
- GATES: REVIEW RETRO
- RESOLUTION: DONE

## Story

As the flow's conformance pass, I want `tatr check` to lint task artifacts
mechanically, so process drift is caught by a command instead of by nobody.
Evidence from the 2026-07-20 flow review: scufris invented LOW/NOTE review
severities and closed tasks with unchecked steps; bevy-common-systems' ledger
had x9 lessons with no Pending promotions section.

## Steps

- [x] main_check in tatr.c: `tatr check [ID] [-r ROOT] [--strict]
      [--ledger FILE]` walks tasks/ (or the one task) and reports findings
      one per line (`<id>: <rule>: <detail>`); exit 1 if any finding, 0 when
      clean.
- [x] Default rules:
      - closed-unchecked: STATUS CLOSED but unchecked `- [ ]` items remain
        under `## Steps`;
      - closed-not-approved: STATUS CLOSED, REVIEW.md exists, latest round
        verdict is not APPROVE;
      - bad-severity: a REVIEW.md finding severity outside
        BLOCKER/MAJOR/MINOR/NIT;
      - malformed-header: STATUS/PRIORITY/TAGS unparseable (report, not
        skip).
- [x] --strict adds: closed-missing-review and closed-missing-retro for
      CLOSED tasks lacking REVIEW.md / RETRO.md.
- [x] --ledger FILE adds: promotion-stalled - an `(xN)` count of 3 or more
      on a line outside the `## Pending promotions` section.
- [x] checker.sh: test_check_* integration tests covering each rule firing
      and staying quiet, per-ID scoping, --strict, --ledger and exit codes.
- [x] README.md and AGENTS.md command lists gain check (docs-sync rule).

## Definition of Done

- All rules implemented and covered (cmd: nix develop -c ./checker.sh)
- Zero leaks (cmd: nix develop -c ./checker.sh --memcheck)
- Exit codes 1-on-findings / 0-clean (test: test_check_exit_codes)
- Docs updated in the same task (cmd: grep -n "check" README.md)

## Notes

- Single translation unit; follow the main_<cmd> + Argparse_Parser + defer:
  shape and wire into the dispatch chain and tatr_print_help (AGENTS.md).
- REVIEW.md is not tatr-owned today: parse verdict/severity with a tolerant
  line scan, not a markdown parser.
- This repo keeps retros in docs/retros/ - follow the local convention.

## Close-out (2026-07-20)

What changed: main_check + check_task + check_ledger + two small helpers
(string_slice_compare_fn, slice_contains_cstr, task_sibling_read,
check_severity_is_known) in tatr.c; dispatch, help text and TATR_VERSION
0.2.0 -> 0.3.0; nine test_check_* integration tests in checker.sh; README
gains a "Checking Task Artifacts" section, AGENTS.md backlog block gains the
command. Malformed tasks are findings, not aborts - the opposite of ls's
policy, deliberately, since surfacing broken artifacts is the command's job.

Also caught beyond the plan: task_status_from_string silently maps unknown
statuses to OPEN, so check re-validates the raw STATUS token
("- STATUS: DONE" is a malformed-header finding, not a silent OPEN).

Difficulties:
- find_current_tasks_dir returns PROJECT dirs (the "/tasks" suffix
  stripped), not tasks dirs - the first walk implementation listed the
  project root, matched no HUIDs and reported nothing. Caught by the smoke
  test (per-ID worked, walk did not); fixed by re-appending TASKS_PATH_CSTR.
  A misleading name; read the callee, not the name.

Evidence: 58/58 checker.sh, 58/58 under --memcheck (zero leaks); sabotage
check: breaking the Steps-section gating turns test_check_clean red
(Action-items box fires), restore returns 58/58. Fix was committed before
the sabotage this time.
