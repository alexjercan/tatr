# Condense tatr skill into referenced docs

- PRIORITY: 65
- TAGS: docs, skill, build
- KIND: TASK
- ACTIVITY: COMPOUNDING
- GATES: PLAN REVIEW RETRO
- RESOLUTION: DONE

## Story

As a maintainer of the tatr agent skill, I want `skills/tatr/SKILL.md` to be
short and route detailed material into reference docs, so that agents load the
critical workflow rules first and only read the long command/reference material
when the task calls for it.

## Steps

- [x] Audit `skills/tatr/SKILL.md`, `skills/tatr/agents/openai.yaml`,
  `flake.nix`, and the README skill section to identify which current content
  must stay in the short entrypoint and which content can move.
- [x] Rewrite `skills/tatr/SKILL.md` to stay under 400 words and roughly 50
  lines. Keep only trigger scope, the core task lifecycle rule, safest default
  commands, "ask before ledger disposition", and load-on-demand routing.
- [x] Split the deferred content into focused reference files under
  `skills/tatr/`, for example `commands.md`, `lifecycle.md`, `records.md`,
  `check-rules.md`, `filtering.md`, `format.md`, `claims.md`, and
  `workflow.md`. Preserve exact command syntax and rule slugs in the reference
  docs.
- [x] Add explicit load rules at the end of `SKILL.md`, such as command lookup
  -> `commands.md`, planning or flow transition -> `lifecycle.md`, scaffolding
  or records -> `records.md`, lint failure -> `check-rules.md`, `ls -f` use ->
  `filtering.md`, manual TASK.md edits -> `format.md`, parallel worktrees ->
  `claims.md`, and project workflow questions -> `workflow.md`.
- [x] Update the nix export for the skill so downstream consumers receive the
  whole skill directory, including the new reference docs and `agents/`
  metadata, rather than only the entrypoint. Verify whether the existing
  `skills.tatr = ./skills/tatr;` is sufficient or should become an explicit
  cleaned source.
- [x] Update README skill text if needed so it describes the directory skill
  and its reference docs accurately.
- [x] Re-read the produced `SKILL.md` and reference index, count words/lines,
  and run the canonical checks.

## Definition of Done

- `skills/tatr/SKILL.md` is at most 400 words and about 50 lines, with detailed
  material reachable through explicit load-on-demand rules. (cmd:
  `wc -w -l skills/tatr/SKILL.md`)
- All moved details remain present in referenced markdown files under
  `skills/tatr/`, including command syntax, lifecycle gates, record schemas,
  check rule slugs, filtering, TASK.md format, claims, and workflow guidance.
  (manual: compare the old skill content against the new reference files)
- The nix skill output includes every reference doc and existing agent metadata.
  (cmd: `nix eval .#skills.tatr`)
- The repository checks pass. (cmd: `nix develop -c ./checker.sh`)
- The backlog and lessons ledger checks pass. (cmd:
  `tatr check --ledger LESSONS.md`)

## Notes

- The current skill is a single long `skills/tatr/SKILL.md` plus
  `skills/tatr/agents/openai.yaml`.
- `flake.nix` currently exports `skills.tatr = ./skills/tatr;`; the task should
  verify this still exports the full directory once more files exist.
- Keep plain ASCII punctuation in all docs.

## Implementation Notes

- `SKILL.md` is now a 48-line, 365-word entrypoint. It keeps critical safety
  and lifecycle rules, plus explicit load-on-demand references.
- The long material moved into `commands.md`, `lifecycle.md`, `records.md`,
  `check-rules.md`, `filtering.md`, `format.md`, `claims.md`, and
  `workflow.md`.
- `flake.nix` now exports `skills.tatr` via `builtins.path`, so
  `nix eval --raw .#skills.tatr` produces a store path for the full skill
  directory.
- Verification found that untracked reference docs are not present in the
  flake source. Staging the files before evaluating confirmed the store path
  includes all docs and `agents/openai.yaml`.

## Reflection

- The main tradeoff was preserving enough detail without recreating the long
  skill in one file. The short entrypoint now routes by task condition; the
  reference docs keep exact command syntax and rule slugs where detail matters.
- This went better after checking the actual flake output instead of assuming a
  directory path would include untracked docs. Next time, stage new flake
  source files before judging nix source-output proofs.
