# Changelog

All notable changes to tatr are documented here.

## Unreleased

### Changed

- **Breaking:** task records carry typed workflow metadata. `KIND`
  (`TASK|EPIC|STORY|SPIKE`), `FLOW STEP`
  (`BACKLOG|UNDERSTANDING|PLANNING|PLANNED|WORKING|REVIEWING|COMPOUNDING|DONE`)
  and `PLAN STATUS` (`DRAFT|APPROVED|NOT_REQUIRED`) are required fields in the
  metadata block, joined by the optional `PARENT` and `DEPENDS ON` task
  references. There is no migration command and no compatibility mode: a
  pre-v2 record is rejected with a diagnostic naming the file and the field,
  and is corrected by hand.
- Metadata values are validated on the exact token the parser consumes, so a
  misspelled value, a trailing space or a CRLF tail fails the load instead of
  silently defaulting.
- `new` and `edit` gained `-k/--kind`, `-f/--flow-step`, `-S/--plan-status`,
  `-P/--parent` and `-d/--depends-on`; on `edit` an empty value clears an
  optional relationship field.
- Filtering gained the `:kind`, `:flow_step`, `:plan_status`, `:parent` and
  `:depends` fields, and `ls` shows kind and flow step.
- `tatr check`: the `## Flow State` prose scan and its `bad-flow-state` rule
  are gone, since the parser now owns value validity; invalid markers surface
  as `malformed-header`. Container exemptions key on `KIND: EPIC` rather than
  on a `goal` tag, which no longer carries any meaning to tatr.
- `tatr ls` now skips a record that does not parse, names it on stderr, and
  exits non-zero, instead of silently listing nothing and exiting 0.

### Fixed

- tatr no longer writes a record it cannot read back: `new` and `edit` re-parse
  the serialized bytes before writing, so a newline in a title or tag fails
  without touching disk rather than producing a file every later command
  rejects. A failed `new` no longer leaves an empty task directory behind.
- `task_save` no longer double-frees the serialized buffer when the write
  itself fails.

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
