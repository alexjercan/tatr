# Retro: strict-check exemption for historical/goal tasks

## What went well

- Small, surgical change: one helper (`check_task_is_history_exempt`) reusing
  the already-parsed tag set, and a one-line guard on the strict block. No new
  allocation, no leak (memcheck 61/61).
- Chose tags (`historical`, `goal`) over a TASK.md annotation - tags are already
  parsed and trimmed, so the check is exact (`goalpost` won't match `goal`) and
  needs no new parsing. Covered both the pre-flow-backfill use case and the
  flow-umbrella case in one mechanism.
- Followed the regression discipline: neutralized the guard, rebuilt, watched
  the suite drop to 54/61, restored to 61/61 - proving the new test can fail.
- Kept the exemption surgical: only closed-missing-review/retro are skipped;
  closed-unchecked, closed-not-approved and bad-severity still fire on these
  tasks. Reviewer confirmed.

## What went wrong

- The task stub was untracked in the main checkout, so the sprouted worktree
  (from committed HEAD) did not have it - `tatr edit` failed with "not found"
  until I carried the stub into the worktree. The `sprout-inherits-committed-head`
  lesson again; I should commit task stubs before sprouting.

## What to improve next time

- Commit newly-created backlog task stubs before sprouting a worktree that needs
  to see them. When several stubs sit untracked in the main checkout, commit the
  unrelated ones as a backlog-planning commit and remove the one being worked
  (the branch reintroduces it) before landing, to keep the checkout clean for
  `sprout land`.

## Action items

- [x] Feature landed 807f7bc; unblocks nix.dotfiles task 20260720-220137.
- The installed `tatr` binary (home-manager) still predates this change until a
  rebuild; use the freshly-built binary for tasks that rely on the exemption.
