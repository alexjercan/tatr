# Require user disposition for lesson promotions

- PRIORITY: 80
- TAGS: feature, flow, lessons
- ACTIVITY: COMPOUNDING
- GATES: PLAN REVIEW RETRO
- RESOLUTION: DONE
- DEPENDS ON: 20260730-153325, 20260730-154745

## Story

As the lessons-ledger owner, I want every threshold promotion to receive an
explicit user disposition and every approved change to use the normal reviewed
task lifecycle, so lessons cannot stall forever or silently rewrite policy.

## Disposition Grammar

A disposition is an annotation inside the count parens of a `## Pending
promotions` entry, extending the annotation vocabulary the ledger header
already documents (DECISION.md records the four forks this settles):

```
- `slug` (x3, PROMOTE 2026-07-30 -> 20260731-101010): <lesson text>
- `slug` (x3, DEFER 2026-07-30 at x3: <reason>): <lesson text>
- `slug` (x3, RETIRE 2026-07-30: <reason>): <lesson text>
- `slug` (x3, ABSORBED 2026-07-30 by <target>): <lesson text>
```

The new grammar is validated ONLY under `## Pending promotions`. Entries that
were decided and moved back to their own section keep the applied markers
`promotion-stalled` already exempts (`PROMOTED <date> -> <target>`, `absorbed
by <target>, <date>`, `RETIRED <date>: <reason>`), so no ledger history is
rewritten.

A DEFER records the count it was taken at. When the lesson recurs and
`/compound` bumps the count past that number, the deferral no longer covers the
entry and the decision is asked for again - the clock-free way a DEFER cannot
become a permanent silence.

## Steps

- [x] Record the four settled forks in `DECISION.md` (annotation syntax,
      Pending-only validation with a task-id PROMOTE target, count-based DEFER
      re-raise, one flat `tatr ledger` verb).
- [x] Add a ledger entry parser: for each line under `## Pending promotions`,
      the slug, the count, and the disposition (none, or one of the four forms
      with its date and task/reason/target payload). Counts, lesson text and
      every other line round-trip byte for byte.
- [x] Write the checker.sh assertions for the exact new finding messages BEFORE
      the emitting code (`test-first-for-check-messages`).
- [x] Extend `check_ledger` with `promotion-awaiting-decision` (a bare `(xN)`
      entry in Pending, or one whose DEFER count is below the current count),
      `bad-disposition` (an annotation matching no form, or missing its date,
      reason or target), and `dangling-promotion-task` (a PROMOTE naming a
      non-HUID or a task with no `TASK.md`). `promotion-stalled` is unchanged.
- [x] Add `main_ledger`: `tatr ledger [-L|--ledger <file>]` (default
      `LESSONS.md`) lists every Pending entry as
      `<slug><TAB>x<count><TAB><state>`, where state is `awaiting-decision` or
      the recorded disposition.
- [x] Add the recording mode: `tatr ledger --slug <s> --disposition
      PROMOTE|DEFER|RETIRE|ABSORBED [--task <id>] [--reason <t>] [--target <t>]`.
      PROMOTE requires `--task` to resolve to an existing task and takes no
      reason/target; DEFER and RETIRE require `--reason`; ABSORBED requires
      `--target`. The date comes from local time, the same source as HUIDs.
      The command writes the ledger file and nothing else - it never edits the
      promoted tool, template, doc or skill.
- [x] Refuse to overwrite an existing disposition, with the one exception that
      a DEFER whose count has been passed is decidable again. No `--force`,
      matching `scaffold` and `flow`.
- [x] Make the write atomic in the codebase's sense: parse and validate the
      whole file and build the new content in memory, then write once. Every
      refusal leaves the ledger byte-identical.
- [x] Wire `ledger` into the dispatch chain and `tatr_print_help`.
- [x] Mutation-test each new guard: delete its side effect one at a time and
      watch its own test go red (`mutation-test-the-new-guard`).
- [x] Update `README.md` (check rules plus a `tatr ledger` section),
      `AGENTS.md`, `CHANGELOG.md`, `skills/tatr/SKILL.md`, and the `LESSONS.md`
      header that documents the annotation vocabulary. Sweep every doc surface
      for the old "passes indefinitely" claim, excluding `tasks/`.
- [x] Revalidate tatr's own ledger: the two bare Pending entries now fail, so
      collect the user's disposition for each and record it with the new
      command until `tatr check --ledger LESSONS.md` is clean.

## Definition of Done

- A bare threshold entry in Pending fails conformance
  (test: `test_ledger_pending_requires_disposition`).
- Each explicit disposition parses, persists, and avoids repeated prompts
  (test: `test_ledger_dispositions`).
- A DEFER stops covering the entry once the count grows past the one it
  recorded, and is then decidable again
  (test: `test_ledger_defer_reraises`).
- PROMOTE requires a resolvable task reference and never edits the promotion
  target itself (test: `test_ledger_promote_requires_task`).
- Invalid disposition writes are atomic
  (test: `test_ledger_disposition_atomicity`).
- `tatr ledger` lists pending decisions and records each disposition through
  its happy path (test: `test_ledger_command`).
- tatr's own ledger passes with dispositions the user chose
  (cmd: `nix develop -c ./dist/tatr check --ledger LESSONS.md`).
- Full native and memory-check suites pass
  (cmd: `nix develop -c ./checker.sh && nix develop -c ./checker.sh --memcheck`).

## Notes

- Parent Epic: nix.dotfiles task 20260730-153122.
- Depends on: 20260730-153325, 20260730-154745 (both CLOSED).
- The user decision is mandatory. The CLI records it; the agent must use the
  platform user-input mechanism or ask directly before calling the command.
- `check_ledger` is at `tatr.c:6085`; its only current rule is
  `promotion-stalled`. `main_scaffold` is the model for a validate-then-write
  command; `task_save` is the model for the never-half-apply discipline.
