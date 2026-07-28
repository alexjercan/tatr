# Changelog

All notable changes to tatr are documented here.

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
