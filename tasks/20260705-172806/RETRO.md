# Retro: AGENTS.md and docs (task 20260705-172806)

## What went well
- Capturing the build/test gotchas in the earlier retros made AGENTS.md almost
  write itself: the clang-vs-gcc split, the `nix develop -c ./checker.sh`
  invocation, and the `set -e` test pattern were already articulated and just
  needed to move into a durable, discoverable file.
- Verifying every documented command against the actual binary caught that the
  README's Limitations list was doubly stale (editing AND filtering), and that
  filtering had shipped long ago but was never documented.

## Difficulties
- One self-inflicted snag: the first Limitations edit pointed at "the filter
  documentation" that did not exist. Rather than soften the reference, I added a
  proper Filtering subsection (the feature was real, just undocumented). Lesson:
  when a doc edit references something, make sure the target exists in the same
  pass.

## Lessons
- Documentation tasks should scan for stale claims, not just add new sections; a
  limitations list ages badly and is worth re-reading in full.
- Keep AGENTS.md close to the real toolchain quirks (nix shell, gcc fallback,
  valgrind) - those are exactly the things a fresh agent trips on.
