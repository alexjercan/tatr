# Review: AGENTS.md and documentation

- VERDICT: APPROVE

## Scope reviewed
- New `AGENTS.md` at the repo root.
- README updates: new `show`/`edit`/`rm` sections, a Filtering subsection under
  Listing, refreshed Features and Limitations, stale line-count removed.

## Findings

### Accuracy
- Every documented invocation was checked against the built binary: the
  `show`/`edit`/`rm` examples, the `edit` option list, and the `ls -f` filter
  examples all produce the described behaviour.
- The two stale limitations ("No built-in task editing command", "No filtering
  ... yet") are removed - both are now false (`edit` and `ls -f` exist). The
  "~930 lines" claim was replaced (the file is now ~2680 lines) with a
  version-independent description.
- AGENTS.md build/test instructions match reality: `make` defaults to clang,
  `make CC=gcc` works, and the suite runs via `nix develop -c ./checker.sh`
  (with `--memcheck`). The documented `task_resolve` / load-mutate-save spine
  matches the code.

### Notes
- I introduced a Filtering subsection rather than leave a dangling "see the
  filter documentation" reference; the filter feature predates this work but was
  undocumented, so documenting it here removes the dangling pointer and is a net
  improvement.
- Pre-existing box-drawing characters remain in the README's directory-tree
  diagram; my additions are plain ASCII per the repo convention.

## Tests
- No code change; full suite (45/45) still green under `--memcheck`.
