# Release v2.0.2

- STATUS: CLOSED
- PRIORITY: 100
- TAGS: release

Release dotted and dashed bare filter literals as v2.0.2.

## Plan

- [x] Update binary and Nix package versions to 2.0.2.
- [x] Add v2.0.2 release notes.
- [x] Document the complete release process in `RELEASE.md` and link it from
      `AGENTS.md`.
- [x] Run clang, integration, memcheck, GCC, MinGW, version, and release
      workflow checks.
- [x] Prepare the verified release commit and annotated tag `v2.0.2`.

## Done when

- Every release version surface reports 2.0.2.
- Full release verification passes.
- Annotated tag `v2.0.2` points at the release commit.

## Result

- Bumped the binary and both Nix packages to 2.0.2.
- Added v2.0.2 release notes for dotted and dashed filter literals.
- Added `RELEASE.md` and linked it from `AGENTS.md`.
- Prepared the verified release commit as the tag target.

## Evidence

- Clang build: pass.
- Integration suite: 9/9 pass.
- Memcheck suite: 9/9 pass.
- GCC and MinGW builds: pass.
- Nix flake check: pass.
- `tatr version`: `tatr 2.0.2`.
- Release version and changelog extraction checks: pass.

## Notes

- Release artifacts remain ignored under `dist/`; the tag workflow rebuilds
  them from the release commit.
