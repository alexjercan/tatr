# Retro: Add Epic graph, frontier, claims, and phase context

- TASK: 20260730-154740
- BRANCH: feat/epic-graph
- REVIEW ROUNDS: 2

## What went well

- Running the concurrency probe the task's Notes asked for, instead of assuming.
  64 contenders x 200 rounds showed `O_CREAT|O_EXCL` and `mkdir` both atomic
  here, which turned the choice into a real tiebreak (the winner writes its
  payload in the call that wins the race; `mkdir` needs a second, non-atomic
  step) rather than a preference dressed up as a finding.
- Reusing the collector-and-report machinery 20260730-154745 built. The eight
  graph rules were written once as `graph_node_problems` and `check` and `flow`
  picked them up together, so the tying invariant held for the new rules
  without a third round of the mistake that cost two rounds on the last task.
- The MinGW warning gate added in the previous task paid for itself within the
  hour: it caught `gethostname` being unavailable there the first time the
  Windows build ran, on a line I had no reason to suspect.
- Fixing the `--memcheck` harness rather than working around it. `run_tatr`
  merged the program's stderr into stdout and filtered valgrind's chatter back
  out, so no test could assert which stream anything landed on and the two run
  modes were quietly testing different things. Moving valgrind to `--log-file`
  was three lines and made the modes agree.
- Both review rounds found real defects, and the second confirmed the first was
  actually fixed by re-deriving every claim, including rebuilding the
  two-worktree topology by hand. The rounds were not a formality.

## What went wrong

- R1.1 (BLOCKER): claim ownership compared the claim's recorded PID against
  `getpid()`. tatr is a one-shot CLI, so the claiming process is gone before
  anything else reads the claim and that comparison can never match. The
  feature was unusable in its own documented workflow - claiming a task made it
  unstartable and unreleasable by the claimer. I designed the payload from
  "what would a human want to see in a stale claim" (owner, host, pid, time)
  and then reached for a field off that list to compare, without asking what
  survives between two invocations.
- R1.2 (MAJOR): my own test could not have caught it. Every release in the
  fixture passed `--force`, and the only start it exercised was by a process
  that had not claimed. The owner path - the entire point of the feature - had
  no assertion at all.
- R1.4 (MAJOR): the claim and the guard that reads it were scoped
  inconsistently. I documented "claim against the shared checkout with `-r`"
  while `flow` read claims from whatever tree it resolved, so in the topology
  the feature exists for the guard could never fire. I checked that each half
  worked in isolation and never ran the two together.
- R1.3 (MAJOR): `graph_depends_reaches` had no visited set, so a legal
  diamond-shaped dependency graph was explored exponentially - 12.4s on 78
  tasks, on a walk that runs for every dependency edge on every `check`. I
  bounded the recursion against infinite loops and mistook that for bounding
  the work.
- R1.6: `tatr new` minted records the rules I had just written reject on sight.
  I added producer-side validation to `flow` and `check` and forgot `new` is a
  producer too.
- Two self-inflicted defects while FIXING review findings: trimming a slice
  after allocating it moved its pointer into the middle of the allocation and
  `free` aborted; and `SS_Fmt` with one slice instead of the two arguments
  `SS_Arg` expands to segfaulted `frontier`. Both were caught in seconds by
  running the thing, and neither by the compiler.

## What to improve next time

- For any identity or ownership comparison, name what the two sides are and
  when each is written, before choosing the field. "Recorded by a process that
  has exited" and "computed by the process asking" cannot be the same pid, and
  that is visible in one sentence without writing any code.
- Test the feature's SUCCESS path first, not only its refusals. Three of the
  four MAJOR-or-worse findings this session were features whose happy path had
  no assertion; a refusal-only suite passes just as happily when nothing works
  at all.
- When a design has two halves that must agree (claim and guard, writer and
  reader, lint and lifecycle), build the smallest end-to-end scenario that
  exercises both TOGETHER before calling it done. Each half working alone is
  the failure mode, not the evidence.
- A recursion bound is not a complexity bound. When adding a graph walk, decide
  whether a node can be visited more than once and say so in the comment.
- Mutation-test before review, not after. It found the frontier ordering gap
  before round 1 and two more gaps after round 2, all of them tests that could
  not distinguish working from broken. Every one was cheaper to find that way
  than through a review round.

## Action items

- [x] Replace pid-based ownership with `TATR_SESSION`, defaulting to the tasks
      tree resolved from the real working directory.
- [x] Add `TATR_CLAIMS_DIR` so the claim and the guard can read one directory
      while each session edits its own checkout, with a test that builds the
      two-tree topology.
- [x] Give `graph_depends_reaches` a per-edge visited set; 12.4s to 0.005s at
      78 tasks, 0.047s at 360.
- [x] Validate relationships in `tatr new`, and in `tatr edit` for the
      references it is asked to set.
- [x] Fix `run_tatr` so `--memcheck` and native runs observe the same streams.
- [x] Ledger: add `identity-must-outlive-the-process` and
      `test-the-success-path-not-only-the-refusal`.
- [x] Ledger: bump `mutation-test-the-new-guard` with "run it before review".
