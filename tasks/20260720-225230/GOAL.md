# Goal: build guard + retro-completeness backlog cleanup

- DATE: 20260720
- UMBRELLA TASK: 20260720-225230
- LANDING SCOPE: squash-merge each task to local `master` via `sprout land`; do NOT push (user's call).

## Goal

Absorb two pending backlog items into the repo:

1. A Makefile build guard that fails a bare-shell build and points the user at
   `nix develop`, so the recurring `build-through-nix-dev-shell` lesson (x3,
   pending in the ledger) is enforced by tooling instead of prose.
2. Reconcile pre-flow task records so `tatr check -S` is clean: mark the
   truly-recordless pre-flow tasks `historical`, and restore the retros that
   live only in git history (old `docs/retros/`) back into their task folders.

## Done means

1. Building outside the nix dev shell fails with a `nix develop` pointer
   (manual: run `make` in a bare shell -> non-zero exit + pointer message).
2. Building inside the dev shell and via `nix flake check` still works
   (cmd: `nix develop -c make`; cmd: `nix flake check`).
3. Every CLOSED task passes strict check (cmd: `./tatr check -S` prints nothing,
   exit 0).
4. The lessons ledger's pending `build-through-nix-dev-shell` entry is annotated
   as absorbed by the guard.

Overall: `nix develop -c ./checker.sh` green and `./tatr check` / `./tatr check -S` clean on master.

## Tasks

- [x] 20260720-220059 (p60, tatr) build guard: fail bare-shell build, require nix develop
      landed 7221078; 2 review rounds (1 out-of-context REQUEST_CHANGES on stale
      README docs, 1 in-session APPROVE); guard passes in nix dev shell + build
      sandbox, TATR_ALLOW_BARE_BUILD=1 opt-out for CI; ledger lesson absorbed.
- [ ] 20260720-220114 (p30, tatr) retro-completeness: mark pre-flow tasks historical, reconcile stray retros

## Manual acceptance (batched for the user at Finish)

Accumulates `manual:` DoD items as tasks land; presented at Finish.
