# Add AGENTS.md and document new agent commands

- STATUS: CLOSED
- PRIORITY: 70
- TAGS: docs
- KIND: TASK
- FLOW STEP: DONE
- PLAN STATUS: NOT_REQUIRED

Once `show`, `edit` and `rm` exist, the docs must reflect them and the repo
needs an AGENTS.md that orients an agent working on tatr.

Depends on: the show, edit and rm tasks being CLOSED (documents their final
behaviour).

## Steps

- [x] Update `README.md`: add usage sections for `show`, `edit` and `rm`; remove
      the now-false "No built-in task editing command" / "No filtering ... yet"
      limitations that these commands resolve; refresh the subcommand list.
- [x] Add `AGENTS.md` at the repo root covering: what tatr is, the single-file
      C architecture and the vendored `aids.h`/`argparse.h` deps, how to build
      (`make`, clang line) and test (`./checker.sh`, `-v`, `--memcheck`), the
      tasks/ markdown storage format and HUID scheme, the subcommand set, and
      conventions for agents (dogfood tatr for its own task tracking, keep it a
      single file, no AI attribution in commits, plain ASCII punctuation).
- [x] Verify the documented command invocations against the actual built binary
      so the docs are accurate.
- [x] Run `make` and `./checker.sh` to confirm nothing regressed.
