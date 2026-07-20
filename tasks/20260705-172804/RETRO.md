# Retro: add edit command (task 20260705-172804)

## What went well
- The show retro paid off immediately: `task_resolve` dropped straight in, and
  every "should fail" test used the `set +e` / split-declaration pattern, so the
  suite went green on the first full run (no wasted iterations this time).
- The `new` command's field-parsing code was the right template for `edit`; the
  only real design decision was "apply only provided fields", which
  `argparse_get_value` returning NULL made trivial.

## Difficulties
- Preserving the description body needed a moment's thought: it works only
  because `task_load` keeps the parsed body in `task.description` (a slice into
  the loaded file buffer) and `task_save` re-serializes it. Confirmed with a
  dedicated test that appends a body and checks it survives a partial edit.
- Tag replacement: resetting `task.meta.tags.count = 0` and re-appending is safe
  because tag slices are not individually owned (they point into the loaded
  buffer or argv). valgrind confirmed no leak / double free.

## Lessons / follow-ups
- `edit` validates status strictly but `new` still silently defaults an unknown
  status to OPEN. Not fixed here (out of scope) - candidate follow-up task if we
  want the two commands symmetric.
- Keep reusing `task_resolve` for `rm`; it already does the isdir existence
  check `rm` needs.
