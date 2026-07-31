# Changelog

All notable changes to tatr are documented here.

## Unreleased

### Added

- `tatr ledger`: the promotions a lessons ledger is waiting on a decision for,
  one `<slug><TAB>x<count><TAB><state>` row each, and the command that records
  the decision the user made. A lesson under `## Pending promotions` now owes an
  explicit disposition, written into the entry's own count parens as
  `PROMOTE <date> -> <task-id>`, `DEFER <date> at x<count>: <reason>`,
  `RETIRE <date>: <reason>` or `ABSORBED <date> by <target>`; three new
  `--ledger` rules enforce it. `promotion-awaiting-decision` fires on a bare
  count there, so the section is a queue with an exit rather than the place
  lessons went to be forgotten. `bad-disposition` covers a malformed annotation
  and `dangling-promotion-task` a PROMOTE that names no existing task -
  PROMOTE requires a task id precisely so the promoted edit to a doc, tool or
  skill goes through the ordinary plan, review, retro and close guards instead
  of being applied straight out of the ledger, and `tatr ledger` writes the
  ledger file and nothing else. A DEFER records the count it was taken at and
  stops covering the entry once the lesson recurs past it, which is what keeps a
  deferral from becoming a permanent silence without putting a wall clock into
  `tatr check`. The grammar is validated only under that heading, so a ledger's
  already-applied `PROMOTED` / `absorbed by` / `RETIRED` markers keep working and
  no history has to be rewritten. Every refusal leaves the ledger byte-identical.

- `tatr frontier <ID>`: the open work under an Epic, one tab-separated row per
  child and never a task body. `READY` rows are ready to pick up, `BLOCKED`
  rows carry `blocked-by=<ids>` naming only the dependencies that are not yet
  CLOSED, and `CLAIMED` rows are held by another session. The order is
  deterministic - state, then priority descending, then ID - so two runs are
  byte-identical and the output diffs cleanly.
- `tatr claim <ID>`, `tatr release <ID> [--force]` and `tatr claims`: dividing
  work between parallel sessions. A claim is a file created with
  `O_CREAT|O_EXCL`, so exactly one of any number of racing sessions wins and
  the losers are told who holds it; the winner writes its identity in the call
  that won the race, so a claim is never anonymous. `tatr flow <ID> --to
  WORKING` refuses a task another session holds, and a session releases its own
  claim with no flag. Two environment variables control the model:
  `TATR_SESSION` (default: the working directory) is the identity ownership is
  decided on - never a pid, since tatr is a one-shot CLI and the claiming
  process is gone before anything reads the claim - and `TATR_CLAIMS_DIR`
  (default: `<tasks dir>/.claims`) is where claims live, so parallel worktrees
  can share one claims directory while each edits its own checkout. Nothing
  expires and nothing steals: recovering another session's claim is a
  deliberate `release --force`. `tasks/.claims/` is machine-local state rather
  than versioned history and is added to `.gitignore`.
- `tatr context <ID> --phase <PHASE>`: the artifact paths one flow phase needs,
  as `<path><TAB>present|missing`. Paths only, never contents. Phases are
  `understand`, `plan`, `work`, `review`, `compound`, `resume` and `landing`;
  the first two and `resume` also list the parent Epic's `TASK.md`, because a
  Story cannot be understood without the container that shaped it. A record the
  phase owns is listed even when it does not exist yet, since the caller needs
  the path to create it.
- Eight `tatr check` rules over the task graph, read from one loader that walks
  the tasks directory once: `missing-parent`, `missing-dependency`,
  `duplicate-dependency`, `self-parent`, `self-dependency`, `parent-cycle`,
  `dependency-cycle` and `bad-epic-relationship`. Every member of a cycle is
  reported and a task merely downstream of one is not.
