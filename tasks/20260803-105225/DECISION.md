# Decision: How --dry-run reports a half-successful advance

- DATE: 20260803-110512
- STATUS: ACCEPTED
- TASK: 20260803-105225
- TAGS: flow, cli, exit-status

## Context

`tatr flow` evaluates two independent halves. The record half
(`flow_gate_preconditions` + `flow_close_preconditions`) asks only about the
record; an unmet one is a flat refusal that writes nothing. The world half
(`flow_world_preconditions`, only on the edge entering `WORKING`) asks about
other records; an unmet one still records the gate and holds the cursor, so the
real command half-succeeds and exits 1.

`--dry-run` is being turned into a probe whose exit status is the contract. Its
consumer is the afk runner in nix.dotfiles: it drives the mechanical flow gates
itself and only wakes an agent when the probe refuses. That consumer needs one
branchable status plus the unmet text. The half-success case therefore needs an
answer: what does a probe say about an advance that would partly happen?

## Decision

Report the two halves as two distinct messages - the same two the real command
prints, tensed into the conditional ("Would refuse to advance" / "Would not
advance ... Cursor would be held at X") - and collapse both into exit status
`1`.

Built from scratch today this is still the shape, because the question the
consumer asks is "would this advance complete?", and the answer is no in both
cases. The messages, not the status, carry the distinction, and they carry it
in the exact words the subsequent real call will print - which is the property
that makes the text hand-offable to an agent verbatim.

The record half keeps short-circuiting the world half in the dry run exactly as
in the real call, so the probe's output is always the refusal it predicts and
never text the real command would not print.

## Alternatives considered

- **A distinct exit code for half-success (`2`).** Would let a caller branch on
  "the gate would be earned but the cursor would hold" without parsing text. It
  loses under YAGNI: no in-tree or stated consumer wants that branch, and an
  exit code is a permanent interface - cheap to add later, impossible to remove
  once something depends on it.
- **Exit `0` for half-success**, on the grounds that the gate half would
  succeed. Rejected: it makes the probe report success for a command that exits
  1, so a caller branching on status would advance past a blocked dependency.
- **Evaluate both halves always** rather than short-circuiting, so a dry run
  lists every problem at once. Rejected: it prints findings the real command
  never prints, so the probe stops predicting the refusal it exists to predict.
- **Do nothing.** Leaves `--dry-run` returning 0 before any precondition is
  evaluated, which is what makes it useless as a probe today; the consumer would
  have to attempt a real advance and undo it.

## Consequences

- A caller can branch on `tatr flow <id> --dry-run` exit status and get exactly
  the answer the real call would give, and can hand the stderr text to an agent
  unedited.
- The probe is no longer free: it loads the whole task graph and reads sibling
  records, so it costs what a real refusal costs. A caller that only wanted the
  edge name pays for it too; none exists in-tree.
- `--dry-run` can no longer answer "what edge is next" on an unreadable or
  unresolvable task graph. The CLI has no other answer to that question -
  `tatr show` prints ACTIVITY, not the successor.
- Half-success stays distinguishable only by reading the message, so the two
  message texts become interface. A future caller wanting to branch on it
  programmatically forces the `2` decision to be revisited.
