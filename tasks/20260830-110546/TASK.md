# Release v3.0.0

- STATUS: CLOSED
- PRIORITY: 0
- TAGS: release

Release v3.0.0 with the breaking removal of IN_PROGRESS. Verify all release
checklist checks, package versions, artifacts, tag, push, workflow, and
published checksums.

## Release checks

- Reviewed changes since v2.0.3 and documented the breaking status removal.
- Confirmed the v3.0.0 tag was available.
- `nix develop -c make clean all`: passed with Clang.
- `./dist/tatr version`: reported 3.0.0.
- `nix develop -c ./checker.sh`: 9/9 passed.
- `nix develop -c ./checker.sh --memcheck`: 9/9 passed with no leaks.
- `nix develop -c make clean all CC=gcc`: passed.
- `nix develop -c make windows`: passed.
- `nix flake check`: passed.
