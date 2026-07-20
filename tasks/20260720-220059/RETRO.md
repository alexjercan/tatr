# Retro: build guard - fail bare-shell build, require nix develop

- TASK: 20260720-220059
- BRANCH: feat/build-guard
- REVIEW ROUNDS: 2 (1 out-of-context REQUEST_CHANGES, 1 in-session APPROVE)

## What went well

- Checked the nix env markers empirically before writing the guard
  (`IN_NIX_SHELL` inside `nix develop`, `NIX_BUILD_TOP` in the build sandbox)
  instead of guessing, so the guard's two pass-conditions were right the first
  time. Confirmed the sandbox case with a real `nix build`, not just
  `nix flake check` evaluation.
- Reading `.github/workflows/test.yml` early caught that CI builds bare via
  apt-installed clang/gcc and via `./checker.sh` (which runs `make`). A guard
  with no escape hatch would have broken CI; the `TATR_ALLOW_BARE_BUILD=1`
  opt-out set at the workflow's top-level `env` covers every job and step.
- Order-only prerequisite (`| guard`) runs the check every `make` without
  forcing relinks, and left `make clean` unguarded so `checker.sh`'s
  `make clean && make` still works.
- Guard tests use `make guard` (the check alone, no compile) under `env -u`,
  so they are cheap and each fails if the guard logic is removed.

## What went wrong

- R1.1: the user-facing README still documented bare `make`/`clang` as the
  build path after the guard landed. Root cause: I ran the doc-surface sweep
  over AGENTS.md (the contributor doc named in the task) but did not extend it
  to README.md (the user doc). The out-of-context reviewer caught it, and a
  follow-up grep then found two more stale spots the reviewer's single citation
  had not (Build Configuration and Testing sections). One sweep, done across
  every doc surface, would have found all three in the first pass.

## What to improve next time

- When a change alters how the project is built or invoked, grep EVERY doc
  surface for the old invocation in one sweep - README, AGENTS.md, docs/, skill
  files - not just the doc the task happens to name. The task naming AGENTS.md
  did not mean AGENTS.md was the only stale surface.

## Action items

- [x] Ledger `build-through-nix-dev-shell` annotated as absorbed by the guard
  (LESSONS.md Pending promotions); no longer prose-only.
- [x] Added `doc-surface-sweep-all-surfaces` to the ledger (see below).
