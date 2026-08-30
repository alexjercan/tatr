---
name: tatr
description: Create, list, query, and edit Markdown tasks with the tatr CLI.
---

# Tatr

Task: `tasks/<YYYYMMDD-HHMMSS>/TASK.md`.

Format:

```markdown
# Title

- STATUS: OPEN
- PRIORITY: 0
- TAGS: tag1, tag2

Optional body.
```

Commands:

```bash
tatr new "Title" -p 100 -t bug -b details.md
printf 'Body from stdin.\n' | tatr new "Title" --body -
tatr ls --sort priority
tatr ls --filter ':status eq OPEN'
tatr edit <id> --status CLOSED
```

Use `-r ROOT` for another project. Valid statuses: `OPEN`, `CLOSED`. For
`new`, pass `-b FILE` to read the body from a Markdown file or `-b -` to read
stdin. Input read failure creates no task. Edit existing task
bodies directly.
