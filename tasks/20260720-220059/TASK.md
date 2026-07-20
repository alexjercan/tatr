# build guard: fail bare-shell build, require nix develop

- STATUS: CLOSED
- PRIORITY: 60
- TAGS: feature

## Story

As a tatr developer, I want a build guard that fails a bare-shell build and
directs the user to `nix develop`, so that the recurring
`build-through-nix-dev-shell` lesson (x3, still pending in the ledger) is
absorbed by tooling instead of prose. Sessions re-learned it twice after the v2
docs already documented it.

## Steps

- [x] Add a guard (Makefile target check or env sentinel) that detects a bare shell (outside the nix dev shell) and fails with a pointer to `nix develop`.
- [x] Ensure it does not false-trigger inside the nix dev shell or in `nix flake check`.
- [x] Verify: bare `make` outside the shell fails clearly; inside the shell it builds.
- [x] Update AGENTS.md ledger entry / mark the lesson resolved once landed.

## Definition of Done

- Building outside the nix dev shell fails with a `nix develop` pointer (manual: run make in a bare shell).
- Building inside the dev shell and via `nix flake check` still works (cmd: `nix flake check`).

## Notes

- Resolves the x3 pending-promotion `build-through-nix-dev-shell`.
