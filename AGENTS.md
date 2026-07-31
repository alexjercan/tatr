# AGENTS.md

Guidance for AI agents (and humans) working in the `tatr` repository.

## What tatr is

tatr (Task Tracker) is a small command-line tool, written in C, that stores
tasks as Markdown files under a `tasks/` directory so they version-control
alongside the code. Each task is a directory named with a timestamp HUID
(`YYYYMMDD-HHMMSS`) containing a `TASK.md`. It is a deliberately minimal,
offline, zero-config alternative to an issue tracker.

## Layout

```
tatr.c        # the entire program (single translation unit)
aids.h        # vendored header-only utility library (strings, arrays, IO, logging)
argparse.h    # vendored header-only CLI argument parser
Makefile      # build + install
checker.sh    # the test suite (integration tests that drive the built binary)
flake.nix     # nix dev shell (provides clang, valgrind, etc.)
skills/tatr/  # Codex/Claude skill exported as `skills.tatr` from the flake
README.md     # user-facing documentation
LESSONS.md    # lessons ledger (read before starting any task)
tasks/        # tatr's own task backlog (tatr dogfoods itself)
```

There is no separate build system beyond the Makefile; `tatr.c` includes the two
vendored headers and defines their implementations at the bottom of the file
(`ARGPARSE_IMPLEMENTATION` / `AIDS_IMPLEMENTATION`).

## Building

The canonical toolchain (clang, valgrind) comes from the nix dev shell, so all
builds run through it:

```bash
nix develop -c make            # builds dist/tatr with clang -Wall -Wextra -O2 -g
nix develop -c make windows    # builds dist/tatr.exe with MinGW
nix develop -c make clean
nix develop -c make install PREFIX=$HOME/.local
```

A Makefile build guard enforces this: a bare `make` (outside `nix develop`,
outside a nix build sandbox) fails with a pointer to `nix develop -c make`
rather than silently building against whatever gcc the login shell happens to
have. The guard passes automatically inside `nix develop` (`IN_NIX_SHELL`) and
inside a nix build sandbox such as `nix flake check` (`NIX_BUILD_TOP`).

If you must build outside nix because the toolchain is provisioned another way
(CI installs clang/gcc + valgrind via apt, or MinGW for `make windows`), set
`TATR_ALLOW_BARE_BUILD=1` to opt out of the guard. The code is portable POSIX C
and compiles cleanly under clang, gcc (`make CC=gcc`), and MinGW
(`make windows`); keep it warning-clean under `-Wall -Wextra`.

## Testing

`checker.sh` is the test suite. It rebuilds the binary and runs integration
tests that invoke `dist/tatr` against throwaway `tasks/` directories. Always run
it after a change:

```bash
nix develop -c ./checker.sh            # build + run all tests
nix develop -c ./checker.sh -v         # verbose (shows failure details)
nix develop -c ./checker.sh --memcheck  # run every test under valgrind (no leaks allowed)
```

The Windows artifact test in `checker.sh` skips when `x86_64-w64-mingw32-gcc`
is not on PATH. Release CI installs MinGW and therefore exercises that test.

Prefer integration tests here over unit tests: add a `test_*` function next to
the related ones and register it in the run list near the bottom of the file.

### checker.sh gotcha

The script runs under `set -e`. A bare command that exits non-zero aborts the
whole run, and `local out=$(cmd)` swallows the command's exit code (you get
`local`'s status, which is always 0). To assert that a command fails, use:

```sh
set +e
local output
output=$(run_tatr <args> 2>&1)
local exit_code=$?
set -e
```

To capture a generated task ID in a test, use the `new_task_id` helper.

### Writing a check rule

Write the `checker.sh` assertion carrying the rule's EXACT expected message
BEFORE the code that emits it. The format is then designed from the assertion,
instead of the assertion being reverse-engineered into whatever the code
happened to print - which is how a test ends up agreeing with a message nobody
chose. Writing it first also proves the assertion is a real one: it fails
before the rule exists, so you have seen it go red.

Use `grep -qx` to match a rule's output as a whole line; a bare `grep -q`
passes on a substring of a longer, different finding.

Watch fixture names when a test asserts a rule does NOT fire. A negative
assertion like `! echo "$output" | grep -q "decided-lesson"` is broken by any
OTHER fixture whose name contains that slug: the grep matches the wrong entry,
the `!` turns it false, and the test fails for a reason that has nothing to do
with the rule. So fixture names must not be substrings of one another.
`test_ledger_pending_requires_disposition` is a live example - its undecided
entry is `open-lesson`, and naming it `undecided-lesson` instead would trip the
`! grep -q "decided-lesson"` assertion meant for a different entry.

