# Add support for non-alpha characters in tags (e.g dot)

- STATUS: CLOSED
- PRIORITY: 100
- TAGS: bug, cli, parsing

The current issue is that when I run `tatr ls` and I try to use a tag with a
`.` in it it will not parse it properly in the `--filter` flag; so this is a
problem with the filter query parser.

## Steps

- [x] Add `checker.sh` integration coverage first in
      `test_filter_tag_literal_punctuation`, following
      `tasks/20260809-094218/NOTES.md`: create dotted, dashed, and unrelated
      tags; verify exact matches; and check the exact refusal for a leading dot.
      Run it against the unchanged lexer and confirm it fails.
- [x] Update the bare-literal path in `tatr_filter_lexer_next` in `tatr.c` with
      a named continuation-character predicate for alphanumeric characters,
      `_`, `.`, and `-`. Keep literal starts, field names, other punctuation,
      and exact keyword recognition unchanged.
- [x] Run the integration suite, then mutation-check the new test by temporarily
      restoring the old continuation condition, rebuilding, confirming
      `test_filter_tag_literal_punctuation` fails, and restoring the fix.
- [x] Update the `README.md` filter syntax to state that `.` and `-` are valid
      only after a bare literal has started, with dotted and dashed tag examples.
- [x] Re-read the changed files and run the clang, memcheck, GCC, and MinGW
      verification commands.

## Definition of Done

- Dotted and dashed tag literals parse, match the exact tagged task, and exclude
  unrelated tasks. (test: `test_filter_tag_literal_punctuation`)
- Leading `.` remains invalid and returns the expected filter diagnostic.
  (test: `test_filter_tag_literal_punctuation`)
- The new integration test fails when the widened continuation rule is removed.
  (test: `test_filter_tag_literal_punctuation` mutation check)
- Filter documentation clearly states the continuation-only contract.
  (human: confirm `README.md` does not imply that `.` or `-` can start a bare
  literal)
- The clang build and integration suite succeed. (cmd: `nix develop -c make &&
  nix develop -c ./checker.sh`)
- The integration suite is leak-free. (cmd: `nix develop -c ./checker.sh
  --memcheck`)
- The GCC build is warning-clean. (cmd: `nix develop -c make clean all CC=gcc`)
- The MinGW build is warning-clean. (cmd: `nix develop -c make windows`)
