# Claims, Frontier, Worktrees

## Frontier

`tatr frontier <epic-id>` prints open children:

```text
<READY|BLOCKED|CLAIMED><TAB><id><TAB>p<priority><TAB><flow-step><TAB><title>
```

BLOCKED rows add `blocked-by=<ids>`. Order: state, priority descending, ID
ascending. Closed children and non-children are absent.

## Claims

- `tatr claim <id>`: atomic claim; one racing session wins.
- `tatr release <id>`: release this session's claim.
- `tatr release <id> --force`: recover another session's stale claim.
- `tatr claims`: list claims.
- Expiry: none.
- Ownership: `TATR_SESSION`; default working directory. Never a PID.
- Storage: `TATR_CLAIMS_DIR`; default `<tasks-dir>/.claims`.

`tatr flow <id> --to WORKING` refuses a foreign claim.

Parallel worktrees require one shared `TATR_CLAIMS_DIR`; per-tree defaults do
not coordinate. Create the worktree before `tatr new` so the task starts on its
branch. If created in the shared checkout, move the stub to the worktree first.
