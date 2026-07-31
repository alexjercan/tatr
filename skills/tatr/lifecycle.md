# Lifecycle

```text
BACKLOG -> UNDERSTANDING -> PLANNING -> PLANNED -> WORKING -> REVIEWING -> COMPOUNDING -> DONE
```

- Bare `tatr flow <id>`: next step.
- Review fix loop: `tatr flow <id> --to WORKING`.
- `DONE`: terminal.
- Invalid, skipped, or backward edge: refusal; no write.

## Derived status

| Flow step | Status |
|---|---|
| BACKLOG through PLANNED | OPEN |
| WORKING through COMPOUNDING | IN_PROGRESS |
| DONE | CLOSED |

## Gates

| Edge | Requires |
|---|---|
| PLANNING -> PLANNED | Valid plan. Writes `PLAN STATUS: APPROVED`; sole approval marker. |
| PLANNED -> WORKING | Approved plan, closed dependencies, no claim owned by another session. |
| REVIEWING -> COMPOUNDING | Schema-clean `REVIEW.md`; latest verdict APPROVE; no open BLOCKER or MAJOR finding. |
| COMPOUNDING -> DONE | All earlier gates; all `## Steps` checked; schema-clean `RETRO.md`; valid `DECISION.md` status when present. |

Checked steps do not prove approval. `PLAN STATUS: NOT_REQUIRED` exists only
for pre-flow history and requires a manual repair.

`KIND: EPIC` exemptions: plan approval, review, retro, unchecked Steps.
Still enforced: dependencies and any present `DECISION.md`.