- `tatr scaffold <ID> <RECORD>`: creates a missing sibling record (`SPIKE`,
  `DECISION`, `REVIEW` or `RETRO`) from `RECORD_SCHEMAS[]`, the single in-code
  table `tatr check` now validates records against - so a scaffolded record
  passes the lint with its `TODO` placeholders still in place, and a format
  change is one edit rather than a scaffolder and a checker drifting apart.
  `--list` prints every kind with its path and `present`/`missing`;
  `--dry-run` prints the path it would write and writes nothing. It refuses to
  overwrite an existing record, and there is no `--force`.
- `tatr proofs <ID>`: prints each `## Definition of Done` proof as one
  `<n><TAB><kind><TAB><text>` line, where kind is `test`, `cmd` or `manual`.
  tatr never executes any of it: a `cmd:` proof's shell text round-trips
  verbatim and running it is the caller's decision. `-k/--kind` filters to one
  kind. A proof wrapped across a bullet's continuation lines is still one
  proof, printed on one line.
- Fourteen `tatr check` rules over the flow artifacts, all reading the same
  schema table: `bad-record-schema` (title prefix, required `- KEY:` header
  fields, required non-empty `## ` sections, per record kind and per task
  kind); `bad-review-round`, `bad-verdict`, `missing-reviewer`,
  `bad-finding-id` and `approve-with-open-findings` over a REVIEW.md's round
  structure; `bad-proof-syntax` over Definition of Done items;
  `missing-spike-record`, `bad-spike-status` and `dangling-seeded-task` over
  SPIKE.md; `dangling-decision-task` and `nonreciprocal-supersede` over
  DECISION.md; and `unused-exemption` over the exemption list below. `## Steps`
  and `## Definition of Done` are the plan gate's output, so they are asked for
  only from `- FLOW STEP: PLANNED` on.

- `tasks/EXEMPTIONS.md`: the historical exemption list. Records written before
  a rule existed are classified with one `- <task-id> <rule>: <reason>` line
  rather than rewritten, because the flow trail is append-only history. Every
  check finding routes through one reporter, so any rule can be exempted the
  same way, and an exemption that never fires is itself a finding on a full
  scan. This repository ships 37 such lines over 21 tasks: 14 pre-flow, 6
  early-flow, and one flow-suite retro (20260730-153325) that predates the
  fixed retro section vocabulary.
- `tatr flow <ID> [--to <STEP>]`: the guarded lifecycle. It walks the eight
  legal edges (BACKLOG -> UNDERSTANDING -> PLANNING -> PLANNED -> WORKING ->
  REVIEWING -> COMPOUNDING -> DONE, plus the REVIEWING -> WORKING fix loop),
  derives `STATUS` from the step, and writes `PLAN STATUS: APPROVED` at the
  plan gate. Starting work requires an approved plan and CLOSED dependencies;
  leaving review requires a `REVIEW.md` whose latest verdict is APPROVE with no
  open BLOCKER/MAJOR finding; closing additionally requires ticked `## Steps`,
  a `RETRO.md` and a valid `DECISION.md` status. `KIND: EPIC` is exempt from
  exactly the four requirements `tatr check` exempts it from. Every
  precondition is evaluated before anything is written and all unmet ones are
  reported, so a refused transition leaves `TASK.md` byte-identical. There is
  no `--force` and no repair command.

### Changed

- `tatr flow` refuses to start a task another session has claimed, and refuses
  to close a `KIND: EPIC` while any of its children is not CLOSED. There is no
  optional-child marker: a child that was dropped is CLOSED with the reason
  recorded, because leaving one OPEN to mean "not required" would make the
  guard unfalsifiable.
- A `DEPENDS ON` reference that does not resolve is now refused at the plan
  gate as `missing-dependency` rather than treated as one more blocker to wait
  for - a dangling edge is a broken graph, not work in progress.
- `tatr new` refuses a `PARENT` or `DEPENDS ON` that does not resolve, a
  `PARENT` that is not a `KIND: EPIC`, and a `KIND: STORY` with no parent;
  `tatr edit` refuses the same for the references it is asked to set. Creating
  a record the lint rejects on sight is the producer's bug, not the linter's
  finding. A broken edge is still reachable by hand, which is how one really
  appears - the referent was removed, or the file was edited directly.
