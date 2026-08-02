# Add edit command to update task metadata and title

- PRIORITY: 90
- TAGS: feature
- KIND: TASK
- ACTIVITY: COMPOUNDING
- GATES: REVIEW RETRO
- RESOLUTION: DONE

The README lists "No built-in task editing command" as a limitation. Agents
cannot open an interactive editor, so they need a non-interactive way to change
a task's status, priority, tags and title. This is the single most important
command for automation: it is how an agent moves a task to IN_PROGRESS and then
CLOSED.

## Behaviour

```
tatr edit <ID> [-s STATUS] [-p PRIORITY] [-t TAGS...] [-T TITLE]
```

- Takes a positional HUID argument plus optional flags mirroring `new`
  (`-s/--status`, `-p/--priority`, `-t/--tags`, and a title override).
- Loads the existing task, applies only the fields that were provided (others
  stay unchanged), and saves it back to the same file with `task_save`.
- The description body must be preserved untouched.
- Exits non-zero on bad ID, missing task, or invalid field values (e.g. an
  unknown status string).

## Steps

- [x] Reuse the HUID-resolution helper added by the `show` task to locate and
      load the task.
- [x] Add optional-with-default detection so unspecified flags do not clobber
      existing values (argparse.h: check whether each option was actually set).
- [x] Implement `command_edit`; validate the status string before applying.
- [x] Save via `task_save`; confirm the description body round-trips unchanged.
- [x] Wire `edit` into dispatch and help.
- [x] Add tests in `checker.sh`: change status, change priority, change tags,
      change title, partial update leaves other fields intact, bad status
      rejected, bad/missing ID errors.
- [x] Run `make` and `./checker.sh`; all green.
