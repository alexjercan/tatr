# Review: Cut v0.2.0: bump version and compact the CHANGELOG

- TASK: 20260731-145621
- BRANCH: chore/v0-2-0-release

## Round 1

- REVIEWER: in-session (trivial diff exception: two version constants and one
  documentation section; no code path changes, `git diff master...HEAD` touches
  `tatr.c` by one line, `flake.nix` by two, `CHANGELOG.md` and the task record)
- VERDICT: REQUEST_CHANGES

- [x] R1.1 (MAJOR) CHANGELOG.md:31 - "Fourteen flow-artifact rules" is followed
  by thirteen rule names (`bad-record-schema`, `bad-review-round`,
  `bad-verdict`, `missing-reviewer`, `bad-finding-id`,
  `approve-with-open-findings`, `bad-proof-syntax`, `missing-spike-record`,
  `bad-spike-status`, `dangling-seeded-task`, `dangling-decision-task`,
  `nonreciprocal-supersede`, `unused-exemption`). The miscount is inherited
  from master's `## Unreleased` prose, but this branch is what ships it as the
  v0.2.0 release notes, and the workflow publishes this section verbatim.
  Drop the numeral, or state the number the list actually has. Verified by
  extracting the backticked slugs from the bullet and counting them, and by
  confirming each named slug exists in `tatr.c`.
- [x] R1.2 (MINOR) CHANGELOG.md:22 - the claims entry no longer says that
  `tasks/.claims/` is gitignored. That is a change to the user's repository,
  not internal rationale, so it is worth one clause.
- [x] R1.3 (MINOR) CHANGELOG.md:41 - the **Breaking:** typed-metadata entry
  names the required fields but drops their legal values
  (`TASK|EPIC|STORY|SPIKE`, the eight flow steps,
  `DRAFT|APPROVED|NOT_REQUIRED`). A reader hand-correcting a pre-v2 record is
  the exact audience of that entry. The flow steps are already spelled out in
  the `tatr flow` bullet above, so only `KIND` and `PLAN STATUS` need values.

Checked and found sound: `tatr.c:13` and both `flake.nix` version lines read
0.2.0 and `dist/tatr version` prints `tatr 0.2.0`; the heading is
`## v0.2.0 - 2026-07-31` with no `## Unreleased` left; the section is 59 lines;
the release workflow's `Validate tag version` greps and its `awk` note
extraction both succeed against this tree; `nix develop -c ./checker.sh` passes
107/107; the eight task-graph rules are named correctly and all exist in
`tatr.c`; no fact from the old section is lost beyond design rationale, which
the task records hold.

Pending user checks: the tag is not pushed and no release is published
(manual), which stays open until after landing.

## Round 2

- REVIEWER: in-session (same trivial-diff exception as round 1; round 2 reviews
  three wording edits inside the section round 1 already read in full)
- VERDICT: APPROVE

Responses verified:

- R1.1 fixed at CHANGELOG.md:24 and :27 - both numerals are gone
  ("Task-graph check rules", "Flow-artifact check rules"), so no count can
  disagree with its list. The eight and thirteen slugs are unchanged and each
  still resolves in `tatr.c`.
- R1.2 fixed at CHANGELOG.md:20 - "(default `<tasks dir>/.claims`, now
  gitignored)".
- R1.3 fixed at CHANGELOG.md:37 - `KIND` carries `TASK|EPIC|STORY|SPIKE` and
  `PLAN STATUS` carries `DRAFT|APPROVED|NOT_REQUIRED`; `FLOW STEP` points at
  the eight steps spelled out in the `tatr flow` bullet above rather than
  repeating them.

No fix regressions: the section is 59 lines (still under the 60-line criterion),
the heading and the absence of `## Unreleased` are unchanged, the release
workflow's awk extraction still yields a non-empty body, and
`nix develop -c ./checker.sh` passes 107/107 after the edits.

Pending user checks: the tag is not pushed and no release is published
(manual), which stays open until after landing.
