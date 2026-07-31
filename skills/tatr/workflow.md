# Tatr Workflow

Picking up work:

1. Run `tatr ls --sort priority` or `tatr ls -f '(:status eq OPEN)' --sort priority`.
2. Run `tatr show <id>` and read the full task plus relevant sibling records.
3. Use `tatr context <id> --phase <phase>` to list only the artifacts needed
   for the current phase.
4. Start with `tatr flow <id> --to WORKING`; it refuses missing approval, open
   dependencies, and conflicting claims.
5. Append implementation notes to the task record or sibling notes as you go.

Finishing work:

1. Run the project's tests and `tatr check --ledger LESSONS.md` when the repo
   has a lessons ledger.
2. Treat `promotion-awaiting-decision` as a user question, not a decision for
   the agent.
3. Record what changed and why, alternatives considered, difficulties and
   diagnosis, and short self-reflection in the task record or retro, following
   the repo's `AGENTS.md`.
4. Walk `WORKING -> REVIEWING -> COMPOUNDING -> DONE`. A refusal lists exactly
   what is missing.
5. Commit task changes together with code or docs changes.

Planning work:

- Keep one cohesive requested thing in one task.
- Split only when pieces are independently implementable and committable, or
  when the user explicitly asks for an EPIC, sprint, version, release, or
  multi-feature container.
- Use project-consistent tags.
- Create tasks for non-trivial follow-up work discovered mid-session instead
  of leaving TODO comments in code.

Gotchas:

- "No 'tasks' directory found" means create `tasks/` at the project root.
- A task only appears in `tatr ls` if its directory matches
  `YYYYMMDD-HHMMSS` and contains a well-formed `TASK.md`.
- Timestamps are local time.
- A record in a wrong state is repaired by hand in `TASK.md`; there is no
  `--force` or repair command.
- Same-second `tatr new` collisions fail. Run one `tatr new` per command and
  retry after the second changes.
