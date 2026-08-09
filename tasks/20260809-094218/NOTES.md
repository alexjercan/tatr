# Notes: Non-alpha characters in filter tag literals

## Problem Statement

`tatr ls --filter` cannot match tags that contain `.`. The lexer ends a bare
identifier before the dot, then reports the dot as an invalid token.

Required punctuation is `.` and `-`. Other punctuation is out of scope.

## Context

- `tatr.c:1332-1334` starts a bare identifier with an alphanumeric character or
  `_`, then accepts only alphanumeric characters and `_`.
- The restriction affects all bare filter values, not tag storage. `new` and
  `edit` accept dotted tags.
- A prior closed task, `20260709-193044`, fixed this exact problem for `.` and
  `-`. Commit `1e4d16a` widened only the continuation character set. It kept
  token starts and field names unchanged.
- The v2 simplification in commit `202d9d2` removed that lexer helper, its
  integration test, and its README text. This reintroduced the bug.
- The current `checker.sh` has one combined list/filter test, but no dotted or
  dashed literal coverage.
- Current executable reproduction:
  - `:tags contains v0.1.0` exits 1 at the first dot.
  - `:tags contains release-candidate` exits 1 at the first dash.
- A disposable build of current `tatr.c` with the prior continuation rule
  proved these cases end to end:
  - `v0.1.0`, `release-candidate`, and `and-x` parse and match only their exact
    tags.
  - `.hidden` and `-urgent` remain invalid because punctuation cannot start a
    literal.
  - `/` remains invalid.
  - The lexer rule also applies to title literals. This is safe because type
    checking still restricts status and priority values.
- The prior review approved the continuation-only contract and identified one
  unchanged quirk: a tag exactly equal to a language keyword such as `and`
  cannot be expressed as a bare filter literal.

## Questions

No unresolved behavior questions.

## Ideas

- Minimal regression fix shape: restore a literal continuation predicate for
  alphanumeric characters, `_`, `.`, and `-`. Do not widen field names or token
  starts.
- This accepts punctuation in any position after the first character, including
  trailing or repeated punctuation such as `release-`, `v1..0`, and `a--b`.
  Treating these as opaque tag text is simpler than adding internal-position
  restrictions.
- Add end-to-end coverage through `new` and `ls` for dotted and dashed tags,
  with an unrelated task excluded.
- Document the accepted bare-literal character set near the filter syntax.
- Confirmed contract: `.` and `-` are continuation characters only. Literals
  still start with an alphanumeric character or `_`; `.hidden` and `-urgent`
  remain invalid.
- Other punctuation remains invalid.
- Test success with dotted and dashed tags and an excluded unrelated task.
- Test a refusal boundary, such as a leading `.` or unsupported `/`, with the
  exact current filter error.
- Keep exact keyword recognition. `and-x` is an identifier; exact `and` remains
  a language keyword.
