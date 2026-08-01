# Commands

```text
tatr [-r ROOT] <command> [options]

new "Title" [-p N] [-t tags] [-b file] [-k KIND] [-P id] [-d ids]
ls [-s created|priority|title] [-R] [-f query]
show <id>
edit <id> [-T title] [-p N] [-t tags] [-k KIND] [-P id] [-d ids]
flow <id> [-o|--to STEP]
rm <id>
scaffold <id> [TASK|SPIKE|DECISION|REVIEW|RETRO] [-l|--list] [-n|--dry-run]
proofs <id> [-k|--kind test|cmd|manual]
frontier <id>
context <id> [-P|--phase understand|plan|work|review|compound|resume|landing]
claim <id>
release <id> [-F|--force]
claims
check [id]
```

Use `tatr <command> --help` for exact flags.

| Command | Behavior |
|---|---|
| `new` | Creates `tasks/<id>/TASK.md`; defaults to OPEN, priority 0, TASK, BACKLOG, DRAFT. `-b -` reads stdin. Same-second collision fails. Validates relationships before writing. |
| `ls` | Prints path, priority, kind, flow step, tags, title. `-R` finds nested `tasks/`. Malformed records go to stderr and cause non-zero exit. |
| `show` | Prints the full record and clickable path. |
| `edit` | Changes passed fields only; preserves body. Tags and dependencies replace their lists. Empty `-P` or `-d` clears them. |
| `flow` | Takes the next edge or named valid edge. Refusal leaves the record unchanged. |
| `rm` | Resolves a validated HUID, then deletes only its task directory. |
| `scaffold` | Creates a missing SPIKE, DECISION, REVIEW, or RETRO record. TASK is listed but refused: use `new`. Never overwrites. `--list` reports presence; `--dry-run` writes nothing. |
| `proofs` | Prints DoD proofs as tab-separated data. Never executes them. |
| `frontier` | Lists open Epic children by readiness. |
| `context` | Lists phase-relevant paths as `path<TAB>present|missing`; never reads contents. |
| `claim`, `release`, `claims` | Coordinate parallel sessions. |
| `check` | Prints `id: rule: detail`; exit 1 on findings, 0 with no output when clean. |

Workflow fields are not `new` or `edit` options. Use `flow` for `STATUS`,
`FLOW STEP`, and `PLAN STATUS`.
