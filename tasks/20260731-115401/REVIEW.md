# Review: Condense tatr skill into referenced docs

- TASK: 20260731-115401
- BRANCH: docs/condense-tatr-skill

## Round 1

- REVIEWER: out-of-context
- VERDICT: REQUEST_CHANGES

- [x] R1.1 (MAJOR) skills/tatr/records.md:6 - The DoD requires the moved
  details to include "record schemas", but the records reference only says
  scaffolding uses the schema table and never documents the actual schema
  shapes. Add the title prefixes, required header fields, and required
  sections for `SPIKE.md`, `DECISION.md`, `REVIEW.md`, `RETRO.md`, plus the
  kind-specific `TASK.md` sections, or add an explicit generated schema
  reference.
  - Response: Added `records.md` `## Schema Shapes`, covering title prefixes,
    required header fields, required sections, REVIEW round shape,
    kind-specific `TASK.md` sections, and scaffold status defaults.

Verified:

- `wc -w -l skills/tatr/SKILL.md` -> 48 lines, 365 words.
- `nix eval .#skills.tatr` returned a store path.
- Store path includes `SKILL.md`, all eight reference docs, and
  `agents/openai.yaml`.
- `tatr check --ledger LESSONS.md` passed.
- `nix develop -c ./checker.sh` passed 107/107.
- `git diff --check master...HEAD` passed.
- Manually compared old `master:skills/tatr/SKILL.md` against the new
  references.

Could not verify:

- User acceptance of the manual DoD item.

## Round 2

- REVIEWER: out-of-context
- VERDICT: APPROVE

Verified:

- R1.1 is resolved: `skills/tatr/records.md` now has `## Schema Shapes`
  covering `TASK.md`, `SPIKE.md`, `DECISION.md`, `REVIEW.md`, and `RETRO.md`.
- The documented title prefixes, required fields, and sections match
  `RECORD_SCHEMAS[]` in `tatr.c`.
- `tatr check --ledger LESSONS.md` passed.
- `git diff --check master...HEAD` passed.
- `wc -w -l skills/tatr/SKILL.md` still reports 48 lines, 365 words.
- `nix eval --raw .#skills.tatr` exports the full skill directory, and
  exported `records.md` includes `## Schema Shapes`.
