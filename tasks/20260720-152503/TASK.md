# Add check subcommand: lint task artifacts for process drift

- STATUS: OPEN
- PRIORITY: 90
- TAGS: feature

## Story

As the flow's conformance pass, I want `tatr check` to lint task artifacts
mechanically, so process drift is caught by a command instead of by nobody.
Evidence from the 2026-07-20 flow review: scufris invented LOW/NOTE review
severities and closed tasks with unchecked steps; bevy-common-systems' ledger
had x9 lessons with no Pending promotions section.

## Steps

- [ ] main_check in tatr.c: `tatr check [ID] [-r ROOT] [--strict]
      [--ledger FILE]` walks tasks/ (or the one task) and reports findings
      one per line (`<id>: <rule>: <detail>`); exit 1 if any finding, 0 when
      clean.
- [ ] Default rules:
      - closed-unchecked: STATUS CLOSED but unchecked `- [ ]` items remain
        under `## Steps`;
      - closed-not-approved: STATUS CLOSED, REVIEW.md exists, latest round
        verdict is not APPROVE;
      - bad-severity: a REVIEW.md finding severity outside
        BLOCKER/MAJOR/MINOR/NIT;
      - malformed-header: STATUS/PRIORITY/TAGS unparseable (report, not
        skip).
- [ ] --strict adds: closed-missing-review and closed-missing-retro for
      CLOSED tasks lacking REVIEW.md / RETRO.md.
- [ ] --ledger FILE adds: promotion-stalled - an `(xN)` count of 3 or more
      on a line outside the `## Pending promotions` section.
- [ ] checker.sh: test_check_* integration tests covering each rule firing
      and staying quiet, per-ID scoping, --strict, --ledger and exit codes.
- [ ] README.md and AGENTS.md command lists gain check (docs-sync rule).

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
