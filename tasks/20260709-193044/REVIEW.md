# Review: allow dots and dashes in filter literals (task 20260709-193044)

- VERDICT: APPROVE

## Scope reviewed
- `tatr.c`: new `tatr_filter_is_literal_char` helper and its use in the
  bare-literal continuation loop of `tatr_filter_lexer_next`.
- `checker.sh`: new `test_filter_tags_version`.
- `README.md`: filtering-section note.

## Findings

### Correctness (blocking): none
- The fix targets exactly the broken path: the literal *continuation* loop.
  The token *start* check (`isalpha || '_' || isdigit`) is untouched, so `.`
  and `-` remain invalid as standalone/leading tokens - a filter like
  `:tags contains -x` still errors, which is the correct pre-existing
  behaviour, not a regression.
- Keyword recognition is safe: `eq/in/and/or/not/contains` are matched by an
  exact full-token `aids_string_slice_compare`, so `and-x` lexes as the
  identifier `and-x`, verified manually. No keyword is shadowed.
- `isalnum((unsigned char)ch)` casts before the ctype call, avoiding UB on
  negative `char` values (incl. the EOF/`0xFF` sentinel, which correctly
  terminates the loop). Good.
- `contains` compares tag values as plain strings, so `.` carries no special
  meaning; `v0.1.0` matches the literal tag and nothing else. Verified end to
  end: the dotted tag, the dashed tag, and an excluded plain tag all behave.
- Full suite green (46/46) under `--memcheck` (valgrind), so no leak or
  invalid access introduced.

### Minor / non-blocking observations
- Scope was deliberately limited to `.` and `-` per the request. `/` and `+`
  (e.g. `area/backend`, semver build metadata `v1.0.0+build`) are still not
  lexable. Correct call to keep the surface minimal; if such tags appear later
  they are a trivial follow-up (extend the same helper) and should come with a
  test.
- Pre-existing quirk, not introduced here: a tag that is exactly a keyword
  (`:tags contains and`) still lexes as the keyword and would fail typecheck.
  Out of scope for this fix.

## Tests
- `test_filter_tags_version` exercises both a dotted (`v0.1.0`) and a dashed
  (`release-candidate`) tag, and asserts an unrelated task is excluded on both
  queries - matching the existing `test_filter_tags_contains` shape. Adequate
  coverage for the change.

No changes requested.
