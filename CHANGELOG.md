# Changelog

All notable changes to tatr are documented here.

## v0.2.0 - 2026-07-31

### Added

- `tatr flow <ID> [--to <STEP>]`: guarded lifecycle over BACKLOG ->
  UNDERSTANDING -> PLANNING -> PLANNED -> WORKING -> REVIEWING -> COMPOUNDING
  -> DONE, plus the REVIEWING -> WORKING fix loop. A refusal writes nothing
  and there is no `--force`.
- `tatr scaffold <ID> <RECORD>`: create a missing `SPIKE`, `DECISION`, `REVIEW`
  or `RETRO` from the schema `tatr check` validates against; `--list`,
  `--dry-run`, never overwrites.
- `tatr proofs <ID>`: each `## Definition of Done` item as
  `<n><TAB><kind><TAB><text>` (`test`, `cmd` or `manual`), never executed.
- `tatr context <ID> --phase <PHASE>`: the `<path><TAB>present|missing` paths a
  phase needs, from `understand` through `landing`.
- `tatr frontier <ID>`: open work under an Epic, one `READY` / `BLOCKED`
  (`blocked-by=<ids>`) / `CLAIMED` row per child.
- `tatr claim <ID>`, `tatr release <ID> [--force]`, `tatr claims`: exclusive,
  non-expiring claims for parallel sessions, keyed on `TATR_SESSION` and stored
  in `TATR_CLAIMS_DIR` (default `<tasks dir>/.claims`, now gitignored).
- `tatr ledger`: lists the lesson promotions awaiting a decision and records the
  user's `PROMOTE` / `DEFER` / `RETIRE` / `ABSORBED`; `--ledger` rules
  `promotion-awaiting-decision`, `bad-disposition`, `dangling-promotion-task`.
- Task-graph check rules: `missing-parent`, `missing-dependency`,
  `duplicate-dependency`, `self-parent`, `self-dependency`, `parent-cycle`,
  `dependency-cycle`, `bad-epic-relationship`.
- Flow-artifact check rules: `bad-record-schema`, `bad-review-round`,
  `bad-verdict`, `missing-reviewer`, `bad-finding-id`,
  `approve-with-open-findings`, `bad-proof-syntax`, `missing-spike-record`,
  `bad-spike-status`, `dangling-seeded-task`, `dangling-decision-task`,
  `nonreciprocal-supersede`, `unused-exemption`.
- `tasks/EXEMPTIONS.md`: `- <task-id> <rule>: <reason>` lines exempting records
  written before a rule existed instead of rewriting history.

### Changed

- **Breaking:** records carry typed workflow metadata: `KIND`
  (`TASK|EPIC|STORY|SPIKE`), `FLOW STEP` (the eight steps above) and
  `PLAN STATUS` (`DRAFT|APPROVED|NOT_REQUIRED`) are required, joined by optional
  `PARENT` and `DEPENDS ON`, and validated on the exact token the parser
  consumes. A pre-v2 record is rejected by file and field; no migration command.
- **Breaking:** `STATUS`, `FLOW STEP` and `PLAN STATUS` are no longer settable
  through `new` or `edit`; `-s`, `-f` and `-S` point at `tatr flow`, and a task
  is born `OPEN` / `BACKLOG` / `DRAFT`.
- `new` and `edit` gained `-k/--kind`, `-P/--parent` and `-d/--depends-on`, and
  refuse an unresolvable reference, a non-Epic `PARENT` or a parentless STORY.
- `tatr flow` gates transitions on the collectors `tatr check` reads, naming the
  rule slug, and refuses a claimed task or an Epic with an open child.
- Filtering gained `:kind`, `:flow_step`, `:plan_status`, `:parent`, `:depends`;
  `ls` shows kind and flow step, and now names an unparsable record on stderr
  and exits non-zero instead of listing nothing.
- `tatr check`: `bad-flow-state` is gone (invalid markers surface as
  `malformed-header`) and container exemptions key on `KIND: EPIC`, not a tag.

### Fixed

- `new` and `edit` re-parse the serialized bytes before writing, so a record
  tatr cannot read back never reaches disk; a failed `new` leaves no directory.
- `task_save` no longer double-frees the buffer when the write itself fails.

## v0.1.0 - 2026-07-28

Initial public release.

### Added

- Filesystem-backed task storage under a project-local `tasks/` directory.
- CLI commands for creating, listing, showing, editing, removing, and checking
  tasks.
- Query filtering for task listing by status, priority, tags, and title.
- Recursive task discovery and `-r/--root` project selection.
- Flow-oriented task artifact checks for review, retro, decision, and lessons
  records.
- Linux release artifact at `dist/tatr`.
- Windows release artifact at `dist/tatr.exe`.
- GitHub release workflow that publishes Linux and Windows archives plus
  checksums when a `v*` tag is pushed.
