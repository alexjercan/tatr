# Retro: Filter tag literal punctuation

## What changed

- Restored `.` and `-` as bare-literal continuation characters in the filter
  lexer.
- Kept literal starts, field names, and other punctuation unchanged.
- Added end-to-end success and refusal coverage and documented the syntax.

## Evidence

- The new integration test failed before the lexer change and passed after it.
- Removing the widened continuation rule made the new test fail again.
- Clang, valgrind, GCC, and MinGW checks passed without warnings or memory
  errors.

## Tradeoffs

- Leading `.` and `-` remain invalid. Supporting them would change token starts
  instead of restoring the prior narrow behavior.
- Exact language keywords remain reserved as bare literals. This existing parser
  behavior stays out of scope.

## Next time

- Keep behavior-level regression tests when simplifying production and test
  surfaces. The v2 simplification removed both the earlier fix and its test.
