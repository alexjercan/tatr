# Review: Add check subcommand: lint task artifacts for process drift

- TASK: 20260720-152503
- BRANCH: feature/check-linter

## Round 1

- VERDICT: REQUEST_CHANGES
- REVIEWER: out-of-context (fresh-context subagent; prompt contained only
  the task id, branch, worktree path, project conventions and review
  instructions)

- [x] R1.1 (MAJOR) tatr.c:2727 - the STATUS re-validation trims the token
  before validating, but task_status_from_string maps any non-exact match
  to OPEN: '- STATUS: CLOSED ' (trailing space) or a CRLF file produces a
  silently-OPEN task that the scan pronounces valid, exempting it from
  every closed-* rule with exit 0. Reproduced both ways by the reviewer.
  Suggested: validate the exact token the parser sees (no trim), so the
  variant becomes a malformed-header finding.
  - Response: fixed - the trim is gone; the exact token is validated and
    'CLOSED ' now reports malformed-header (pinned in
    test_check_scanner_edges).
- [x] R1.2 (MINOR) tatr.c:2790 - any checkbox starting with capital R is
  treated as a finding line ('- [ ] Rebase onto master (before merging)'
  yields bad-severity 'before merging'). Suggested: require a digit after
  the R.
  - Response: fixed - a digit is required after the R; the Rebase prose line
    is pinned as not-a-finding in test_check_scanner_edges.
- [x] R1.3 (MINOR) tatr.c:2892 - the ledger scan breaks after the first
  '(x' candidate whether or not it parsed, so '(x-axis) then (x7)' and
  '(x2, enforced) ... (x5)' are false negatives. Suggested: break only
  after a successfully parsed (xN), keep scanning otherwise.
  - Response: fixed - unparsed candidates continue the scan; the
    '(x2, enforced) ... (x4)' line is pinned in test_check_ledger.
- [x] R1.4 (MINOR) tatr.c:2762 - tatr's own five legacy REVIEW.md files use
  'Verdict: APPROVE', so the shipped lint exits 1 on this repo's backlog
  with a misleading 'no VERDICT line' for approved reviews; and the exact
  compare flags '- VERDICT: APPROVE (1 round)'. Suggested: match the first
  whitespace-delimited token, and either accept the legacy spelling or
  normalize the five legacy files in this task.
  - Response: fixed, both halves - the verdict value is now the first
    whitespace-delimited token ('APPROVE (1 round)' pinned), and the five
    legacy files were normalized to '- VERDICT: APPROVE' as linter-adoption
    cleanup, together with ticking 21 legacy Steps boxes on the four
    shipped 20260705 tasks (show/edit/rm/AGENTS.md all verifiably exist).
    The backlog now lints clean except this task's own honest
    REQUEST_CHANGES, which this round resolves.
- [x] R1.5 (MINOR) checker.sh - untested rules/paths: missing-TASK.md
  finding, no-VERDICT finding, absolute --ledger path, ledger-unreadable
  finding, and --strict staying quiet when the files exist. The DoD line
  "All rules implemented and covered" is overstated for these variants.
  Suggested: add coverage.
  - Response: fixed - test_check_missing_artifacts (missing TASK.md,
    verdict-less REVIEW.md), absolute/missing/directory --ledger asserts in
    test_check_ledger, and a strict-stays-quiet assert in test_check_strict.
    60/60 plain and under --memcheck.
- [x] R1.6 (NIT) tatr.c:2666 - task_sibling_read's comment claims
  aids_io_read reports failures on stderr; it only sets a failure reason,
  so an existing-but-unreadable REVIEW.md is silently treated as absent
  (wrong closed-missing-review under --strict). Suggested: aids_log the
  failure and fix the comment.
  - Response: fixed - aids_log(AIDS_WARNING, ...) on the read failure and
    the comment now says "logged and treated as absent".
- [x] R1.7 (NIT) tatr.c:2871 - --ledger pointing at a directory passes as a
  clean empty ledger (fopen on a directory succeeds on Linux). Suggested:
  guard with aids_io_isdir.
  - Response: fixed - isdir guard added; --ledger tasks now reports
    'ledger: unreadable' (pinned in test_check_ledger).
- [x] R1.8 (NIT) tatr.c:2716 - '## Steps'-prefixed headings collide
  ('## Steps taken later' re-enters the Steps section, false
  closed-unchecked). Suggested: exact compare of the trimmed heading.
  - Response: fixed - exact compare of the trimmed heading against
    '## Steps'.

Reviewer verification notes: clean build under clang and gcc -Wall -Wextra;
58/58 and 58/58 --memcheck reproduced; targeted valgrind on the
missing-TASK.md, deserialize-failure and empty-file paths all zero errors;
ownership audit of raw/task._buffer found no double-free; behavior
contracts (silence-when-clean, stdout/stderr split, exit codes, -r,
per-ID, sorted walk) verified by hand; all ticked Steps boxes delivered;
close-out's numbers reproduce exactly.

## Round 2

- VERDICT: APPROVE
- REVIEWER: out-of-context (same fresh-context subagent, resumed)

All eight findings verified resolved against commit 93848f3 by re-running
the reviewer's own round-1 repros: trailing-space and CRLF STATUS now
malformed-header findings; R-prose checkbox quiet; both ledger false
negatives fire; verdict reads the first token ('APPROVED' still flags, so
no over-tolerance); legacy normalization judged truthful adoption cleanup
(semantics preserved, ticks match verifiably shipped work, history in
git). 60/60 plain and --memcheck, targeted valgrind zero errors. One
cosmetic take-or-leave note: a raw control byte prints inside the quoted
invalid-STATUS token; left as is.
