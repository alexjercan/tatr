# Retro: Scaffold and validate flow artifact schemas

- TASK: 20260730-154745
- BRANCH: feat/artifact-schemas
- REVIEW ROUNDS: 3

## What went well

- Deriving the schemas from the SKILL files instead of inventing them. My first
  draft of `RECORD_SCHEMAS[]` was guessed (`## Options`, `## Direction`,
  `## Seeded Tasks`, a `STATUS` of EXPLORING/LANDED/ABANDONED). Reading
  `/spike`, `/review`, `/compound` and `/plan` before going further replaced
  every one of those with the real convention. A schema table is only worth
  having if it encodes what the process actually says.
- The exemptions-file design held up under adversarial review. The reviewer
  specifically checked whether `tasks/EXEMPTIONS.md` was being used to silence
  findings that should have been fixed, spot-checked exempted records against
  what the exemption claimed, and found the classification honest. Writing the
  DECISION.md first - including the alternatives I rejected and WHY an in-record
  marker was worse - is what made that check cheap for them.
- Mutation-testing all 17 new guards before review. Sixteen went red on the
  first pass; the seventeenth exposed a bad MUTATION (`0 * check_finding(...)`
  still prints, so the finding appeared and only the count was zeroed) rather
  than a weak test. Redoing it properly took one run. The technique earns its
  keep, but a mutation that leaves the side effect intact tests nothing.
- The three review rounds each did real work rather than converging politely.
  Round 3's differential fuzz - 7 source steps x 16 record mutations x every
  legal edge, asserting the post-transition finding set is a subset of the
  pre-transition one - is a far better proof of the invariant than the two
  hand-built repros I had.

## What went wrong

- R1.1 (MAJOR): `tatr flow` could produce states `tatr check` flags. I added
  fourteen new lint rules and never asked whether the lifecycle enforced any of
  them, even though AGENTS.md states the invariant explicitly and the previous
  task (20260730-154657) had failed the same way, and even though I had read
  `guard-from-the-rule-not-its-summary` in the ledger before starting. The
  ledger entry did not fire because I was reading it as advice about writing
  guards, and I did not think of myself as writing guards - I was writing lint
  rules. The lesson is about the RELATIONSHIP, not about either side.
- R2.1 and R2.2 (MAJOR): the R1.1 fix was itself incomplete in exactly the same
  way. I moved the rules I had just written into shared collectors and left
  `bad-severity` and the DECISION.md supersede rules - pre-existing rules I was
  not thinking about - in `check_task`. Two of the fourteen holes stayed open.
  Root cause: I fixed the instances the finding named instead of establishing
  the property the finding was about, and I did not enumerate the rule set to
  check my own work.
- R1.2 (MAJOR): I added a `format(printf, ...)` attribute and regressed the
  MinGW build to four warnings. The suite did not catch it because
  `test_windows_build_target` only asserted the artifact was a PE file, so the
  compiler's warnings scrolled past a passing test. I had run the full suite and
  read "93/93" as "the Windows build is fine".
- R1.3, R1.5, R1.6: three separate invented facts in the docs - a `tatr proofs`
  transcript with 3 lines where the command prints 7, a `--list` example
  claiming a file was missing that exists, and a "twenty-eight records" count
  matching nothing shipped. All three were written from memory of what I
  expected rather than from a run, in a task whose own Notes say commands "must
  be documented from real output".
- R2.3: my R1.4 fix introduced a new bug. Making `proof_print_text` stop
  collapsing intra-line whitespace let a literal tab reach stdout, which breaks
  the three-field record format the command promises. The previous
  over-collapsing behavior had been masking it.

## What to improve next time

- When a change adds rules to a validator that has a paired enforcer (lint and
  lifecycle, parser and serializer, schema and migration), enumerate the full
  rule set on BOTH sides and diff the lists. Not "did I wire up the new ones" -
  the complete set, mechanically. Both MAJOR rounds here were incomplete
  enumeration, not faulty reasoning.
- Treat "fix the finding" as "establish the property the finding names". R1.1
  said the invariant was broken; I made the reported path hold instead of making
  the invariant structural. The round-2 fix finally did it by moving the rule
  home and writing the constraint into AGENTS.md, which is what should have
  happened in round 1.
- Every command transcript in a doc gets pasted from a real run in the same
  session, not typed. Three of seven round-1 findings were invented output; this
  is the cheapest class of finding to eliminate entirely.
- A green suite only covers what it asserts. Before trusting "N/N passed" on a
  cross-cutting property (warning-cleanliness, no leaks, portability), read the
  test that supposedly covers it and check it actually would fail.

## Action items

- [x] Add `test_transition_cannot_mint_a_flagged_record`, which walks the whole
      lifecycle and asserts each refusal names the lint's own rule slug.
- [x] Make `test_windows_build_target` fail on any `warning:` in the MinGW
      build output, not just on a missing PE artifact.
- [x] State the structural rule in AGENTS.md: every record rule lives in a
      collector and nowhere else, because a rule added to `check_task` directly
      is a rule the lifecycle does not enforce.
- [x] Ledger: sharpen `guard-from-the-rule-not-its-summary` so it fires when
      ADDING a rule, not only when writing a guard - the paired enforcer is the
      thing to check, in whichever direction the change arrives.
- [x] Ledger: add `paste-transcripts-from-a-real-run` - a documented command
      output is a claim, and claims get verified by running them.
- [x] Ledger: add `a-passing-suite-is-not-a-covered-property` - bump
      `no-outcome-before-the-run` if it is the same lesson.