### Mutation-test every new guard

Before review, delete each new guard's SIDE EFFECT one at a time, rebuild, and
confirm that guard's OWN test goes red. Removing only the return value is not a
mutation - `0 * report(...)` still prints, so the assertion still passes.

Two results are findings, not noise:

- **No test dies.** The guard is unverified. Either it needs an assertion, or
  it is redundant with a neighbouring check - in which case say so in the
  record rather than leaving the mutation-test step ticked as if it held.
- **The mutant hangs, corrupts data, or crashes** instead of refusing cleanly.
  The code below was assuming a precondition that only this guard enforced, and
  nothing asserts it at the point of use. Assert it there too. In
  `20260730-154756` a malformed-entry refusal was the only thing keeping an
  entry with NULL splice offsets out of pointer arithmetic that wrapped to a
  runaway length; deleting the guard hung the suite instead of failing it.

Revert the mutation in the same step that observes the red. A deliberately
broken build that outlives its step is indistinguishable from a bug to whoever
picks the branch up next.

## Code conventions

- Keep it a single file. New commands go in `tatr.c`; do not split into multiple
  translation units or add build complexity.
- Follow the existing command shape: each subcommand is a `static int main_<cmd>`
  that builds its own `Argparse_Parser`, parses `ctx->argc/argv`, does its work,
  and cleans up under a `defer:` label. Wire it into the dispatch chain and the
  `tatr_print_help` list in `main`.
- Reuse the shared spine rather than reimplementing it:
  - `task_resolve(cwd, huid, &tasks_dir, &task_file_path)` validates a HUID,
    locates the `tasks/` dir (upward search, honoring `-r ROOT`), checks the
    task exists, and hands back owned paths. `show`, `edit` and `rm` all use it.
  - `task_load` / `task_save` and `task_serialize` / `task_deserialize` are the
    only path to and from `TASK.md`. Editing a task means load, mutate fields,
    save; this preserves the description body automatically.
- Memory: match the ownership discipline in the existing code and free
  everything in `defer:`. Every change must pass `--memcheck` with zero leaks.
- Anything that deletes on disk must be gated behind a validated HUID and only
  ever touch `tasks/<id>/`. Never build a destructive path from raw user input.
- Error out non-zero with a clear `aids_log(AIDS_ERROR, ...)` message; do not
  half-apply a change (validate before writing).

## Working with the backlog

tatr tracks its own work in `tasks/`. Use the tool itself:

```bash
tatr ls -s priority          # see the backlog
tatr show <ID>               # read a task's full description and steps
tatr flow <ID> --to WORKING  # claim a task (refuses an unapproved plan or open deps)
tatr flow <ID>               # advance one step; at COMPOUNDING it closes the task
tatr new "..." -p 80 -t feature  # add newly discovered work
tatr new "..." -b body.md    # seed the description body from a file ('-' = stdin)
tatr new "..." -k STORY -P <ID> -d <ID>  # a Story under an Epic, blocked on a task
tatr flow <ID> --to PLANNED  # the plan gate: the only writer of PLAN STATUS
tatr ls -f ':kind eq EPIC'   # the containers
tatr ls -f ':depends contains <ID>'  # everything blocked on a task
tatr frontier <ID>           # the open work under an Epic (READY/BLOCKED/CLAIMED)
tatr claim <ID>              # take a task for this session (atomic; -r for a shared checkout)
tatr release <ID> [--force]  # give it back; --force recovers another session's stale claim
tatr claims                  # what is held in this tasks directory
tatr context <ID> --phase work  # only the artifact paths that phase needs
tatr check                   # lint the backlog for process drift (exit 1 on findings)
tatr ledger                  # the lessons ledger's promotions awaiting a decision
tatr ledger --slug <s> --disposition RETIRE --reason "..."  # record one
tatr scaffold <ID> REVIEW    # write a sibling record from the schema table
tatr scaffold <ID> --list    # every record kind for a task, path and presence
tatr proofs <ID>             # the task's DoD proofs as data (nothing is executed)
```

