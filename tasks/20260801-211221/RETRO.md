# Retro: Remove lesson ledger ownership from tatr

- TASK: 20260801-211221
- BRANCH: master
- REVIEW ROUNDS: 1

## What went well

- Removing the interface outright kept ownership unambiguous. Negative command
  and option tests, a mutation, full integration suites, and cross-repo Nix
  checks covered both removal and retained task behavior.
- The live-surface sweep caught stale README, AGENTS, skill, and deployment
  contracts across repository boundaries.

## What went wrong

- The first source-sweep proof included the refusal fixture, whose job requires
  spelling the retired interface. The proof was scoped to production and live
  documentation after distinguishing test input from a shipped contract.
- A subcommand help probe exercised unrelated vendored-parser cleanup under
  memcheck. Exact global-help and refusal assertions covered the public
  boundary without coupling removal evidence to that parser path.
- The installed `knowledge` CLI defaulted to the current repository despite
  the skill declaring a central path. The local artifact was removed and the
  observation resubmitted with an explicit `--repo`.

## What to improve next time

- For interface removals, design the absence sweep and negative fixture
  together: isolate the retired spelling in one named fixture and exclude only
  that fixture from live-contract searches.
- Coordinate landing before long verification runs so worktree removal cannot
  interrupt a suite already executing there.

## Action items

- Submitted the reusable interface-removal testing pattern to central
  knowledge. No follow-up task required.
