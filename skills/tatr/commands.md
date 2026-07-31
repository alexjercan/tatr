# Tatr Commands

```bash
tatr [-r ROOT] <subcommand> [options]

tatr new "Title" [-p <priority>] [-t tag1,tag2] [-b <file>] [<metadata options>]
tatr ls [-s created|priority|title] [-R] [-f '<query>']
tatr show <id>
tatr edit <id> [-T "New title"] [-p <priority>] [-t tag1,tag2] [<metadata options>]
tatr flow <id> [-o|--to <STEP>]
tatr rm <id>
tatr scaffold <id> [SPIKE|DECISION|REVIEW|RETRO] [-l|--list] [-n|--dry-run]
tatr proofs <id> [-k|--kind test|cmd|manual]
tatr frontier <id>
tatr context <id> [-P|--phase understand|plan|work|review|compound|resume|landing]
tatr claim <id>
tatr release <id> [-F|--force]
tatr claims
tatr check [<id>] [-L|--ledger <file>]
tatr ledger [-L|--ledger <file>] [-s|--slug <slug> -D|--disposition PROMOTE|DEFER|RETIRE|ABSORBED
            [-t|--task <id>] [-R|--reason <text>] [-T|--target <text>]]
```

`<metadata options>`, shared by `new` and `edit`:

```bash
-k|--kind TASK|EPIC|STORY|SPIKE
-P|--parent <id>
-d|--depends-on <id>
```

`STATUS`, `FLOW STEP`, and `PLAN STATUS` are not metadata options. They are
written only by `tatr flow`; `new` and `edit` reject `-s`, `-f`, and `-S`.

- `new` creates `tasks/<id>/TASK.md` and prints the id. Defaults are `OPEN`,
  priority 0, `KIND: TASK`, `FLOW STEP: BACKLOG`, `PLAN STATUS: DRAFT`, and no
  relationships. `-b/--body-file <file>` seeds the description body; `-`
  reads stdin. Same-second id collisions fail instead of overwriting.
  Relationship flags are validated before creating anything.
- `ls` prints `<filepath>: [PRIORITY: N, KIND: K, FLOW STEP: F, TAGS: ...]
  Title`. Sort by `created`, `priority`, or `title`; `-R` recurses into nested
  `tasks/`; `-f` uses the filter language.
- `show <id>` prints the whole serialized record with a clickable path.
- `edit <id>` updates only passed fields and preserves the body. `-t` and `-d`
  replace lists; empty `-P ""` or `-d ""` clears optional relationships.
- `flow <id>` moves the lifecycle forward, or to `--to <STEP>`, refusing
  unmet gates without modifying the file.
- `context <id> --phase <phase>` prints only artifact paths needed for the
  phase as `<path><TAB>present|missing`, never contents. Phases are
  `understand`, `plan`, `work`, `review`, `compound`, `resume`, and `landing`.
  `understand`, `plan`, and `resume` also include the parent EPIC `TASK.md`.
- `rm <id>` deletes the task directory after resolving a validated id.
- `check [<id>]` prints findings as `<id>: <rule>: <detail>`, exits 1 on any
  finding, and exits 0 with no output when clean. `--ledger` also checks the
  lessons ledger.
