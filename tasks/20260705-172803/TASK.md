# Add show command to display a single task by ID

- STATUS: CLOSED
- PRIORITY: 100
- TAGS: feature

Agents driving the plan-work-review cycle need to read the full details of a
single task by its HUID, not just the one-line `ls` summary. Add a `show`
subcommand.

## Behaviour

```
tatr show <ID>
```

- Takes a single positional HUID argument (format `YYYYMMDD-HHMMSS`).
- Resolves the task file `tasks/<ID>/TASK.md` using the same upward search for
  the `tasks/` directory that `new` and `ls` use, plus the global `-r ROOT`.
- Loads and prints the full task: title, status, priority, tags and the full
  description body (i.e. the raw TASK.md content, or a cleanly formatted
  rendering of it). Include the clickable file path like `ls` does.
- Exits non-zero with a clear error if the ID is not a valid HUID or the task
  does not exist.

## Steps

- [ ] Add a helper that resolves a HUID argument to a task file path within the
      located `tasks/` dir and loads the `Task` (reuse `task_file_path_build`,
      `ishuid`, `task_load`). This helper will be shared by `edit` and `rm`.
- [ ] Implement `command_show` following the argparse.h subcommand pattern used
      by `command_new`/`command_ls`.
- [ ] Wire `show` into the subcommand dispatch and the top-level usage/help.
- [ ] Add tests in `checker.sh`: show an existing task, show with a bad ID,
      show a non-existent ID.
- [ ] Run `make` and `./checker.sh` (and `--memcheck` if valgrind is present);
      all green.
