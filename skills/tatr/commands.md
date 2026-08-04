# Commands

```text
tatr [-r ROOT] <command> [options]

new "Title" [-p N] [-t tags] [-b file] [-P id] [-d ids]
ls [-s created|priority|title] [-R] [-f query]
show <id>
edit <id> [-T title] [-p N] [-t tags] [-P id] [-d ids]
flow <id> [-n|--dry-run]
rewind <id> -o|--to ACTIVITY [-F|--force]
close <id> -x|--resolution R [-O|--of id] [-R|--reason text]
reopen <id>
rm <id>
scaffold <id> [TASK|DECISION|REVIEW|RETRO] [-l|--list] [-n|--dry-run]
proofs <id> [-k|--kind test|cmd|manual]
frontier <id>
context <id> [-P|--phase understand|plan|work|review|compound|resume|landing]
claim <id>
release <id> [-F|--force]
claims
check [id]
migrate [-a|--apply]
```

Use `tatr <command> --help` for exact flags.

| Command | Behavior |
|---|---|
| `new` | Creates `tasks/<id>/TASK.md`; defaults to priority 0 and ACTIVITY/GATES/RESOLUTION unset. `-b -` reads stdin. Same-second collision fails. Validates relationships before writing. |
| `ls` | Prints path, priority, derived status, activity, tags, title. `-R` finds nested `tasks/`. Malformed records go to stderr and cause non-zero exit. |
| `show` | Prints the full record and clickable path. |
| `edit` | Changes passed fields only; preserves body. Tags and dependencies replace their lists. Empty `-P` or `-d` clears them. |
| `flow` | Advances one activity; runs and records that activity's exit gate. May half-succeed: gate recorded, cursor held. A refused gate leaves the record unchanged. |
| `rewind` | Moves the cursor backward only; runs no gate; clears the gates the move invalidates, `--force` when it carries one. |
| `close` | Sets RESOLUTION. `DONE` runs the close gate; the others run none and work from any activity. |
| `reopen` | Clears RESOLUTION and everything `close` wrote with it: `- DUPLICATE OF:` and a trailing `## Dropped` block. Cursor and gates stay. |
| `rm` | Resolves a validated HUID, then deletes only its task directory. |
| `scaffold` | Creates a missing DECISION, REVIEW, or RETRO record. TASK is listed but refused: use `new`. Never overwrites. `--list` reports presence; `--dry-run` writes nothing. |
| `proofs` | Prints DoD proofs as tab-separated data. Never executes them. |
| `frontier` | Lists a task's open children by readiness. |
| `context` | Lists phase-relevant paths as `path<TAB>present|missing`; never reads contents. |
| `claim`, `release`, `claims` | Coordinate parallel sessions. |
| `check` | Prints `id: rule: detail`; exit 1 on findings, 0 with no output when clean. |
| `migrate` | Converts legacy records in place; dry-run without `--apply`. |

`-k/--kind` is gone from `new` and `edit`, and says so by name: there is one
kind of record. Workflow fields are not `new` or `edit` options either. `ACTIVITY` moves with `flow`
and `rewind`, `GATES` is written by `flow` alone, `RESOLUTION` by `close` and
`reopen`. `STATUS` is derived and not settable anywhere.
