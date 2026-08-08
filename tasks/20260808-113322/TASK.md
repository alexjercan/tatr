# Make tatr great again

- STATUS: CLOSED
- PRIORITY: 100
- TAGS: slop

`tatr` became slop; make it work again as it was before!

## Plan

- [x] Replace `tatr.c` with the small task model inspired by `03ff244`: title,
  description, STATUS, TAGS, and PRIORITY only.
- [x] Keep only `new`, `ls`, `edit`, `help`, and `version`; make all removed
  commands fail as unknown commands.
- [x] Make `ls` parse every selected TASK.md, fail on invalid task files, sort
  by created time, priority, or title, and support status, tag, priority, and
  title queries.
- [x] Replace `checker.sh` with integration coverage for the retained commands,
  strict parsing, query behavior, sorting, editing, and removed-command refusal.
- [x] Reduce `README.md` and `skills/tatr/SKILL.md` to the retained task format
  and command surface. Remove EXEMPTIONS.md and obsolete workflow artifacts.
- [x] Build warning-clean with clang, gcc, and MinGW. Run the integration suite
  and memcheck.

## Done when

- `nix develop -c make clean all` passes.
- `nix develop -c ./checker.sh` passes.
- `nix develop -c ./checker.sh --memcheck` passes.
- `nix develop -c make clean all CC=gcc` passes.
- `nix develop -c make windows` passes.
- `tatr help` lists only `new`, `ls`, `edit`, `help`, and `version`.
- Invalid TASK.md metadata makes `tatr ls` exit non-zero with the file path.

## Result

- Reduced `tatr.c` from 8,425 to about 2,500 lines using the pre-lifecycle
  implementation as the base.
- Removed all lifecycle commands, schemas, checks, migrations, exemptions, and
  task sidecar records.
- Kept current build, Nix, and GitHub Actions infrastructure.
- Fixed strict status validation for both disk parsing and `tatr new`.
- Verified clang, gcc, MinGW, integration tests, and valgrind.
