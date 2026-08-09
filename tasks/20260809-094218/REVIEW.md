# Review

## Findings

- [ ] MINOR `checker.sh:139` - The dotted and dashed checks only require one
  matching output line. They still pass if the unrelated task is also returned,
  so they do not prove the stated exclusion behavior. Assert that `Unrelated`
  is absent, or assert that each result contains exactly one line.

## Human Proofs

- [x] `README.md:74` - User confirmed the continuation-only wording.

## Verification

- Clang build and integration suite: 9/9 passed.
- Valgrind integration suite: 9/9 passed.
- GCC and MinGW builds: warning-clean.
- Mutation proof: the new integration test failed with the old continuation
  condition.
- Manual dotted, dashed, keyword-prefixed, leading-punctuation, and unsupported
  punctuation checks passed.

## Verdict

APPROVE
