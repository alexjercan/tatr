# Claims, Frontier, and Worktrees

`tatr frontier <id>` prints open work under an EPIC, one tab-separated row per
child:

```text
<STATE><TAB><id><TAB>p<priority><TAB><flow step><TAB><title>
```

A blocked row appends `<TAB>blocked-by=<ids>`. STATE is `READY`, `BLOCKED`, or
`CLAIMED`. CLOSED children and non-children are omitted. Order is deterministic:
READY, BLOCKED, CLAIMED, then priority descending, then id ascending.

`tatr claim <id>`, `tatr release <id>`, and `tatr claims` coordinate parallel
sessions. A claim is an atomic `O_CREAT|O_EXCL` file, so one racing session
wins and losers are told who holds it. `tatr flow <id> --to WORKING` refuses a
task another session holds.

Ownership is `TATR_SESSION`, defaulting to the working directory. It is never a
pid because tatr is a one-shot CLI. `TATR_CLAIMS_DIR` defaults to
`<tasks dir>/.claims`; point parallel worktrees at one shared directory or each
tree will have isolated claims and the guard cannot work across them.

A session releases its own claim with no flag. `release --force` deliberately
recovers another session's claim. Nothing expires automatically.

When work will happen in a sprout worktree, create the worktree first and run
`tatr new` inside it so the task file is born on the branch. If a task stub was
unavoidably created in the shared main checkout, copy it into the worktree and
remove the main-checkout stub as the first worktree act.
