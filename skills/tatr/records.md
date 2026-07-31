# Records

Sibling records: `SPIKE.md`, `DECISION.md`, `REVIEW.md`, `RETRO.md`; optional
free-form notes such as `NOTES.md`.

Create schema records with `tatr scaffold <id> <kind>`. The same table powers
`tatr scaffold`, `tatr check`, and lifecycle gates. Existing file: edit by hand;
no overwrite flag.

## Required shapes

| Record | Header | Sections |
|---|---|---|
| `TASK.md` | `# title`; typed metadata from `format.md` | TASK/STORY: Steps, Definition of Done. EPIC: Done Means, Child Tasks. SPIKE: Question. |
| `SPIKE.md` | `# Spike:`; DATE, STATUS, TAGS | Question, Context, Options considered, Recommendation, Open questions, Next steps |
| `DECISION.md` | `# Decision:`; DATE, STATUS, TASK, TAGS | Context, Decision, Alternatives considered, Consequences |
| `REVIEW.md` | `# Review:`; TASK, BRANCH | Sequential `## Round N`; REVIEWER; VERDICT; findings |
| `RETRO.md` | `# Retro:`; TASK, BRANCH, REVIEW ROUNDS | What went well, What went wrong, What to improve next time, Action items |

Required fields and sections need non-empty content. Defaults: SPIKE status
`RECOMMENDED`; DECISION status `ACCEPTED`.

Review values:

- Verdict: `APPROVE|REQUEST_CHANGES`.
- Finding: `- [ ] R<round>.<index> (BLOCKER|MAJOR|MINOR|NIT) file:line - detail`.
- Round and finding numbers: start at 1, remain contiguous.

## Proofs

`tatr proofs <id>` prints:

```text
<n><TAB><test|cmd|manual><TAB><text>
```

- Reads `## Definition of Done` proof markers.
- `-k` selects one kind.
- Never executes commands.
- Preserves intra-line spacing; collapses newline or tab runs for stable fields.

## Lessons ledger

- `tatr ledger`: list promotions awaiting a user decision.
- `--slug` plus `--disposition`: record the user's decision in the ledger only.
- Ask first. Never infer PROMOTE, DEFER, RETIRE, or ABSORBED.

| Disposition | Requires |
|---|---|
| PROMOTE | `--task <existing-id>` |
| DEFER | `--reason <text>`; returns to the queue after a later recurrence |
| RETIRE | `--reason <text>` |
| ABSORBED | `--target <tool-or-template>` |
