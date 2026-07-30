# Allow dots and dashes in filter literals so version tags like v0.1.0 parse

- STATUS: CLOSED
- PRIORITY: 100
- TAGS: bug
- KIND: TASK
- FLOW STEP: DONE
- PLAN STATUS: NOT_REQUIRED

The filter query language cannot match version-style tags. Running
`tatr ls -f ':tags contains v0.1.0'` fails to parse because the filter lexer
in `tatr.c` (`tatr_filter_lexer_next`) lexes bare literals/identifiers with a
loop that only accepts `isalnum` and `_`:

```c
if (isalpha(lexer->ch) || lexer->ch == '_' || isdigit(lexer->ch)) {
    while (isalnum(lexer->ch) || lexer->ch == '_') {
        tatr_filter_lexer_read(lexer);
    }
```

So `v0.1.0` lexes as the identifier `v0`, then the `.` becomes an INVALID
token and the whole query is rejected. Tags containing `.` (e.g. `v0.1.0`) or
`-` (e.g. `release-candidate`) can be created with `new`/`edit` but can never
be filtered on.

## Behaviour

- `tatr ls -f ':tags contains v0.1.0'` parses and returns tasks whose tags
  include `v0.1.0`.
- `.` and `-` are valid characters *inside* a literal (a run that has already
  started with an alnum/`_`). They do not become valid standalone tokens.
- `eq`/`in`/`contains` against `:status`, `:priority`, `:tags` keep working
  for their existing literal values (plain words and numbers).
- Field names (`:status`) are unaffected; only bare literals gain the new
  characters.

## Design notes

- The clean change is to widen the *continuation* character class for the
  bare-literal branch only, leaving the start-of-token check (alnum/`_`) and
  the `:field` lexer unchanged. A small `tatr_filter_is_literal_char` helper
  keeps the intent explicit.
- Keyword recognition (`eq`, `in`, `and`, ...) already only fires on an exact
  full-token compare, so a literal like `and.1` still lexes as an identifier,
  not the `and` keyword. Confirm this holds after the change.
- Scope the new characters to `.` and `-` (the request). Do not pull in `/`,
  `+`, etc. unless a test shows they are needed; keep the surface minimal.

## Steps

- [x] Add a `tatr_filter_is_literal_char` helper (alnum, `_`, `.`, `-`) and use
      it for the continuation loop of the bare-literal branch in
      `tatr_filter_lexer_next`; leave the start check and `:field` lexer alone.
- [x] Manually verify `tatr ls -f ':tags contains v0.1.0'` works against a task
      tagged `v0.1.0`, and that a keyword-prefixed literal (e.g. `and-x`) is
      still treated as a literal.
- [x] Add tests in `checker.sh`: filter a dotted version tag (`v0.1.0`), filter
      a dashed tag (`release-candidate`), and confirm an unrelated task is
      excluded. Follow the existing `test_filter_tags_contains` pattern.
- [x] Update the README filtering section if it implies literals are word-only,
      noting that version-style tags (dots/dashes) are supported.
- [x] Run `make` and `./checker.sh`; all green.
