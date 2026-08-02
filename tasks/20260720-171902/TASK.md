# Adopt flow v2: create root LESSONS.md, AGENTS.md flow section

- PRIORITY: 90
- TAGS: chore, process
- KIND: TASK
- ACTIVITY: COMPOUNDING
- GATES: REVIEW RETRO
- RESOLUTION: DONE

## Story

As a repo in the flow ecosystem, I want the v2 /flow conventions in place -
root LESSONS.md ledger, clean tatr check, AGENTS.md pointing at /flow - so
development here compounds the same way as everywhere else. Part of the
six-repo adoption goal (umbrella: nix.dotfiles tasks/20260720-171807).

## Steps

- [x] Ledger at the root [amended: created, not moved - the repo had no
      ledger anywhere, so there was nothing to git mv]. LESSONS.md was
      created at the root from the lessons-skill format, seeded by
      distilling the seven retros under docs/retros/ (kept in place as
      pre-flow history). Doc-surface sweep found one stale reference: the
      README check example pointed --ledger at the old default path under
      docs/; updated. Format: bare counts until promotion, a "## Pending
      promotions (3+ occurrences, user decides)" section holding the one
      3+ entry (build-through-nix-dev-shell x3); no PROMOTED/absorbed
      annotations pre-existed to preserve - two historical AGENTS.md
      absorptions were annotated PROMOTED 2026-07-05 to record them.
- [x] Fix tatr check findings [amended: none existed]. The backlog was
      already clean before this task (exit 0, zero findings - the
      normalization happened as adoption cleanup in 20260720-152503), and
      stays clean after. Nothing to tick, normalize, or map; residue list
      is empty.
- [x] AGENTS.md: added a short "Development flow" section stating: /flow
      drives development here (plan/work/review/compound via tatr tasks,
      sprout worktrees, out-of-context round-1 reviews, DoD proofs with
      test:/cmd:/manual: notation); LESSONS.md at the repo root is the
      lessons ledger, read before starting any task; `tatr check` (plus
      `--ledger LESSONS.md`) is the conformance gate. Also reconciled the
      old docs guidance in the same file: per-task records now live in
      tasks/<id>/ (RETRO.md next to TASK.md), docs/retros/ is frozen
      pre-flow history, and the Layout tree lists LESSONS.md. No other
      restructuring.
- [x] Verify: tatr check exit 0 (no residue), tatr check --ledger
      LESSONS.md exit 0, and nix develop -c ./checker.sh green
      (Passed 60/60 tests, exit 0).

## Definition of Done

- LESSONS.md at the repo root, old docs/ path gone, no stale references
  (cmd: test -f LESSONS.md && test ! -f docs/LESSONS.md && ! grep -rn "docs/LESSONS" --include="*.md" --include="*.sh" --exclude-dir=tasks .)
  [amended: --exclude-dir=tasks added - this task's own record quotes the
  old path in its step text and in this very cmd, so the original grep
  self-matches; task records are history, not doc surface]
- tatr check clean or residue documented (cmd: /home/alex/personal/tatr/tatr check;
  manual: user reviews the residue list at the goal's Finish)
- ledger lints clean (cmd: /home/alex/personal/tatr/tatr check --ledger LESSONS.md)
- AGENTS.md names /flow and LESSONS.md (cmd: grep -n "flow\|LESSONS.md" AGENTS.md)

## Notes

- Use the tatr binary at /home/alex/personal/tatr/tatr (the installed one
  may predate the check subcommand).
- Preserve history honestly: normalizations keep meaning; ticks record
  verifiably shipped work only (linter-adoption cleanup, per the precedent
  in tatr's own 20260720-152503).

## Close-out

What changed:
- LESSONS.md created at the repo root (the repo had no ledger; the task's
  move wording assumed docs/LESSONS.md existed - amended). Twelve lessons
  distilled from the seven docs/retros/ files: process -
  reproduce-before-fixing, docs-sweep-stale-claims,
  fix-the-tool-not-the-prompt, ask-the-cheap-design-question,
  read-the-callee-not-the-name, validate-the-exact-parsed-token; domain -
  one-resolve-spine, checker-set-e-exit-codes, huid-gates-destruction,
  aids-listdir-raw-dirents, named-literal-char-predicate; pending -
  build-through-nix-dev-shell (x3).
- AGENTS.md: new "Development flow" section (between Working with the
  backlog and Commits); Layout tree lists LESSONS.md and marks docs/ as
  pre-flow history; the "retros live in docs/" paragraph now says per-task
  records go in tasks/<id>/ (RETRO.md next to TASK.md) and docs/retros/ is
  frozen history. docs/retros/ itself untouched.
- README.md: check example now reads `--ledger LESSONS.md` (was the old
  docs/ path; would also have tripped the DoD grep); plus an adjacent
  stale-claim fix, `./tests/checker.sh` -> `./checker.sh` in the Testing
  section (the file lives at the repo root), applied per the
  docs-sweep-stale-claims lesson being seeded in the same change.

Decisions and why:
- Occurrence counts were kept conservative (mostly x1 per originating
  retro); only patterns with clearly separate recurrences got higher
  counts.
- one-resolve-spine, checker-set-e-exit-codes and huid-gates-destruction
  are annotated PROMOTED 2026-07-05 -> AGENTS.md because task
  20260705-172806 verifiably absorbed them there; recording the historical
  promotion is bookkeeping, not self-promotion, and stops future sessions
  from re-proposing them.
- build-through-nix-dev-shell sits in Pending promotions at x3: AGENTS.md
  has documented the dev shell since 2026-07-05, yet the 07-09 and 07-18
  retros re-learned it - the user decides whether prose suffices or a
  Makefile guard should absorb it (tool > prose).
- tatr check was already clean (0 findings), so the whole normalization
  step collapsed to verification; recorded rather than invented.

Difficulties:
- The DoD grep self-matched: the pattern "docs/LESSONS" is quoted in this
  task's own step text and in the cmd itself, so the proof could never
  pass as written once the task file existed. Fixed by scoping the grep
  with --exclude-dir=tasks (task records legitimately cite the old path as
  history). Candidate lesson for the flow ledger: a DoD grep whose pattern
  is quoted inside the task record matches itself; scope the proof to the
  surfaces it guards.

Self-reflection:
- Reading the lint implementation (check_ledger in tatr.c) before writing
  the ledger paid off: it showed that any bare (x3)+ anywhere outside the
  Pending section - including prose in the preamble - would fire, so the
  preamble was written to avoid bare counts. Reading the callee, as the
  seeded lesson says.
- The checker.sh run was first piped to tail (exit code eaten); re-ran
  with the log redirected to a file to capture the true exit 0. Should
  have done that the first time.

## Scope addition (2026-07-20, user-directed at review time)

After round 1 verified the ledger distillation faithful against all seven
retros, the user directed the v2 ephemeral-docs wipe: docs/ (the seven
pre-flow retros, its only contents) removed; AGENTS.md Layout line and
records paragraph plus the ledger preamble updated from keep-as-history to
removed-distilled (git history keeps the files). Round 2 verifies.
