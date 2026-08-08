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
tatr new "Title" -p 100 -t bug
tatr ls --sort priority
tatr ls --filter ':status eq OPEN'
tatr edit <id> --status IN_PROGRESS
```

Use `-r ROOT` for another project. Valid statuses: `OPEN`, `IN_PROGRESS`,
`CLOSED`. Invalid task files are errors. Edit task bodies directly.