Per-task records live in the task's own folder: `RETRO.md` (and `REVIEW.md`)
next to its `TASK.md`, per the flow skills. Their format is not prose to be
copied: `RECORD_SCHEMAS[]` in `tatr.c` is the one source for the title prefix,
required `- KEY:` header fields and required `## ` sections of every kind, and
`tatr scaffold <ID> <RECORD>` writes from that table while `tatr check`
validates against it. Write a new record with `scaffold` rather than by hand;
it is schema-clean from the first byte. Scaffolding refuses to overwrite an
existing record - there is no `--force`, for the same reason `tatr flow` has
none. The pre-flow `docs/retros/` were
distilled into the ledger and removed (git history keeps them). Durable
lessons go to `LESSONS.md` at the root; there is no scratch drawer.

`tatr check` is always strict: there is no `--strict` flag to opt in or out. It
requires a `REVIEW.md` and `RETRO.md` on every CLOSED task, EXCEPT records with
`- KIND: EPIC`: an EPIC is an explicit /flow epic, sprint, version, release, or
multi-feature container. The container's broader done definition, child-task
list, decisions index, and manual acceptance live in the container's own
`TASK.md`; child tasks carry the per-task review and retro records. Containers
are also exempt from the `closed-unchecked` rule - a frozen container's step
boxes stay verbatim (superseded / dropped / premise-falsified steps are honest
history) rather than being ticked to silence the lint - and from
`unplanned-in-progress`, since the plan gate applies to the work tasks under
them.

Workflow state is not editable metadata: `tatr flow` is the only writer of
`- STATUS`, `- FLOW STEP` and `- PLAN STATUS`, and `new`/`edit` reject `-s`,
`-f` and `-S` with a pointer to it. A task is born BACKLOG / DRAFT / OPEN and
reaches every other state by walking the chain
(BACKLOG -> UNDERSTANDING -> PLANNING -> PLANNED -> WORKING -> REVIEWING ->
COMPOUNDING -> DONE, plus the REVIEWING -> WORKING fix loop). STATUS is derived
from the step, so a task closes atomically at DONE. The gates are: an approved
plan and CLOSED dependencies to start; an APPROVEd REVIEW.md with no open
BLOCKER/MAJOR finding to leave review; and additionally ticked Steps, a
RETRO.md and a valid DECISION.md status to close. `KIND: EPIC` is exempt from
exactly the four requirements `tatr check` exempts it from. A transition may
never produce a state the lint would flag - both read the same artifacts
through the same helpers - so a guard added to one belongs in that shared
helper, not in a copy. There is no `--force` and no repair verb: a record in a
wrong state is fixed by hand in `TASK.md`, in the diff, where a reviewer sees
it.

Workflow state is typed metadata, not prose: `- KIND`, `- FLOW STEP` and
`- PLAN STATUS` are required fields in the block under the title, and the
parser rejects any value outside their enums. There is no `## Flow State`
section and no `bad-flow-state` rule any more - an invalid marker is a
`malformed-header` parse failure. `unplanned-in-progress` fires when an
ordinary IN_PROGRESS task lacks `- PLAN STATUS: APPROVED`; OPEN backlog and
CLOSED tasks are never asked for one, and `NOT_REQUIRED` is how a record says
its cycle predated plan state.

The `DECISION.md` and `SPIKE.md` content rules are presence-gated, firing only
when a task carries that sibling, so they need no such exemption. Only
`missing-spike-record` keys on `KIND: SPIKE`: any SPIKE.md that exists is
validated whatever the task's kind, because `tatr scaffold <id> SPIKE` will
write one for any task. `## Steps` and
`## Definition of Done` are the plan gate's output, so `bad-record-schema` asks
for them only from `- FLOW STEP: PLANNED` on - a task `tatr new` just created
must not be a finding the moment it exists.

`PARENT` and `DEPENDS ON` are a graph, not two strings. `task_graph_load` reads
every record under one tasks dir once, and `graph_node_problems` answers what a
task's place in it is worth: `missing-parent`, `missing-dependency`,
`duplicate-dependency`, `self-parent`, `self-dependency`, `parent-cycle`,
`dependency-cycle` and `bad-epic-relationship`. A dangling dependency is a
broken graph, not a blocker to wait for, so it is refused at the plan gate
rather than treated as one more thing to finish first.

