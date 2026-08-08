# Changelog

All notable changes to tatr are documented here.

## v2.0.1 - 2026-08-08

### Added

- `tatr new -b, --body FILE` reads a task body from a Markdown file.
- `tatr new -b -` reads a task body from stdin.

Body input is read before task creation. An unreadable input exits non-zero and
does not create a task directory.

## v2.0.0 - 2026-08-08

`tatr` is a small task tracker again. Tasks contain a title, `STATUS`,
`PRIORITY`, `TAGS`, and an optional Markdown body.

### Changed (breaking)

- The command surface is `new`, `ls`, `edit`, `help`, and `version` only.
- `ls` validates every selected task and fails on malformed TASK.md files.
- Existing repository tasks use the reduced schema. A normalization script is
  available at `scripts/normalize-tasks.sh` for old lifecycle records.
- Documentation and the tatr skill describe only the reduced format.

### Removed

- Lifecycle, planning, review, retro, claim, proof, check, scaffold, context,
  migration, removal, and show commands.
- Workflow metadata, exemptions, and sibling task records such as NOTES.md,
  DECISION.md, REVIEW.md, RETRO.md, SPIKE.md, and GOAL.md.

## v1.1.0 - 2026-08-05

`KIND` is removed. Every record under `tasks/` is a task: an epic, a story and
a spike differ in what their title says and in how they are linked, not in what
the tool does with them. This is a breaking change to the record format, the
filter language, `new`/`edit` and the check rules; every existing record must be
migrated with `tatr migrate --apply`.

### Changed (breaking)

- The record format. `- KIND: ` is gone, so a record reads `- PRIORITY: `,
  `- TAGS: `, `- ACTIVITY: `, `- GATES: `, `- RESOLUTION: `, then the optional
  `- DUPLICATE OF: `, `- PARENT: ` and `- DEPENDS ON: `. A record still
  carrying `- KIND: ` is refused by every command that loads it, with a pointer
  at `tatr migrate`.
- `-k/--kind` is gone from `new` and `edit`. Both refuse the flag by name -
  `'-k' was removed: KIND is not a field` - rather than as an unknown argument.
- `:kind` is retired from the filter language and refused by name, following
  `:flow_step` and `:plan_status`.
- `tatr ls` no longer prints a `KIND: ` column.
- `PARENT` is a plain hierarchy link: any task may parent any task. The
  `bad-epic-relationship` rule is gone, and so is the `new`/`edit` refusal of a
  parent that was not an Epic and of a Story without one.
- Container exemptions are gone. A task other tasks name as their `PARENT` owes
  the same records as anything else - `closed-missing-review`,
  `closed-missing-retro`, `closed-unchecked` and `inconsistent-gates` all apply
  to it. What survives is the close guard: a task cannot close as `DONE` while
  a child of its own is open, and that now runs for every task rather than for
  a declared kind.
- Every `TASK.md` owes the same two sections once the `PLAN` gate is earned -
  `## Steps` and `## Definition of Done`. The kind-specific section sets
  (`## Done Means`/`## Child Tasks`, `## Question`) are gone.
- `SPIKE.md` is no longer a schema record. `missing-spike-record`,
  `bad-spike-status` and `dangling-seeded-task` are gone, `tatr scaffold` no
  longer writes one, and `tatr context` no longer lists one. A `SPIKE.md` a
  task carries is now an ordinary untyped note, like `NOTES.md`.
- `tatr frontier <ID>` works on any task and no longer refuses a non-Epic. A
  task nothing names as its parent has an empty frontier and exits 0.

### Added

- Leaving `UNDERSTANDING` requires a schema-clean `DECISION.md`: a task cannot
  be planned before something records what it is for and which direction was
  chosen. It earns no gate, so nothing is written down about having met it and
  the edge asks again every time it is walked - including after a rewind. The
  whole `DECISION.md` rule set runs, so the edge cannot mint a record the lint
  would flag. `tatr flow --dry-run` predicts the refusal like any other.
- `tatr migrate` converts v1 records too: it deletes the `- KIND: ` line and
  keeps every other byte, reporting `<id>	KIND: <value> -> dropped`. It still
  converts v0 records in the same pass, and remains dry-run by default.

## v1.0.1 - 2026-08-03

### Changed

- `tatr flow <ID> --dry-run` is a real precondition probe. It evaluates every
  precondition the real call evaluates, in the same order through the same
  collectors, and exits non-zero when the advance would not complete; it
  previously returned before the task graph was loaded and always exited 0.
  A refusal prints the text the subsequent real call prints, verbatim, so the
  output is hand-offable to a consumer unedited. It still writes nothing, and
  it now requires a loadable task graph, so it can no longer report the next
  edge for an unreadable or unresolvable one.

## v1.0.0 - 2026-08-02

