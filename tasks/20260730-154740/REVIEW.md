# Review: Add Epic graph, frontier, claims, and phase context

- TASK: 20260730-154740
- BRANCH: feat/epic-graph

## Round 1

- REVIEWER: out-of-context
- VERDICT: REQUEST_CHANGES

Verified by the reviewer: both suites 98/98 native and under valgrind, `make`
and `make windows` warning-clean, the diff plain ASCII, the graph traversals
terminating without false positives on diamonds, an empty or garbage claim file
handled the safe way, no fd leak, and all five DoD `test:` proofs present and
passing with no `manual:` proof to defer. Four guards were mutation tested
individually and each turned exactly its own test red.

- [x] R1.1 (BLOCKER) tatr.c:5233 - The claim-ownership check compares the
  claim's `- PID:` against `getpid()`, but tatr is a one-shot CLI: the process
  that ran `tatr claim` has exited by the time anything else runs, so the
  comparison can never match. After `tatr claim <id>`, `tatr flow <id> --to
  WORKING` is refused for the session that took the claim and `tatr release
  <id>` is refused as "not by this process", so `--force` becomes mandatory for
  every release and the non-force release the README documents does not exist.
  Key ownership on something that survives across invocations.
  - Response: Confirmed by re-running it - claiming a task made it unstartable
    and unreleasable by the claimer. The identity model is replaced rather than
    patched: `TATR_SESSION` (default: the working directory) is what ownership
    compares, recorded as a `- SESSION:` field; OWNER, HOST, PID and SINCE stay
    as diagnostics for a human reading a contended claim, which is all a pid was
    ever good for. Verified end to end: the claiming session starts and releases
    with no flag, another session is refused both, and `--force` still recovers.
- [x] R1.2 (MAJOR) checker.sh:3705 - `test_atomic_claim` cannot distinguish a
  working ownership check from a broken one: every release passes `--force` and
  the only start it exercises is by a process that did not claim. That is why
  R1.1 shipped green. Add the owner path to the fixture.
  - Response: Fixed as suggested, and this is the finding that matters most -
    the feature's whole point had no assertion. The competing start and the
    refused release now come from a different `TATR_SESSION`, and the owner's
    own start-and-release path is asserted directly. A claim file with no
    SESSION field at all is covered too, after mutation testing showed that
    branch was also unpinned.
- [x] R1.3 (MAJOR) tatr.c:4653 - `graph_depends_reaches` is a depth-first walk
  with no visited set, so a legal diamond-shaped dependency graph is explored
  exponentially; the depth bound stops infinite recursion, not the blow-up.
  Measured 14.1s on 69 tasks, and this runs on every `tatr check` and every
  gated `tatr flow`.
  - Response: Confirmed by re-deriving the curve: 0.016s at 48 tasks, 0.2s at
    60, 3.2s at 72, 12.4s at 78 - quadrupling every two diamonds. Fixed with a
    per-edge visited set so each node is expanded once. Now 0.005s at 78 tasks
    and 0.047s at 360, with every cycle still reported.
- [x] R1.4 (MAJOR) README.md:605 - The claim and the guard that reads it are
  scoped inconsistently: the docs say to claim against the shared checkout with
  `-r`, but `flow` reads the claim from whatever tasks dir it resolved, which
  in a worktree is a different tree that never holds the claim. In the
  documented topology the guard can never fire.
  - Response: Confirmed. Fixed by separating the two locations: `TATR_CLAIMS_DIR`
    (default `<tasks dir>/.claims`) decides where claims live, so parallel
    worktrees share one claims directory while each `flow` still edits its own
    checkout. `test_claims_across_worktrees` builds exactly that topology - two
    trees, one claims dir - and asserts the guard fires for session B, does not
    fire for session A, and that without the override each tree is back to its
    own claims dir. The README documents the full two-variable recipe.
- [x] R1.5 (MINOR) tatr.c:4531 - the `aids_io_listdir` failure path in
  `task_graph_load` leaks the entries array.
  - Response: Fixed; the cleanup call was added before the `return_defer`.
- [x] R1.6 (MINOR) tatr.c:4715 - `tatr new` still mints records the new rules
  flag on sight: `-k STORY` with no `-P`, or `-P`/`-d` naming a task that does
  not exist. Validate the references in `main_new`.
  - Response: Fixed in `new` and, for the references it is asked to set, in
    `edit` - an edit that touches a title is not blocked by a dangling edge it
    did not create. A refused create creates nothing. A broken edge is still
    reachable by hand, which is how one really appears (the referent was
    removed, or the file was edited directly), and the lint is what reports it;
    `test_new_refuses_broken_relationships` pins both halves.
