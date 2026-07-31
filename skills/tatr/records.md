# Tatr Records

Sibling records live next to `TASK.md`: `SPIKE.md`, `DECISION.md`,
`REVIEW.md`, `RETRO.md`, and optional notes such as `NOTES.md`.

Use `tatr scaffold <id> <RECORD>` for `SPIKE`, `DECISION`, `REVIEW`, and
`RETRO`. It writes from the schema table in `tatr.c`, the same table
`tatr check` validates. It prints `<path><TAB><RECORD>`, refuses to overwrite
an existing record, and has no `--force`. Edit existing records by hand so the
diff shows the repair.

`tatr scaffold <id> --list` prints each record kind with its path and
`present` or `missing`. `--dry-run` prints what would be written.

## Schema Shapes

All records open with a required title prefix and then required `- KEY: `
header fields with non-empty values. Section headings must each have at least
one non-blank line under them.

- `TASK.md`: title prefix `# `. Required metadata is the typed `TASK.md`
  header from `format.md`. Required body sections are kind-specific:
  `TASK`/`STORY` -> `## Steps`, `## Definition of Done`; `EPIC` ->
  `## Done Means`, `## Child Tasks`; `SPIKE` -> `## Question`.
- `SPIKE.md`: title prefix `# Spike: `. Fields: `- DATE: `, `- STATUS: `,
  `- TAGS: `. Sections: `## Question`, `## Context`,
  `## Options considered`, `## Recommendation`, `## Open questions`,
  `## Next steps`. Scaffold defaults `STATUS` to `RECOMMENDED`.
- `DECISION.md`: title prefix `# Decision: `. Fields: `- DATE: `,
  `- STATUS: `, `- TASK: `, `- TAGS: `. Sections: `## Context`,
  `## Decision`, `## Alternatives considered`, `## Consequences`. Scaffold
  defaults `STATUS` to `ACCEPTED`.
- `REVIEW.md`: title prefix `# Review: `. Fields: `- TASK: `, `- BRANCH: `.
  Body is round-structured: `## Round 1`, `- REVIEWER: `,
  `- VERDICT: APPROVE|REQUEST_CHANGES`, and findings shaped as
  `- [ ] R<round>.<index> (BLOCKER|MAJOR|MINOR|NIT) file:line - detail`.
- `RETRO.md`: title prefix `# Retro: `. Fields: `- TASK: `, `- BRANCH: `,
  `- REVIEW ROUNDS: `. Sections: `## What went well`,
  `## What went wrong`, `## What to improve next time`, `## Action items`.

`tatr proofs <id>` prints each `## Definition of Done` proof as:

```text
<n><TAB><kind><TAB><text>
```

Kinds are `test`, `cmd`, and `manual`. Nothing is executed. A `cmd:` proof's
shell text round-trips verbatim and running it is the caller's decision.
Whitespace collapses only when it contains a newline or tab, so each output
line remains three fields while intra-line spacing survives.

`tatr ledger` lists promotions awaiting a user decision from `LESSONS.md` by
default. With `--slug` plus `--disposition`, it records the decision inside the
entry's count parens and writes only the ledger file. Ask the user before
calling it. PROMOTE requires `--task <id>` naming an existing task. DEFER and
RETIRE require `--reason`; ABSORBED requires `--target`. DEFER may be revisited
after the lesson count passes the deferred count; other dispositions are
settled.
