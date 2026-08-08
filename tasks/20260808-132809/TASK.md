# Release v2.0.1

- STATUS: CLOSED
- PRIORITY: 100
- TAGS: release

Release the `tatr new -b, --body` change as v2.0.1.

## Plan

- [x] Update binary and Nix package versions to 2.0.1.
- [x] Add v2.0.1 release notes.
- [x] Run clang, integration, memcheck, gcc, MinGW, and version checks.
- [x] Prepare the verified release commit for tag `v2.0.1`.

## Done when

- Every release version surface reports 2.0.1.
- Full release verification passes.
- Annotated tag `v2.0.1` points at the release commit.

## Result

- Bumped the binary and both Nix packages to 2.0.1.
- Added v2.0.1 changelog notes for file and stdin body input.
- Prepared the verified release commit as the tag target.

## Evidence

- Clang build: pass.
- Integration suite: 8/8 pass.
- Memcheck suite: 8/8 pass.
- GCC build: pass.
- MinGW build: pass.
- `tatr version`: `tatr 2.0.1`.
- Release version grep checks: pass.