- [x] R1.7 (NIT) tatr.c:6590 - `main_release` reads the claim, decides, then
  unlinks by path with no handle held, so a claim released and re-taken in
  between is unlinked from under its new owner.
  - Response: Narrowed and documented. A non-force release now re-reads the
    claim immediately before the unlink and refuses if it changed hands. The
    comment says plainly that this narrows but cannot close the window, since
    POSIX has no "unlink this exact file" primitive to hold it open across.

## Round 2

- REVIEWER: out-of-context
- VERDICT: APPROVE

Every round-1 response was re-derived rather than taken on trust. Both suites
are 100/100 native and under valgrind. The new claim model was exercised
directly and mutation tested in both directions - always-ours (guard blind) and
always-theirs (the exact R1.1 regression) - and each turned the claim tests red.
Degraded claim files all fail closed: empty, truncated mid-field, and no SESSION
all count as another session's and need `--force`. The two-tree/one-claims-dir
topology was rebuilt by hand and behaves as documented. The dependency walk is a
standard reachability DFS whose visited set is correctly per-edge (sharing it
across edges would suppress a later edge's cycle) and freed on every path;
0.041s at 361 tasks, 0.706s at 1000. `new`/`edit` validation does not block any
real backlog shape - Epic, Stories, a Story depending on a sibling, a parentless
SPIKE, a loose TASK and an Epic under an Epic all create cleanly. No test was
loosened; `test_atomic_claim` is materially stronger.

- [x] R2.1 (MINOR) tatr.c:4956 - `claim_session_id` defaults to `getcwd`, but
  `tasks_dir_path_build` walks up from the cwd, so running tatr from a
  subdirectory of the same checkout changes the session identity: claim at the
  repo root, then release from `./src`, is refused as "not by this one".
  Default the session to the tasks directory resolved from the real cwd, and
  name `TATR_SESSION` in the refusal so the way out is visible.
  - Response: Fixed exactly as suggested. The default identity is now the tasks
    tree found by walking up from the real working directory - stable for every
    invocation inside one checkout, still distinct per worktree, and unaffected
    by `-r` (which is how sessions cooperate and so must not make them the same
    session). Both refusal messages now name TATR_SESSION.
    `test_claim_session_identity` claims at the root and releases from a nested
    subdirectory.
- [x] R2.2 (MINOR) tatr.c:5058 - `TATR_SESSION` is written verbatim into a
  line-oriented record with no validation. A value containing a newline writes a
  claim its true owner can never release and that another session can silently
  take; a trailing space locks its own owner out. Reject control characters and
  trim identically on write and compare.
  - Response: Fixed as suggested. A control character is refused with a clear
    error rather than sanitised, since a session id that is not what the caller
    asked for is its own kind of wrong; a whitespace-only value is refused too;
    and the value is trimmed BEFORE the copy so write and compare agree. Fixing
    this surfaced a crash in my first attempt - trimming the slice afterwards
    moves its `str` into the middle of the allocation, and freeing that aborted
    - which is why the trim now happens on the source string.
- [x] R2.3 (MINOR) tasks/20260730-154740/DECISION.md:52 - the ACCEPTED design
  record still documents the pre-round-1 design and contradicts the code: the
  `-r` answer, the rejected runtime-directory alternative, and "claims are
  scoped to a tasks directory". Every other doc was updated; the decision record
  was not.
  - Response: Fixed. The claim-scope paragraphs now describe what shipped, with
    a dated amendment saying plainly what the original decision got wrong and
    why one flag could not serve both roles. The two abandoned choices are
    listed among the alternatives, so the record shows the fork rather than
    hiding that it moved.
- [x] R2.4 (NIT) checker.sh:3861 - the "released, the start is no longer
  blocked" assertion runs `flow --to REVIEWING`, but the claim guard only
  executes on `--to WORKING`, so it cannot fail whatever the claim state is.
  - Response: Fixed; the assertion moved to a `--to WORKING` transition on the
    task released just above it, with a comment saying why the edge matters.
- [x] R2.5 (NIT) skills/tatr/SKILL.md:46 - the agent-facing `new` bullet and the
  README's Creating Tasks section do not mention the new relationship
  validation; only the CHANGELOG records it.
  - Response: Fixed in both, with a transcript pasted from a real run.
- [x] R2.6 (NIT) tatr.c:7003 - `main_frontier` marks a row CLAIMED from file
  existence alone, so a session's own claim reads as work someone else holds.
  Now that ownership is knowable, distinguish them.
  - Response: Fixed by naming the holder: a CLAIMED row carries
    `claimed-by=<session>`, the same shape a BLOCKED row carries `blocked-by`,
    so your own claim is visible as yours without adding a fourth state. The
    first attempt at this segfaulted - `SS_Fmt` takes the two arguments
    `SS_Arg` expands to, and I passed one slice - which the test caught.