`tatr flow` gates transitions on these same rules, through the same collector
functions (`record_schema_problems`, `review_round_problems`,
`spike_record_problems`, `decision_record_problems`, `task_record_problems`,
`graph_node_problems`):
`PLANNING -> PLANNED` owes the plan sections and their proofs,
`REVIEWING -> COMPOUNDING` a schema-clean REVIEW.md, `COMPOUNDING -> DONE`
additionally a schema-clean RETRO.md and DECISION.md. A refusal prints the rule
slug the lint would print.

Every record rule lives in a collector and nowhere else. The collectors return
problems as data; `check_report_problems` is the only place `check` prints one
and `flow_unmet_add_problems` the only place `flow` collects one. A rule added
to `check_task` directly - as `bad-severity` and the supersede rules once were -
is a rule the lifecycle does not enforce, and that is exactly how a transition
comes to mint a record the lint then flags.

Records written before a rule existed are classified in `tasks/EXEMPTIONS.md`,
one `- <task-id> <rule>: <reason>` line each, rather than rewritten: the flow
trail is append-only history. Every finding routes through `check_finding`, so
any rule can be exempted the same way, and an exemption that never fires is
itself a finding (`unused-exemption`) on a full scan. Do not add exemptions for
new work - scaffold the record instead. Adding an entry shows up in the diff,
which is the whole mechanism: it is visible and attributable, unlike the drift
it replaces.

Two lifecycle guards belong to the graph rather than to a record: a task another
session has CLAIMED cannot be started, and a `KIND: EPIC` cannot close while any
of its children is not CLOSED. There is no optional-child marker - a child that
was dropped is CLOSED with the reason recorded, the same shape a falsified
investigation already takes - because leaving one OPEN to mean "not required"
would make the guard unfalsifiable.

A claim is an `O_CREAT|O_EXCL` file under `tasks/.claims/<id>`: the kernel makes
exactly one racing session the winner, and the winner writes its identity in the
call that won. Ownership is `TATR_SESSION` (default: the working directory), NOT
a pid - tatr is a one-shot CLI, so the process that took the claim is gone
before anything else reads it and a recorded pid could never match again.
`TATR_CLAIMS_DIR` (default: `<tasks dir>/.claims`) is where claims live; point
parallel worktrees at one directory so the start guard can see across them
while each session still edits its own checkout. Nothing steals a claim
automatically; a session releases its own with no flag, and recovering
another's is a deliberate `tatr release <ID> --force`.

tatr never executes a Definition of Done proof. `tatr proofs <ID>` prints each
one as `<n><TAB><kind><TAB><text>`; the shell text of a `cmd:` proof
round-trips verbatim and running it is the caller's decision, made in the
caller's shell. A whitespace run collapses to one space only when it
contains a byte that would break the record format - a newline (a continued
bullet's wrap) or a tab (the field separator) - so intra-line spacing a command
may depend on survives byte for byte and every line stays three fields.

## Development flow

/flow drives development here: work is planned into tatr tasks, implemented in
sprout worktrees, reviewed out-of-context in round 1, and closed with DoD
proofs in test:/cmd:/manual: notation. `LESSONS.md` at the repo root is the
lessons ledger - read it before starting any task. `tatr check` (plus
`tatr check --ledger LESSONS.md`) is the conformance gate; keep both clean.

A lesson under the ledger's `## Pending promotions` heading owes an explicit
user disposition, written into the entry's own count parens as
`PROMOTE <date> -> <task-id>`, `DEFER <date> at x<count>: <reason>`,
`RETIRE <date>: <reason>` or `ABSORBED <date> by <target>`. A bare count there
is `promotion-awaiting-decision`, so the section is a queue with an exit rather
than a place lessons go to be forgotten. That grammar is validated only under
that heading; a lesson decided earlier and moved back to its own section keeps
the applied markers `promotion-stalled` exempts, because the ledger's history is
not rewritten to adopt a rule.

The decision is the USER'S. `tatr ledger` records it and writes the ledger file
and nothing else: ask before calling it, and never infer a disposition. PROMOTE
requires a task id precisely so the promoted edit to a doc, tool or skill goes
through the ordinary plan, review, retro and close guards instead of being made
straight out of the ledger. A DEFER records the count it was taken at and stops
covering the entry once the lesson recurs, which is the only disposition that
can be revisited without a hand edit.

## Commits

- Plain commit messages, no AI attribution or co-author trailers.
- Use plain ASCII punctuation only: `-`, `--`, `...`, `->`, straight quotes. No
  em dashes, smart quotes, ellipsis characters, or arrows, in code, comments,
  docs, or commit messages.
