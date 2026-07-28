# Decision: Produce tatr.exe with a MinGW cross build

- DATE: 20260728-095154
- STATUS: ACCEPTED
- TASK: 20260728-095149
- TAGS: decision,build,windows

## Context

The user asked for a `tatr.exe` they can run from Windows. The repo is a small C
CLI with a Makefile and Nix dev shell, and the existing package is Unix-only.
The load-bearing fork is whether to provide a native Windows executable or a
wrapper or installer around the Unix build.

## Decision

Build a native Windows console executable with MinGW and make the repeatable
artifact `dist/tatr.exe`.

## Alternatives considered

- **Rename the Unix binary to `tatr.exe`** - rejected because it would still be
  an ELF binary and would not run on Windows.
- **Ship an installer first** - rejected because the user asked for the
  executable artifact, and an installer can be a later packaging layer on top of
  a proven `tatr.exe`.
- **Require a native Windows development environment** - rejected because the
  repository already standardizes builds through Nix, so a Linux cross-build is
  easier to verify and repeat.

## Consequences

The Windows artifact has a concrete automated build command and can be checked
from the existing Linux workflow. The implementation must keep a small
platform-compatibility layer for POSIX calls that differ under MinGW, and
runtime smoke testing on Windows or Wine may be added later if it proves too
heavy for the default checker path.
