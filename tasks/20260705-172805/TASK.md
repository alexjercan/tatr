# Add rm command to remove a task

- STATUS: CLOSED
- PRIORITY: 80
- TAGS: feature

Agents that create scratch or mistaken tasks need a way to remove them without
shelling out to `rm -rf`. Add a `rm` subcommand that deletes a task directory.

## Behaviour

```
tatr rm <ID>
```

- Takes a positional HUID argument.
- Resolves and removes the whole `tasks/<ID>/` directory (the `TASK.md` and its
  containing dir).
- Exits non-zero on bad ID or missing task; prints a confirmation line on
  success.
- Refuses to remove anything outside the located `tasks/` dir (only operate on
  a validated HUID subdirectory).

## Steps

- [ ] Reuse the HUID-resolution helper to validate the ID and confirm the task
      exists before deleting.
- [ ] Implement `command_rm`: remove `TASK.md` then the task directory (use the
      available POSIX/aids.h calls; keep it to the single task dir).
- [ ] Wire `rm` into dispatch and help.
- [ ] Add tests in `checker.sh`: rm an existing task (gone afterwards), rm a
      bad/missing ID errors, and confirm sibling tasks are untouched.
- [ ] Run `make` and `./checker.sh`; all green.
