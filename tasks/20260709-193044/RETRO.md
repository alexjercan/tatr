# Retro: allow dots/dashes in filter literals (task 20260709-193044)

## What went well
- The bug was a one-character-class problem with a clean seam: the literal
  *continuation* loop in `tatr_filter_lexer_next` was the only thing that
  needed to change. Extracting `tatr_filter_is_literal_char` kept the start
  check and the `:field` lexer untouched, so the blast radius stayed tiny (10
  lines of `tatr.c`) and the existing 45 tests all kept passing.
- Reproducing first paid off: creating a task tagged `v0.1.0` and running the
  failing query before touching code confirmed the exact failure (`v0` then an
  INVALID `.`), so the fix was aimed, not guessed.
- Keyword safety was verified rather than assumed - `and-x` lexing as an
  identifier (not the `and` keyword) was checked manually and holds because
  keyword recognition uses an exact full-token compare.

## Difficulties / gotchas
- The toolchain is not on the bare PATH: the Makefile hardcodes `CC = clang`
  and the repo relies on the nix flake for both `clang` and `valgrind`. The bare
  shell only has `gcc`. The fix was to run every build/test through
  `nix develop -c bash -c '...'`, which puts clang + valgrind on PATH exactly as
  the Makefile and `checker.sh --memcheck` expect. Worth remembering for future
  tasks in this repo: do not reach for `make CC=gcc`, use the dev shell.
- `nix develop` prints a "Git tree is dirty" warning during work-in-progress -
  harmless, just noise, not a failure.

## Lessons / follow-ups
- When touching the filter lexer, prefer a named `is_*_char` predicate over
  inlining the ctype checks - it documents intent (why `.` and `-` but not `/`)
  and gives the next change a single place to edit.
- Deliberately scoped to `.` and `-` per the request. If tags with `/`
  (`area/backend`) or semver build metadata (`v1.0.0+build`) ever show up, the
  follow-up is trivial: extend `tatr_filter_is_literal_char` and add a matching
  `checker.sh` case. Left out on purpose to keep the token surface minimal.
- Pre-existing quirk left untouched: a tag that is exactly a keyword
  (`:tags contains and`) still lexes as the keyword and fails typecheck. Not
  worth fixing speculatively; note it here so a future session recognizes it as
  known rather than new.
