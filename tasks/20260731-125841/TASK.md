# Tighten tatr skill references

- PRIORITY: 0
- TAGS: docs, skill
- ACTIVITY: COMPOUNDING
- GATES: PLAN REVIEW RETRO
- RESOLUTION: DONE

## Story

Tighten `skills/tatr/` for fast agent loading without losing workflow rules.

## Steps

- [x] Verify skill claims against CLI, README, tests, and prior task 20260731-115401.
- [x] Rewrite `SKILL.md` near 300 words and compress every reference.
- [x] Validate content, word counts, ASCII, links, skill metadata, and project checks.

## Definition of Done

- `SKILL.md` stays near 300 words; each reference stays below 600. (cmd: `wc -w skills/tatr/*.md`)
- All skill Markdown is concise, routed, accurate, and ASCII-only. (manual: inspect `skills/tatr/*.md`)
- Skill validation and project checks pass. (cmd: `python /home/alex/.codex/skills/.system/skill-creator/scripts/quick_validate.py skills/tatr && dist/tatr check --ledger LESSONS.md`)

## Notes

- Documentation-only change. Preserve CLI behavior and safety constraints.
- User requested fragments, bullets, flat structure, and essential information only.
- Result: 455 -> 377 lines; 2823 -> 2206 words across skill Markdown.
- `SKILL.md`: 287 words. References: 112-417 words each.
- Replaced repeated prose with command/rule tables and direct routing.
- Source audit caught two draft errors: filter operator support and TASK scaffold refusal.
- Tradeoff: fewer edge-case explanations; CLI help and gate diagnostics remain canonical.
- Verification: skill validator, ASCII/link checks, `git diff --check`, tatr ledger check,
  and 107/107 integration tests pass.
- Next time: derive compact operator matrices directly from type-check branches before drafting.