The `FLOW STEP` chain is replaced by three independent fields. This is a
breaking change to the record format, the lifecycle commands and the filter
language; every existing record must be migrated with `tatr migrate --apply`.

### Changed (breaking)

- The record format. `- STATUS: `, `- FLOW STEP: ` and `- PLAN STATUS: ` are
  gone; `- ACTIVITY: `, `- GATES: ` and `- RESOLUTION: ` take their place, in
  that order after `- KIND: `. A record still carrying `- FLOW STEP: ` is
  refused by every command that loads it, with a pointer at `tatr migrate`.
- `ACTIVITY` is a nullable cursor over `UNDERSTANDING`, `PLANNING`, `WORKING`,
  `REVIEWING` and `COMPOUNDING`. It moves backward as freely as forward and
  proves nothing.
- `GATES` is the set of gates earned, over `PLAN`, `REVIEW` and `RETRO`,
  serialized in gate order and space separated. `tatr flow` is its only writer.
- `RESOLUTION` is nullable over `DONE`, `WONTDO`, `DUPLICATE` and `SUPERSEDED`,
  with `- DUPLICATE OF: <ID>` for the last two.
- `STATUS` is derived and no longer stored: `CLOSED` when `RESOLUTION` is set,
  `OPEN` when `ACTIVITY` is unset, `IN_PROGRESS` otherwise. Every command that
  reports still prints it.
- `tatr flow <ID>` lost `--to`. It advances exactly one activity, runs the
  current activity's exit gate, and records it. It may half-succeed: when the
  gate passes but the world does not permit the advance - an open dependency, a
  foreign claim - it records the gate, holds the cursor, and reports both
  halves in one write.
- `tatr flow <ID> --to DROPPED --reason <text> [--superseded-by <ID>]` became
  `tatr close <ID> --resolution WONTDO --reason <text>`.
- `:flow_step` and `:plan_status` are retired from the filter language; both
  are refused by name with a pointer at the replacement.
- `tatr frontier` computes `READY` from the derived query - the `PLAN` gate
  earned, the cursor below `WORKING`, dependencies `CLOSED`, unclaimed - and
  prints the row's gates next to its activity (`PLANNING+PLAN`). The
  `blocked-by` column is unchanged.

### Added

- `tatr rewind <ID> --to <ACTIVITY> [--force]`: move the cursor backward. Runs
  no gate; clears every gate produced at or after the target - a rewind to
  `WORKING` keeps `PLAN` and clears `REVIEW` and `RETRO` - and names each one.
  `--force` is required when the record actually carries a gate being cleared.
- `tatr close <ID> --resolution <R> [--of <ID>] [--reason <text>]`: `DONE` runs
  the close gate (all three gates earned, no unchecked `## Steps`, a valid
  `DECISION.md` when present); the other three run no gate and are legal from
  any activity. The happy path stays folded into `tatr flow` out of
  `COMPOUNDING`.
- `tatr reopen <ID>`: clear `RESOLUTION`, the `- DUPLICATE OF: ` pointer and a
  trailing `## Dropped` block, leaving the cursor and the gates where they
  were. A close, reopen and re-close leaves one closure reason, not two.
- `tatr flow <ID> --dry-run`: print the edge and the gate it would run.
- `tatr migrate [--apply]`: convert v0 records in place, dry-run by default.
  The only v0-format knowledge in the binary; removed again in v1.1.0.
- `inconsistent-gates`: a cursor past an activity whose gate the record does
  not carry - the drift the chain made unrepresentable. It subsumes
  `unplanned-in-progress`, which is removed.
- `dangling-duplicate-of`: a `- DUPLICATE OF: ` that names no other task.
- Whole-task exemptions. `- <task-id>: <reason>` with no rule token suppresses
  every rule for that task; `- <task-id> <rule>: <reason>` keeps its meaning.
  An unused entry of either form is still reported.

### Removed

- `PLANNED`, `BACKLOG`, `DONE` and `DROPPED` as lifecycle values. `BACKLOG` is
  the absence of an activity, `PLANNED` is a query, and the other two are
  resolutions.
- `NOT_REQUIRED`. A record that never earned a plan simply carries no `PLAN`
  gate.
- `unplanned-in-progress`. Its predicate is a strict subset of
  `inconsistent-gates`, so the two only ever fired together and a repository
  accepting the fact had to write two exemption lines.

## v0.2.2 - 2026-08-02

### Added

- `tatr flow <ID> --to DROPPED --reason <text> [--superseded-by <ID>]`: retire a
  task without a REVIEW.md or RETRO.md, recording the reason and optionally what
  superseded it. A retired task is terminal and cannot be reopened.

## v0.2.1 - 2026-08-01

### Removed

- Lesson ledger ownership. `tatr` now owns task and sibling-record lifecycle
  only; general knowledge maintenance follows agent configuration.
- The `ledger` command, the `check --ledger` option, and all lesson promotion
  parsing, findings, documentation, and fixtures.

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
