# Review: build guard - fail bare-shell build, require nix develop

- TASK: 20260720-220059
- BRANCH: feat/build-guard

## Round 1

- VERDICT: REQUEST_CHANGES
- REVIEWER: out-of-context

Out-of-context reviewer verified the guard end to end (bare `make` fails with
the pointer; `IN_NIX_SHELL` and `TATR_ALLOW_BARE_BUILD` pass; `nix flake check`
green; suite 64/64; the 3 new guard tests each fail if the guard is removed;
AGENTS.md and the ledger annotation are honest). One doc gap found. In-session
pass re-derived R1.1 by reading README.md directly.

- [ ] R1.1 (MINOR) README.md:30,53 - the user-facing "Using Make" and "Manual
  Build" sections still show a bare `make` / direct `clang ... tatr.c` as the
  build path, which now hits the guard (or bypasses the intended nix
  workflow). AGENTS.md was updated but README was not. Update README's build
  sections to run builds through `nix develop -c make`, and document the
  `TATR_ALLOW_BARE_BUILD=1` opt-out for environments without nix.
  - Response: Fixed. README "Using Make" now builds via `nix develop -c make`
    and documents the `TATR_ALLOW_BARE_BUILD=1` opt-out; "Manual Build" notes
    that the direct `clang` compile bypasses the Makefile guard. README.md:30-58.

## Round 2

- VERDICT: APPROVE
- REVIEWER: in-session (trivial follow-up: doc-only change addressing one MINOR
  finding; no code touched)

- [x] R1.1 - confirmed: README's build sections now route through
  `nix develop -c make` and document the `TATR_ALLOW_BARE_BUILD=1` opt-out;
  grep of README/AGENTS.md finds no remaining bare-`make`-as-canonical claim.
  Full suite re-run green (64/64). No BLOCKER/MAJOR findings; no open manual
  DoD items (both DoD proofs are cmd/manual already satisfied in Round 1).
