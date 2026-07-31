# Task Format

Use `tatr new` and `tatr edit` for metadata. Hand-edit body content.

```markdown
# Task Title

- STATUS: OPEN
- PRIORITY: 100
- TAGS: feature, security
- KIND: TASK
- FLOW STEP: BACKLOG
- PLAN STATUS: DRAFT
- PARENT: 20260730-153122
- DEPENDS ON: 20260730-153325, 20260730-154745

<body>
```

- First six fields: required, fixed order.
- PARENT and DEPENDS ON: optional, same task tree only.
- Values: exact and case-sensitive.
- Priority: non-negative; higher = more important relative to this backlog.

| Field | Values |
|---|---|
| STATUS | OPEN, IN_PROGRESS, CLOSED |
| KIND | TASK, EPIC, STORY, SPIKE |
| FLOW STEP | BACKLOG, UNDERSTANDING, PLANNING, PLANNED, WORKING, REVIEWING, COMPOUNDING, DONE |
| PLAN STATUS | DRAFT, APPROVED, NOT_REQUIRED |

`flow` owns STATUS, FLOW STEP, and PLAN STATUS. Relationship writes validate
existence, STORY parent requirements, and EPIC parent kind before mutation.

Body bytes after metadata remain opaque and preserved. Blank lines between
fields normalize on write. Empty keys such as `- PARENT:` fail parsing.
Legacy records without KIND or with `## Flow State` require manual repair.
`ls` skips malformed records, reports them on stderr, and exits non-zero.

Suggested non-trivial body:

```markdown
## Story

<cold-start context and outcome>

## Steps

- [ ] Concrete action.

## Definition of Done

- Observable outcome. (test: <name>)
- Observable outcome. (cmd: <command>)
- Observable outcome. (manual: <user check>)

## Notes

- Constraints, paths, sequencing, assumptions.
```
