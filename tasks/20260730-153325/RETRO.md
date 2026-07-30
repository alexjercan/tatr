# Retro: Add typed v2 workflow schema and correct tatr history by hand

- TASK: 20260730-153325
- DATE: 20260730
- REVIEW ROUNDS: 2 (REQUEST_CHANGES, then APPROVE)

## What shipped

The v2 record: KIND, FLOW STEP and PLAN STATUS as required typed fields in the
metadata block, plus optional PARENT and DEPENDS ON, with every enum validated
on the exact token the parser consumes. No migration command and no
compatibility path; the repository's own 31 records were corrected by hand in
the same branch. `tatr check` lost its `## Flow State` prose scan and its
`bad-flow-state` rule, and its container exemptions now key on `KIND: EPIC`.

## What went well

- **Asking about the four forks before planning.** Field layout, epic
  identity, migration, and history classification were each a real fork, and
  the user's answers changed the shape of the work substantially - migration,
  worth two of the original six steps, was deleted outright. Guessing any of
  them would have produced a plan that had to be rebuilt.
- **Test-first for the check messages.** Writing `test_check_epic_exemptions`
  and the rejection tests with their exact expected strings before the
  emitting code meant the diagnostics were designed from the assertion rather
  than reverse-engineered into it. This is `test-first-for-check-messages`
  (x2 in the ledger) paying off a third time.
- **Reading the git history of a rule before re-implementing it.** The
  `goal`-tag exemptions AGENTS.md described did not exist in the code; `git
  show 9303caf` found the commit that removed them while leaving the docs
  behind. Without that check I would have "preserved" behavior that was not
  there, or deleted a doc claim that was meant to be true.
- **The out-of-context review earned its cost twice.** Round 1 found two
  MAJORs I had no line of sight on, one of which was a bug I had just written
  and the other a latent one that my own change had promoted to a certainty.

## What went wrong

- **I invented a guard the task did not ask for, and it was the branch's only
  real bug.** Rejecting a metadata-shaped line at the start of the body was my
  idea, not the plan's. It made `tatr new -b` write a record tatr itself could
  not read and exit 0 - the exact "half-applied change" AGENTS.md prohibits -
  while a misspelled field one line deeper was still swallowed, so it did not
  even deliver its own guarantee. I had noticed the false-positive risk while
  writing it, reasoned about it in two directions, and then shipped it anyway
  because I could not decide. The deciding question was available and I did
  not ask it: *what invariant am I actually protecting, and is this the layer
  that can hold it?* The answer was the write side, where `task_save`'s
  re-parse now makes the failure impossible rather than merely detected.
- **My round-trip test dodged the case it was named for.** The body carried a
  metadata lookalike (`Mentions "- KIND: EPIC" in prose.`) but never in the
  only position the guard inspected. The test described the risk accurately
  and then tested somewhere else, which is worse than not testing it - it
  bought false confidence. A test for a positional rule has to put the input
  in that position.
- **Two shell mistakes cost a cycle each.** A missing `sleep 1` between two
  `tatr new` calls let the second ID collide, and `new_task_id` scraped the ID
  back out of the *error* message, so both variables named one task and an
  edit renamed the wrong record. Separately I read exit codes through a pipe
  (`$T show x 2>&1 | tail -1; echo $?`) and got `tail`'s status, briefly
  believing `show` exited 0 on a parse failure when it exits 1.

## Lessons

- `guard-at-the-layer-that-holds-it`: before adding a validation, name the
  invariant and check whether this layer can actually enforce it; a detector
  that fires on some inputs where the producer can still emit bad output is
  worse than none, because it adds false positives without closing the hole.
  Here the read-side heuristic became a write-side re-parse.
- `test-the-position-not-the-shape`: when a rule depends on WHERE input
  appears, the test fixture must place the input exactly there. A fixture that
  contains the right bytes somewhere else passes while the rule is broken.
- `pipe-eats-the-exit-code`: this is the ledger's `checker-set-e-exit-codes`
  in a second costume - `cmd | tail; echo $?` reports the tail's status. The
  ledger entry names the `local out=$(cmd)` form only, and I did not
  generalize it to pipelines.
- `read-the-history-of-a-doc-claim`: when docs describe behavior, grep the
  code before preserving OR deleting the claim, then `git log -S` the
  discrepancy. A doc/code mismatch is a dropped change, and the commit that
  dropped it says which side was intended.

## Follow-ups

- The `- PARENT:20240101-000000` edge (no space after the colon) silently
  becomes body text. Recorded as R2.6 in REVIEW.md and left unfixed on
  purpose: catching it needs back the heuristic that caused R1.1, tatr never
  writes such a line, and only a hand-edit produces it.
- The flow, work and plan skills in nix.dotfiles still reference
  `## Flow State`, which no longer exists. That belongs to the parent Epic
  (nix.dotfiles 20260730-153122), not to this repository.
- `KIND: STORY` has no user in this repository yet; the Epic graph task
  (20260730-154740) is what will exercise it, along with PARENT resolution and
  cycle detection, which this task deliberately does not do.
