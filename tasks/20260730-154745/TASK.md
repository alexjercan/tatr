# Scaffold and validate flow artifact schemas

- STATUS: CLOSED
- PRIORITY: 85
- TAGS: feature, flow, schema, testing
- KIND: TASK
- FLOW STEP: DONE
- PLAN STATUS: APPROVED
- DEPENDS ON: 20260730-153325, 20260730-154657

## Story

As a flow-suite maintainer, I want tatr to scaffold and validate task sibling
records from one canonical implementation, so flow artifacts, proof contracts,
review rounds, retros, and decision links are enforced by the CLI instead of
being repeated in skill prose.

## Steps

- [x] Inspect the lifecycle commands and close guards from 20260730-154657,
      then list every artifact field those commands depend on before changing
      schemas.
- [x] Define one in-code schema table for TASK, SPIKE, DECISION, REVIEW, and
      RETRO headings, required sections, allowed status/verdict values, and
      task-kind-specific record requirements.
- [x] Add scaffold commands that create missing sibling records from the
      schema table, refuse to overwrite existing files by default, and expose a
      dry-run/list mode for callers that only need paths and template names.
- [x] Extend `tatr check` to validate non-empty required sections, SPIKE
      status/seeded pointers, DoD `test:`/`cmd:`/`manual:` proof syntax,
      sequential review rounds, finding IDs, reviewer fields, verdicts, and
      APPROVE with no open BLOCKER or MAJOR findings.
- [x] Strengthen DECISION checks so task pointers resolve and supersede links
      are reciprocal between the old and replacement records.
- [x] Add a structured proof-listing command that prints each DoD proof as data
      without executing shell text.
- [x] Revalidate tatr's existing task siblings under the stricter rules,
      classify historical exceptions explicitly, and avoid rewriting history
      except for required lifecycle annotations.
- [x] Add integration tests that exercise scaffold, schema lint, proof listing,
      review consistency, reciprocal supersede, and existing-artifact
      migration behavior.
- [x] Update README.md, AGENTS.md, CHANGELOG.md, and `skills/tatr/SKILL.md`
      from the implemented command names and output.

## Definition of Done

- Every record kind scaffolds exactly the documented format and refuses to
  clobber an existing file (test: `test_record_scaffolds`).
- Required artifact sections and values are checked from one schema source for
  TASK, SPIKE, REVIEW, RETRO, and DECISION (test:
  `test_check_record_schemas`).
- APPROVE with an open BLOCKER or MAJOR finding is rejected (test:
  `test_check_review_approval_consistency`).
- Supersede links must resolve in both directions (test:
  `test_check_reciprocal_supersede`).
- Structured proof listing round-trips shell text without executing it (test:
  `test_proof_listing_does_not_execute`).
- Current repo artifacts either pass the new checks or are classified by an
  explicit historical exemption (test: `test_existing_artifacts_are_classified`).
- Full native and memory-check suites pass
  (cmd: `nix develop -c ./checker.sh && nix develop -c ./checker.sh --memcheck`).

## Notes

- Parent Epic: nix.dotfiles task 20260730-153122.
- Depends on: 20260730-153325 and 20260730-154657.
- 20260730-154657 is the approved lifecycle-command prerequisite. This task
  should consume those transition names and guards rather than invent a
  parallel artifact state machine.
- Formats should have one implementation source; skill examples point to the
  CLI instead of copying templates.
- Decision to defer to implementation: exact scaffold command names can be
  chosen after reading the 20260730-154657 command surface, but all commands
  must be documented from real output.
