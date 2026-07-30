# Review: Add typed v2 workflow schema and correct tatr history by hand

- TASK: 20260730-153325
- BRANCH: feat/v2-schema

## Round 1

- REVIEWER: out-of-context agent
- COMMIT: cddc77f
- VERDICT: REQUEST_CHANGES

Build clean, 77/77 native and memcheck with zero leaks, `tatr check --ledger
LESSONS.md` exit 0, no non-ASCII punctuation. Ownership of argv-backed slices
confirmed safe (argparse stores raw `argv` pointers and frees nothing).

### Findings

- [x] R1.1 (MAJOR) tatr.c:247 - `tatr new -b <file>` writes a record tatr
      itself cannot read, and exits 0. If the body's first line is
      metadata-shaped (`- NOTE: ...`), the file is written and every later
      read fails. Violates AGENTS.md's "validate before writing" and breaks
      the branch's own round-trip invariant. Reproduced independently.
- [x] R1.2 (MAJOR) tatr.c:1781 - a single unparseable record makes `tatr ls`
      list nothing and still exit 0. Structurally pre-existing, but v2 turns
      it from unreachable into the guaranteed first experience, since hand
      correction is the only migration path and `ls` is how a user would find
      the broken record. The warning also printed `(null)`.
- [x] R1.3 (MINOR) tatr.c:591 - the unknown-metadata guard only inspects the
      first body line, so a correctly spelled field one line deeper is still
      silently swallowed. The guard cost a false positive without delivering
      its guarantee.
- [x] R1.4 (MINOR) tatr.c - the parser tolerates blank lines and indentation
      between fields while README claims the block is read "in exactly this
      order", and normalizes the author's layout away on the next write.
- [x] R1.5 (MINOR) tatr.c:595 - a valueless `- PARENT:` reported "unknown
      metadata field", naming a field that is in the list the message prints.
- [x] R1.6 (MINOR) tatr.c:444 - a CRLF record yields `invalid STATUS 'OPEN'`,
      which reads as self-contradictory; the retired rule's "whitespace and
      line endings count" hint was dropped from every user-visible message
      while CHANGELOG and SKILL.md still claimed it.
- [x] R1.7 (MINOR) checker.sh:740 - the new tests miss the cases that broke: a
      body whose FIRST line is metadata-shaped, CRLF, an empty `- DEPENDS ON:`
      line, that a `goal` tag no longer grants the exemption, and a real
      parent/child pair (the filter test used a hardcoded ID that matched
      nothing, and asserted line counts rather than contents).
- [x] R1.8 (MINOR) tasks/ - the relationship fields are barely dogfooded, and
      the `- TAGS: goal` to `- TAGS: flow` rewrite rode along undocumented, so
      a saved `:tags contains goal` query silently stops matching.
- [x] R1.9 (NIT) tatr.c:206 - `task_cleanup` frees `_buffer` without nulling.
- [x] R1.10 (NIT) tatr.c:762 - `task_save`'s write-failure path frees
      `serialized_task.str` and then `return_defer` frees it again
      (pre-existing on master).
- [x] R1.11 (NIT) tatr.c:1226 - `task_print_full` logs and returns on
      serialize failure while `main_show` still exits 0.
- [x] R1.12 (NIT) duplicate `DEPENDS ON` entries are silently preserved.
- [x] R1.13 (NIT) checker.sh:834 - `reject_bad_value` is defined inside a test
      function and leaks into global shell scope.

The 31 hand-corrected records were found clean: classification consistent, no
body damaged, the diff purely inserted metadata plus three `## Flow State`
heading removals.

### Resolution (commit 097cdaa)

R1.1 and R1.3 were one problem. The guard was deleted rather than narrowed:
the body is opaque again, and the invariant it reached for moved to the write
side, where `task_save` re-parses the serialized bytes and refuses to write
what would not read back. That also closes a case neither round found - a
newline in a title or tag - and `task_create` now removes the directory it
created when the save fails. R1.5 kept its own diagnostic via
`starts_with_empty_field`, since hand correction is the only migration path.

R1.2: `load_tasks_from_dir` skips an unreadable record, names it, and reports
the count; `ls` exits non-zero. R1.4 was resolved by correcting the docs to
match the parser rather than tightening the parser. R1.6, R1.9 through R1.13
applied as suggested. R1.7 added two tests and extended three. R1.8 recorded
the tag rewrite in DECISION.md and CHANGELOG; the local EPIC record was
deliberately NOT created, because the parent Epic genuinely lives in another
repository and a fake local one would dogfood the field by lying.

## Round 2

- REVIEWER: out-of-context agent (same reviewer, re-run against the fixes)
- COMMIT: 097cdaa
- VERDICT: APPROVE

All round 1 findings verified fixed by driving the binary, not by reading the
diff. The reviewer independently re-checked the paths the fixes touched and
ran `valgrind --leak-check=full --show-leak-kinds=all
--errors-for-leak-kinds=all` over five scenarios (ls-with-skip, verify-failure
plus rmdir, verify-success with parent/tags/depends, check on a mixed dir,
edit on a record with a valueless PARENT): zero errors, all blocks freed.
`task_save`'s verify path was confirmed free of double-free: on parse failure
`task_deserialize`'s own cleanup runs while `_buffer` is NULL and
`verify_initialized` stays false. R1.4 (docs softened, parser left tolerant)
was explicitly endorsed as the right contract for a format whose only
migration path is hand editing. R1.8 (no fake local EPIC record) accepted.
The reviewer withdrew its own NIT about `local x=$(run_tatr ...)`.

### Findings

- [x] R2.1 (MINOR) tatr.c:1119 - `main_new` reported a stale
      `aids_failure_reason()` left over from the successful collision
      pre-check, so a refused write printed "No such file or directory",
      contradicting the real cause logged two lines above. Same class at
      tatr.c:1455 (`Failed to save task: (null)`).
- [x] R2.2 (NIT) `- DEPENDS ON: ` (trailing space, empty) was silently
      accepted while `- DEPENDS ON:` was a hard error: two visually identical
      hand-edits behaving oppositely on an invisible byte.
- [x] R2.3 (NIT) README did not mention that `ls` now exits non-zero when a
      record is skipped, though CHANGELOG and SKILL.md did - a user-visible
      break for `tatr ls && ...` scripts.
- [x] R2.4 (NIT) `task_create`'s `snprintf(...) > 0` did not detect
      truncation; same pre-existing pattern in `main_rm`, where the path is
      passed to `unlink`.
- [x] R2.5 (NIT) a `task_file_path_build` failure between `mkdir` and
      `task_save` could still strand an empty directory.
- [ ] R2.6 (NIT) `- PARENT:20240101-000000` (no space after the colon)
      silently becomes body text. Not fixed: only a hand-edit can produce it,
      the write-side verify means tatr never writes it, and catching it needs
      back the shape heuristic that caused R1.1. Recorded as a known edge.
- [ ] R2.7 (NIT) `reject_bad_value` reads `$task_file`/`$id` from the caller
      and writes globals. Not fixed: the header comment documents the
      contract and there is one caller.

### Resolution (commit 3rd)

R2.1: both call sites drop `aids_failure_reason()`, with a comment saying why.
R2.2: `DEPENDS ON` with an empty value now takes the same diagnostic as
`PARENT`, and the empty-field test loops over both spellings. R2.3: README's
`ls` section states the exit-code change. R2.4: both snprintf sites check
`>= sizeof(buffer)`. R2.5: the file path is built before the mkdir.
