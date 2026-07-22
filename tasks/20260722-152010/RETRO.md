# Retro: Add bad-decision-status and dangling-supersede checks to tatr check

- TASK: 20260722-152010
- BRANCH: feature/decision-checks
- REVIEW ROUNDS: 1 (APPROVE, out-of-context)

## What went well

- Reusing the existing spine paid off: `task_sibling_read` gave presence-gating
  for free, `ishuid` handled ref validation, and mirroring the `bad-severity`
  scan shape meant the review found nothing structural to argue with.
- The out-of-context reviewer did real independent probing (self-reference,
  target-dir-without-DECISION.md, CRLF, comment-only files, bare-huid refs) that
  went well beyond the five tests, and an in-session sabotage pass confirmed the
  firing tests actually pin the code (68/70 with `check_decision` neutralized).
- Design call to fold STATUS-validity inline instead of a separate predicate
  avoided parsing the value twice; the reviewer independently reached the same
  conclusion.

## What went wrong

- First build failed: `aids_string_slice_starts_with` takes the prefix by value,
  not by pointer, and I passed `&superseded_by`. Compiler-caught, one-line fix.
  Root cause: pattern-matched `aids_string_slice_compare`'s (ptr, ptr) shape
  without checking `starts_with`'s signature (which the existing calls pass by
  value). A grep of an existing call site before writing would have caught it.
- The initial `dangling-supersede` test asserted the HUID in the message, but
  the finding prints the full ref. The test was wrong, not the code - but
  because I wrote the implementation before the test, the message-format
  decision was already baked in and the test had to chase it.

## What to improve next time

- For a firing check rule, write the test (with its exact expected message)
  BEFORE the emitting code, so the message format is designed from the
  assertion rather than reverse-engineered into it (this is the flow features
  playbook's test-first rule, which this task did not fully honor).
- When calling an aids/argparse helper, glance at one existing call site for the
  by-value-vs-by-pointer convention before writing a fresh call.

## Action items

- [x] Ledger: added `test-first-for-check-messages` process lesson.
- No follow-up code work: the three review findings were all NITs left as-is
  with recorded rationale.
