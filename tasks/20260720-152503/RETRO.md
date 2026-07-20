# Retro: check subcommand - artifact linter (task 20260720-152503)

## What went well
- The seams held: main_check reuses task_resolve/task_load/find_current_tasks_dir
  and the argparse shape; no new translation units. Malformed-tasks-as-findings
  (opposite of ls's abort policy) fell out naturally from writing a custom walk.
- The out-of-context review round was the strongest of this flow so far: it
  reproduced a whitespace/CRLF hole in the STATUS re-validation (the exact
  silent-OPEN class the guard was written to catch), an R-prose false
  positive, and a ledger-scan false negative - all with running repros.
- Dogfooding on the repo's own backlog surfaced real historical drift
  (21 unchecked Steps boxes on shipped tasks, five legacy Verdict spellings);
  normalizing them as adoption cleanup left the repo lint-clean.

## Difficulties / gotchas
- find_current_tasks_dir returns PROJECT directories (the "/tasks" suffix
  stripped), not tasks directories; the first walk listed the project root,
  matched no HUIDs and reported nothing. Read the callee, not its name.
- task_status_from_string silently defaults unknown statuses to OPEN; any
  checker of STATUS must validate the raw token exactly as the parser
  consumes it (no trimming), or whitespace variants pass validation while
  deserializing to OPEN.

## Lessons / follow-ups
- A linter's scan must be tested against the exact byte-level variants its
  own parser mistreats; a trimmed re-validation of an untrimmed parse is a
  hole, not a guard.
- Cosmetic follow-up parked: escape control bytes in finding details
  (a raw \r prints inside quoted tokens).
