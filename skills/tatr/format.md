# Task File Format

Keep the metadata header exact; tatr owns it:

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

<free-form description body>
```

The first six fields are required and always written back in that order.
`PARENT` and `DEPENDS ON` are optional and appear only when set.

Valid values are exact and case-sensitive:

- `STATUS`: `OPEN|IN_PROGRESS|CLOSED`
- `KIND`: `TASK|EPIC|STORY|SPIKE`
- `FLOW STEP`: `BACKLOG|UNDERSTANDING|PLANNING|PLANNED|WORKING|REVIEWING|COMPOUNDING|DONE`
- `PLAN STATUS`: `DRAFT|APPROVED|NOT_REQUIRED`

Priority is a non-negative integer; higher means more important. Slot it
relative to the backlog, not an absolute scale. Check project tag conventions
before tagging.

`PARENT` and `DEPENDS ON` hold task ids from the same `tasks/` tree. A parent
in another repository belongs in body prose. Creation and edits that touch
relationships validate the referenced tasks, STORY parent requirements, and
EPIC parent kind before writing.

There is no migration path from the older format that had no `KIND` line and
kept flow state under `## Flow State`. Such records are rejected; correct them
by hand.

Everything after the metadata block is opaque body text and is preserved byte
for byte. Blank lines and leading whitespace between fields are tolerated and
normalized on the next write. A key with no value, such as `- PARENT:`, is an
error, not body text.

tatr never writes a record it cannot read back: `new` and `edit` re-parse the
serialized bytes before writing and fail without touching disk if the result
would not parse.

`tatr ls` skips malformed records, names them on stderr, and exits non-zero so
one broken record cannot hide the rest of the backlog.

For non-trivial tasks, use:

```markdown
## Story

As a <who>, I want <what>, so that <why>.

## Steps

- [ ] Concrete, verifiable actions.

## Definition of Done

- Observable outcome. (test: <name>)
- Observable outcome. (cmd: <command>)
- Observable outcome. (manual: <what the user confirms>)

## Notes

- Constraints, file pointers, sequencing, and assumptions.
```

Prefer `tatr edit` for metadata and `tatr new -b` for the initial body. Hand
edit later body updates.
