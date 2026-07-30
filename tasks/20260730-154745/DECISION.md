# Decision: one in-code record schema, a scaffold verb, and a file-based historical exemption list

- DATE: 20260730-170000
- STATUS: ACCEPTED
- TASK: 20260730-154745
- TAGS: decision,flow,schema

## Context

`tatr check` currently validates one record kind properly (TASK.md, through the
v2 parser) and two rules on a second (DECISION.md status and supersede links).
Everything else about the flow artifacts - what a REVIEW.md round looks like,
what a RETRO.md must contain, what `test:`/`cmd:`/`manual:` mean in a Definition
of Done - lives only in skill prose, repeated across `/plan`, `/work`,
`/review`, `/compound` and `/spike`. Prose cannot be enforced and drifts: this
repository's own 28 REVIEW.md files carry four different header shapes and its
28 RETRO.md files carry eleven different section vocabularies.

Three load-bearing forks had to be settled before writing any code: where the
canonical format lives, how the existing non-conformant sibling records are
classified (21 of this repository's 31 tasks trip at least one new rule), and
whether tatr executes the `test:`/`cmd:`/`manual:` proofs a Definition of Done
declares.

## Decision

**One in-code schema table.** `RECORD_SCHEMAS[]` in `tatr.c` holds, per record
kind, the required title prefix, the required `- KEY:` header fields, the
required `## ` sections, and the body template. `tatr scaffold` writes from that
table and `tatr check` validates against that same table, so a format change is
one edit and the scaffolder cannot emit a record the checker rejects. TASK.md
keeps its own parser (it is typed metadata, not prose) but gains kind-specific
required sections from the same table.

**`tatr scaffold <id> <RECORD>`** creates a missing sibling from the schema,
refuses to overwrite an existing file (no `--force`: an existing record is
edited by hand, in the diff), and exposes `--dry-run` (print the path and
template name, write nothing) and `--list` (every record kind for the task, with
present/missing status) for callers that only need paths.

**`tatr proofs <id>`** prints each Definition of Done proof as one
`<n><TAB><kind><TAB><text>` line and never executes anything. The shell text of
a `cmd:` proof round-trips verbatim; running it is the caller's decision, made
in the caller's shell, where the user can see the command.

**`tasks/EXEMPTIONS.md`** is the historical exemption list: one
`- <task-id> <rule>: <reason>` line per suppressed finding. Every check finding
routes through `check_finding`, which consults the list, so any rule can be
exempted the same way. An exemption that never fires is itself a finding
(`unused-exemption`) on a full scan, so the list cannot silently rot. The file
lives in the diff where a reviewer sees a new entry being added - the same
argument that keeps `tatr flow` free of a `--force` flag.

## Alternatives considered

- **A template directory shipped with the binary** (`skills/tatr/templates/*.md`)
  - splits the format from the checker: the scaffolder would read the templates
  while `check` re-implemented the same expectations in C, and the two would
  drift exactly the way the skill prose already has. Rejected.
- **A per-repo schema config file** - makes every repository's flow artifacts a
  different shape, which defeats the point of a shared tool. Rejected.
- **An in-record marker line** added to each historical file to classify it -
  rewrites history, and a self-granted in-record marker is exactly what
  `KIND: EPIC` was introduced to stop the `goal` tag from being. Rejected.
- **A date cutoff baked into the tool** - implicit, unauditable, and wrong the
  moment a repository other than tatr uses it. Rejected.
- **Executing DoD proofs from tatr** - would mean tatr running arbitrary shell
  text out of a markdown file, on behalf of whoever wrote the file. Rejected;
  `tatr proofs` prints and the caller decides.

## Consequences

- The stricter rules can land in one commit without touching a single
  historical REVIEW.md or RETRO.md byte: 37 exemption lines over 21 tasks.
- A new record kind is added by appending one entry to `RECORD_SCHEMAS[]`; the
  scaffolder, the linter and `--list` all pick it up.
- An agent can suppress a check by adding an exemption line. That is deliberate:
  it is visible, attributable and reviewable, unlike the drift it replaces.
- tatr still never runs anything. `checker.sh` remains the only place proofs are
  executed, by a human or an agent reading `tatr proofs` output.
