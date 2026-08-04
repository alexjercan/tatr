# Check Rules

`tatr check [id]` prints `id: rule: detail`. Any finding -> exit 1. Clean ->
exit 0 with no output.

## Records and review

| Rule | Finding |
|---|---|
| `bad-record-schema` | Wrong title, missing/empty required header, or missing/empty required section. Covers TASK, DECISION, REVIEW, RETRO. |
| `bad-review-round` | Missing Round 1 or non-contiguous rounds. |
| `bad-verdict` | Missing verdict or value outside APPROVE/REQUEST_CHANGES. |
| `missing-reviewer` | Missing or empty reviewer. |
| `bad-finding-id` | Wrong round/index or skipped index. |
| `bad-severity` | Severity outside BLOCKER/MAJOR/MINOR/NIT. |
| `approve-with-open-findings` | Latest verdict APPROVE with open BLOCKER or MAJOR. |
| `bad-proof-syntax` | DoD item lacks `test:`, `cmd:`, or `manual:`. |
| `dangling-decision-task` | `DECISION.md` TASK has no task. |
| `bad-decision-status` | Status is neither ACCEPTED nor `SUPERSEDED by <ref>`. |
| `dangling-supersede` | Supersede reference has no `DECISION.md`. |
| `nonreciprocal-supersede` | Supersede links disagree. |

## Task graph and lifecycle

| Rule | Finding |
|---|---|
| `malformed-header` | Missing/unreadable TASK, invalid exact metadata token, or a legacy record still carrying `- STATUS: `, `- FLOW STEP: ` or `- KIND: `. |
| `missing-parent`, `missing-dependency` | Referenced task missing. |
| `self-parent`, `self-dependency` | Task references itself. |
| `duplicate-dependency` | Repeated dependency. |
| `parent-cycle`, `dependency-cycle` | Link cycle. |
| `inconsistent-gates` | Cursor past an activity whose gate is not in GATES. Covers work started without the PLAN gate. |
| `closed-unchecked` | RESOLUTION: DONE has unchecked Steps. |
| `closed-missing-review`, `closed-missing-retro` | RESOLUTION: DONE lacks record. |
| `closed-not-approved` | RESOLUTION: DONE lacks latest APPROVE verdict. |
| `dropped-missing-reason` | RESOLUTION: WONTDO without a non-empty `- REASON:`. |
| `dropped-bad-superseder` | `- SUPERSEDED BY:` names no other task. |
| `dangling-duplicate-of` | `- DUPLICATE OF:` names no other task. |

## Scope and exemptions

- DECISION content rules: only when the sibling exists.
- Plan sections (Steps, Definition of Done): required once PLAN is in GATES.
  Same two for every task; having children changes nothing.
- Historical exemption, in `tasks/EXEMPTIONS.md`:
  `- <task-id> <rule>: <reason>` suppresses one rule;
  `- <task-id>: <reason>` suppresses every rule for that task.
- New work: fix or scaffold; never exempt.
- `unused-exemption`: exemption never matched a finding.
