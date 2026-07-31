# Check Rules

`tatr check [id]` prints `id: rule: detail`. Any finding -> exit 1. Clean ->
exit 0 with no output. `--ledger FILE` also checks lesson promotions.

## Records and review

| Rule | Finding |
|---|---|
| `bad-record-schema` | Wrong title, missing/empty required header, or missing/empty required section. Covers TASK, SPIKE, DECISION, REVIEW, RETRO. |
| `bad-review-round` | Missing Round 1 or non-contiguous rounds. |
| `bad-verdict` | Missing verdict or value outside APPROVE/REQUEST_CHANGES. |
| `missing-reviewer` | Missing or empty reviewer. |
| `bad-finding-id` | Wrong round/index or skipped index. |
| `bad-severity` | Severity outside BLOCKER/MAJOR/MINOR/NIT. |
| `approve-with-open-findings` | Latest verdict APPROVE with open BLOCKER or MAJOR. |
| `bad-proof-syntax` | DoD item lacks `test:`, `cmd:`, or `manual:`. |
| `missing-spike-record` | Planned SPIKE task lacks `SPIKE.md`. |
| `bad-spike-status` | Status outside RECOMMENDED/INCONCLUSIVE/DROPPED. |
| `dangling-seeded-task` | Next-step task ID in `SPIKE.md` has no task. |
| `dangling-decision-task` | `DECISION.md` TASK has no task. |
| `bad-decision-status` | Status is neither ACCEPTED nor `SUPERSEDED by <ref>`. |
| `dangling-supersede` | Supersede reference has no `DECISION.md`. |
| `nonreciprocal-supersede` | Supersede links disagree. |

## Task graph and lifecycle

| Rule | Finding |
|---|---|
| `malformed-header` | Missing/unreadable TASK or invalid exact metadata token. |
| `missing-parent`, `missing-dependency` | Referenced task missing. |
| `self-parent`, `self-dependency` | Task references itself. |
| `duplicate-dependency` | Repeated dependency. |
| `parent-cycle`, `dependency-cycle` | Link cycle. |
| `bad-epic-relationship` | Parent is not an EPIC, or STORY lacks a parent. |
| `unplanned-in-progress` | IN_PROGRESS non-EPIC lacks approved plan. |
| `closed-unchecked` | CLOSED non-EPIC has unchecked Steps. |
| `closed-missing-review`, `closed-missing-retro` | CLOSED non-EPIC lacks record. |
| `closed-not-approved` | CLOSED non-EPIC lacks latest APPROVE verdict. |

## Ledger

| Rule | Finding |
|---|---|
| `promotion-stalled` | x3+ lesson remains outside Pending promotions without a lifecycle marker. |
| `promotion-awaiting-decision` | Pending entry has bare count, or recurrence passed its DEFER count. |
| `bad-disposition` | Pending annotation violates disposition grammar. |
| `dangling-promotion-task` | PROMOTE target is missing. |

## Scope and exemptions

- SPIKE and DECISION content rules: only when the sibling exists.
- `missing-spike-record`: only planned `KIND: SPIKE`.
- TASK/STORY plan sections: required from PLANNED onward.
- EPIC body sections: Done Means and Child Tasks.
- SPIKE body section: Question; planned SPIKE also needs `SPIKE.md`.
- Historical exemption: `- <task-id> <rule>: <reason>` in `tasks/EXEMPTIONS.md`.
- New work: fix or scaffold; never exempt.
- `unused-exemption`: exemption never matched a finding.