- `tatr flow` gates every transition on the same record rules `tatr check`
  applies, reading them through the same collector functions rather than a
  second copy - including `bad-severity` and the `DECISION.md` supersede rules,
  which now live in those collectors rather than in `check` alone.
  `PLANNING -> PLANNED` requires the plan sections and their proofs,
  `REVIEWING -> COMPOUNDING` a schema-clean `REVIEW.md`, and
  `COMPOUNDING -> DONE` additionally a schema-clean `RETRO.md` and
  `DECISION.md`. A refusal names the rule slug the lint would print. This is
  what keeps the tying invariant true: no transition can produce a state the
  lint would then flag.
- **Breaking:** `STATUS`, `FLOW STEP` and `PLAN STATUS` are no longer settable
  through `new` or `edit`. `-s/--status` is gone from both, and
  `-f/--flow-step` and `-S/--plan-status` are gone from the shared metadata
  options; the retired spellings fail with a pointer to `tatr flow` rather than
  a generic unknown-argument error. A task is always born `OPEN` / `BACKLOG` /
  `DRAFT`, and `PLAN STATUS: NOT_REQUIRED` is reachable only by hand.
- The artifact scans `tatr check` lints with - the unchecked `## Steps` count,
  the latest `- VERDICT:`, the open BLOCKER/MAJOR finding count, the
  `DECISION.md` status - are shared with the lifecycle guards, so a transition
  can never produce a state the lint would flag.
- **Breaking:** task records carry typed workflow metadata. `KIND`
  (`TASK|EPIC|STORY|SPIKE`), `FLOW STEP`
  (`BACKLOG|UNDERSTANDING|PLANNING|PLANNED|WORKING|REVIEWING|COMPOUNDING|DONE`)
  and `PLAN STATUS` (`DRAFT|APPROVED|NOT_REQUIRED`) are required fields in the
  metadata block, joined by the optional `PARENT` and `DEPENDS ON` task
  references. There is no migration command and no compatibility mode: a
  pre-v2 record is rejected with a diagnostic naming the file and the field,
  and is corrected by hand.
- Metadata values are validated on the exact token the parser consumes, so a
  misspelled value, a trailing space or a CRLF tail fails the load instead of
  silently defaulting.
- `new` and `edit` gained `-k/--kind`, `-f/--flow-step`, `-S/--plan-status`,
  `-P/--parent` and `-d/--depends-on`; on `edit` an empty value clears an
  optional relationship field.
- Filtering gained the `:kind`, `:flow_step`, `:plan_status`, `:parent` and
  `:depends` fields, and `ls` shows kind and flow step.
- `tatr check`: the `## Flow State` prose scan and its `bad-flow-state` rule
  are gone, since the parser now owns value validity; invalid markers surface
  as `malformed-header`. Container exemptions key on `KIND: EPIC` rather than
  on a `goal` tag, which no longer carries any meaning to tatr.
- `tatr ls` now skips a record that does not parse, names it on stderr, and
  exits non-zero, instead of silently listing nothing and exiting 0.

### Fixed

- tatr no longer writes a record it cannot read back: `new` and `edit` re-parse
  the serialized bytes before writing, so a newline in a title or tag fails
  without touching disk rather than producing a file every later command
  rejects. A failed `new` no longer leaves an empty task directory behind.
- `task_save` no longer double-frees the serialized buffer when the write
  itself fails.

## v0.1.0 - 2026-07-28

Initial public release.

### Added

- Filesystem-backed task storage under a project-local `tasks/` directory.
- CLI commands for creating, listing, showing, editing, removing, and checking
  tasks.
- Query filtering for task listing by status, priority, tags, and title.
- Recursive task discovery and `-r/--root` project selection.
- Flow-oriented task artifact checks for review, retro, decision, and lessons
  records.
- Linux release artifact at `dist/tatr`.
- Windows release artifact at `dist/tatr.exe`.
- GitHub release workflow that publishes Linux and Windows archives plus
  checksums when a `v*` tag is pushed.
