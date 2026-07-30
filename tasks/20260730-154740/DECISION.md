# Decision: Epic graph in check, a claims directory beside the tasks, and path-only context

- DATE: 20260730-211258
- STATUS: ACCEPTED
- TASK: 20260730-154740
- TAGS: decision,flow,epic,parallel

## Context

20260730-153325 gave task records typed `PARENT` and `DEPENDS ON` fields, and
20260730-154657 gave them a guarded lifecycle. Nothing yet reads those fields as
a graph: `tatr flow` checks that each dependency is CLOSED but never asks
whether it exists, whether a task depends on itself, or whether the dependency
edges form a cycle. Nothing lets a session find the open work under an Epic
without reading every task, and nothing stops two parallel sessions from
starting the same Story.

Four load-bearing forks had to be settled before writing code: where graph
validation lives, where a claim lives so that parallel sessions actually see
each other, what "required Stories" means when an Epic tries to close, and what
`tatr context` prints.

## Decision

**Graph validation is `tatr check` rules, not a new verb.** The graph is a
property of the record set, which is exactly what `check` lints. New rules join
the existing ones: `missing-parent`, `missing-dependency`,
`duplicate-dependency`, `self-dependency`, `self-parent`, `dependency-cycle`,
`parent-cycle` and `bad-epic-relationship` (a `PARENT` that does not name a
`KIND: EPIC`, or a `KIND: STORY` with no parent at all). They go through the
same collector-and-report path 20260730-154745 established, so `tatr flow`
enforces them at the same transitions and no transition can mint a graph the
lint would flag.

**A claim is an `O_CREAT|O_EXCL` file under `tasks/.claims/<id>`.** The probe
this task's Notes asked for ran 64 concurrent contenders over 200 rounds
against the real filesystem: `O_CREAT|O_EXCL` and `mkdir` each produced exactly
one winner in 200 of 200 rounds. `O_CREAT|O_EXCL` wins the tie because the
winner writes its owner payload in the same call that wins the race, while
`mkdir` would need a second, non-atomic step to record who won. The payload
records owner, host, pid and claim time - what a human needs to decide whether
a claim is live or stale.

The claims live in a dotted directory beside the task directories rather than
inside them: one `.gitignore` line covers the lot, `tatr rm` and every HUID scan
skip it for free, and a claim never appears in a task's record folder, where
everything else is versioned history.

**A claim coordinates sessions that share a claims directory, and ownership is
a session id.** Two environment variables carry that: `TATR_CLAIMS_DIR`
(default `<tasks dir>/.claims`) decides where claims live, and `TATR_SESSION`
(default: the tasks tree found by walking up from the real working directory)
decides who holds one. Sprout worktrees are separate checkouts with separate
`tasks/` trees (verified: different inodes), so parallel sessions point
`TATR_CLAIMS_DIR` at one shared directory while each still edits its own
checkout.

Ownership cannot be a process id. tatr is a one-shot CLI: the process that runs
`tatr claim` has exited before anything else reads the claim, so a recorded pid
names a process that is already gone and can never match again. `OWNER`, `HOST`,
`PID` and `SINCE` are still recorded, but as diagnostics for a human reading a
contended claim rather than as the comparison.

*Amended 2026-07-30, after review round 1. The original decision said the
existing `-r ROOT` flag was the answer - claim against the main checkout, then
sprout - and keyed ownership on the pid. Both were wrong, and wrong together:
the pid could never match, so a session could not start or release its own
claim; and `flow` reads the claim from the tasks dir IT resolved, which in a
worktree is a tree that never holds the claim, so the start guard could not
fire in the very topology the feature exists for. One flag cannot serve both
roles, because the tree a session claims in and the tree it edits are
deliberately different.*

**An Epic closes when every child is CLOSED.** There is no optional-child field
and none is invented: a child that was dropped or superseded is CLOSED with the
reason recorded, which is the same shape the flow skill already gives a
falsified investigation. Leaving it OPEN forever to mean "not required" would
make the guard unfalsifiable.

**`tatr context <id> --phase <phase>` prints paths, never contents.** One
`<path><TAB><present|missing>` line per artifact the phase owns, in the same
shape `tatr scaffold --list` already uses. Printing a missing path is
deliberate: the review phase owns `REVIEW.md` whether or not it exists yet, and
the caller needs the path in order to create it.

## Alternatives considered

- **A `tatr graph` verb** for validation - would give the record set two linters
  with two exit codes and two things to run in CI. Rejected; `check` already is
  the conformance gate.
- **`mkdir` as the claim primitive** - equally atomic in the probe, but ownership
  would have to be written in a second step, so a crash between the two leaves a
  claim nobody can attribute. Rejected.
- **A lock file inside `tasks/<id>/`** - would put ephemeral machine state in a
  folder whose every other file is versioned history, and would show up in
  `git status` on every claim. Rejected.
- **Claims in a per-user runtime directory keyed by project name** - would work
  across sprout worktrees without configuration, but keys coordination on a
  directory name that any two unrelated checkouts can share. Rejected as a
  silent correctness trap; `TATR_CLAIMS_DIR` is explicit about the same thing.
- **Scoping claims to the tasks directory alone, reached with `-r`** - the
  original decision, abandoned in review round 1. A session claims in the
  shared tree and edits its own, so one flag cannot name both and the start
  guard could never fire. See the amendment above.
- **A process id as the owner** - the original decision, abandoned in review
  round 1: a one-shot CLI's pid is gone before the claim is read again.
- **An `OPTIONAL` or `CANCELLED` marker for dropped children** - a third status
  or a new field to make an Epic closable with open children. Rejected: closing
  a dropped task with its reason is already how this repository records a result
  that turned out not to need work.

## Consequences

- Graph problems are found by the same command and fixed at the same gates as
  every other record problem, and `tatr flow` refuses to start a Story whose
  dependency does not exist rather than treating it as satisfied.
- Claims need two environment variables understood to work across worktrees.
  The README documents the pair with a worked recipe, `tatr claims` prints the
  directory it is reading, and every refusal names the session that holds the
  claim so the way out is visible.
- A stale claim needs a deliberate `tatr release <id> --force`; there is no
  timeout that silently steals one, because "the owner is slow" and "the owner
  is dead" are indistinguishable to tatr.
- `tasks/.claims/` needs a `.gitignore` entry, which this task adds.
