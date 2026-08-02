# Task Format

Use `tatr new` and `tatr edit` for metadata. Hand-edit body content.

```markdown
# Task Title

- PRIORITY: 100
- TAGS: feature, security
- KIND: TASK
- ACTIVITY: -
- GATES: -
- RESOLUTION: -
- PARENT: 20260730-153122
- DEPENDS ON: 20260730-153325, 20260730-154745

<body>
```

- First six fields: required, fixed order. `-` means unset.
- DUPLICATE OF, PARENT and DEPENDS ON: optional, same task tree only.
- Values: exact and case-sensitive.
- Priority: non-negative; higher = more important relative to this backlog.

| Field | Values |
|---|---|
| KIND | TASK, EPIC, STORY, SPIKE |
| ACTIVITY | -, UNDERSTANDING, PLANNING, WORKING, REVIEWING, COMPOUNDING |
| GATES | -, or a space-separated subset of PLAN REVIEW RETRO in that order |
| RESOLUTION | -, DONE, WONTDO, DUPLICATE, SUPERSEDED |

STATUS is not a field: it is derived from ACTIVITY and RESOLUTION and printed,
never stored. `flow`, `rewind`, `close` and `reopen` own the three that are
stored. Relationship writes validate existence, STORY parent requirements, and
EPIC parent kind before mutation.

Body bytes after metadata remain opaque and preserved. Blank lines between
fields normalize on write. Empty keys such as `- PARENT:` fail parsing.
A record still carrying `- FLOW STEP: ` is v0: every command refuses it, and
`tatr migrate --apply` converts it. Other malformed headers require manual
repair.
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
