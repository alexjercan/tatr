# Retro: Update docs for TASK.md epic containers

- TASK: 20260725-123102
- BRANCH: master
- REVIEW ROUNDS: 1

## What went well

- The stale wording was concentrated in the check-rule docs, so the update
  stayed small and easy to verify with literal greps.
- Checking the implementation comments before patching kept README and AGENTS
  aligned with the actual `goal` tag behavior instead of inventing a new tag.

## What went wrong

- The installed tatr skill text read at the start of the turn still contained
  the old sidecar wording because it had not been redeployed from nix.dotfiles.
  Root cause: skill deployment lags source edits until the home-manager surface
  refreshes.

## What to improve next time

- When changing flow terminology, sweep user docs and code comments together,
  because checker comments often become the source for README wording.

## Action items

- None.
