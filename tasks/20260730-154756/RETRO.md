# Retro: Require user disposition for lesson promotions

- TASK: 20260730-154756
- BRANCH: feature/ledger-dispositions
- REVIEW ROUNDS: 2

## What went well

Mutation-testing the new guards earned its keep in a way prose review could
not have. It is what produced round 1's two MAJOR findings, and in round 1's
fix loop it produced the cycle's most serious defect - a hang - from a guard
that every test suite reported as fine.

Disclosing an unkillable check instead of ticking the step wholesale paid off.
Round 1's resolution stated plainly that the `verify.disposition`/`verify.count`
terms are round-trip completeness rather than independently killable. The
round-2 reviewer ran 26 mutations and found exactly those two as survivors, so
the disclosure read as confirmation rather than as a hole it had caught us
hiding. Writing down what a test does NOT cover is cheaper than being found out.

Both review rounds were out-of-context, and both found things this session
would not have. Round 2's `return_defer` catch is the clearest case: an
unintended edit to an unrelated command, invisible to every test, findable only
by reading the diff as a stranger.

## What went wrong

**A guard was load-bearing against a hang while being entirely untested.**
Deleting the malformed-entry refusal did not produce a wrong message, it
produced an infinite loop: the R1.3 fallback returns an entry with `problem`
set but `count_end`/`paren_close` NULL, and the splice computes
`(unsigned long)(NULL - match_line.str)`, which wraps to a runaway length. Root
cause: the splice was written against the entries the parser returned on the
happy path, and when R1.3 added a new way to return an entry, the consumer's
unstated precondition ("a parsed entry has a count group") was never restated
as a check. The guard above it happened to hold that precondition, so nothing
was visibly wrong until the guard was removed.

**A live mutation was left in the working tree across a session boundary.**
The previous session disabled the guard to watch its test go red and did not
revert before losing context. The next session inherited a worktree whose
source contained `if (0 && match.problem[0] != '\0')` with nothing recording
why, and the suite hung rather than failed - a deliberately broken build is
indistinguishable from a bug to whoever arrives next.

**The check suite was run through a pipe again.** `nix develop -c ./checker.sh
| tail -20` buffers everything until exit, so a hang looked like a slow run for
twenty minutes and the failing test's name never appeared. This is the exact
failure `checker-set-e-exit-codes` already documents, already promoted to
AGENTS.md, recurring anyway - prose did not prevent it.

**An unrelated command lost a line in a repetitive dispatch chain.** Inserting
the `ledger` branch dropped the `check` branch's `return_defer(result)`. It is
behaviour-neutral today, which is why no test and no amount of suite-running
would have surfaced it.

## What to improve next time

- When mutation-testing, watch HOW the mutant fails, not just that it fails. A
  mutant that hangs, corrupts or crashes instead of reporting cleanly is
  telling you the code below assumed a precondition nobody asserts there. Fix
  the assumption at its use, not just the guard.
- Revert a mutation in the same step that observes the red, before doing
  anything else. Never let a deliberately broken build survive a tool call you
  might not return from.
- Run the check suite bare, to a file, never through a pipe - including when
  you only want the last few lines. `> log 2>&1` then read the file.
- After inserting a branch into a repetitive chain, diff it against its
  neighbours rather than reading the new branch alone.

## Action items

- No follow-up code task. R2.4 (the round-trip refusal's error message
  misdirects on surrounding whitespace) is recorded open in REVIEW.md at the
  implementer's discretion; it is a message-wording NIT on a refusal that is
  already correct and already leaves the ledger untouched.
- `checker-set-e-exit-codes` recurring after promotion to AGENTS.md is evidence
  for `fix-the-tool-not-the-prompt`: the rule wants a tool, not more prose.
  Noted for the user rather than self-applied.
