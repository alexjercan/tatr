# Retro: add rm command (task 20260705-172805)

## What went well
- Three tasks in, `task_resolve` has proven to be the right abstraction: `rm`
  needed exactly the isdir existence check it already does, so validation was
  free and consistent with show/edit.
- Reusing `cleanup_string_slice_array` for the listdir result kept rm leak-free
  on the first valgrind run.

## Difficulties / gotchas
- `aids_io_listdir` does not filter `.` and `..`, so rm had to skip them
  explicitly; otherwise `unlink(".")` would fail. Worth remembering that this
  helper returns raw dirents.
- This repo's own task dirs contain a REVIEW.md alongside TASK.md, so a naive
  `unlink(TASK.md) + rmdir` would have failed on a non-empty dir. rm enumerates
  and removes all entries instead. Added a test that plants an extra file to
  lock this behaviour in.

## Lessons
- When a command deletes on disk, gate every path behind the validated HUID and
  only ever touch `tasks/<id>/` - never build a delete path from raw user input.
- The three feature commands now share one resolve/serialize/save spine; the
  docs task should describe that spine so future commands follow it.
