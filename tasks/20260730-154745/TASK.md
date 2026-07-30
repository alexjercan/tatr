# Scaffold and validate flow artifact schemas

- STATUS: OPEN
- PRIORITY: 85
- TAGS: feature,flow,schema,testing
- KIND: TASK
- FLOW STEP: BACKLOG
- PLAN STATUS: DRAFT
- DEPENDS ON: 20260730-153325

## Story

As a flow-suite maintainer, I want tatr to scaffold and validate task sibling
records, so formats, proof syntax, review rounds, and decision links do not
need to be repeated across skill prose.

## Steps

- [ ] Define canonical schemas for TASK, SPIKE, DECISION, REVIEW, and RETRO,
      with task-kind-specific required records and headings.
- [ ] Add commands that scaffold each record from the canonical schema without
      overwriting existing content.
- [ ] Extend `tatr check` to validate non-empty required sections, SPIKE
      status/seeded pointers, DoD `test:`/`cmd:`/`manual:` proof syntax,
      sequential review rounds/finding IDs/reviewer fields/verdicts, and
      APPROVE with no open BLOCKER/MAJOR findings.
- [ ] Strengthen DECISION checks so supersede links are reciprocal and task
      pointers resolve.
- [ ] Keep proof execution outside tatr, but add structured proof listing so
      work and review can execute the exact contract.
- [ ] Revalidate and deliberately migrate/classify tatr's existing sibling
      artifacts under the new checks.
- [ ] Add integration tests and update README.md, AGENTS.md, CHANGELOG.md, and
      `skills/tatr/SKILL.md`.

## Definition of Done

- Every record kind scaffolds exactly the documented format without clobbering
  an existing file (test: `test_record_scaffolds`).
- Malformed proof, review, spike, retro, and decision records emit stable
  findings (test: `test_check_record_schemas`).
- APPROVE with an open BLOCKER/MAJOR is rejected
  (test: `test_check_review_approval_consistency`).
- Supersede links must resolve in both directions
  (test: `test_check_reciprocal_supersede`).
- Structured proof listing round-trips shell text without executing it
  (test: `test_proof_listing_does_not_execute`).
- Full native and memory-check suites pass
  (cmd: `nix develop -c ./checker.sh && nix develop -c ./checker.sh --memcheck`).

## Notes

- Parent Epic: nix.dotfiles task 20260730-153122.
- Depends on: 20260730-153325.
- Formats should have one implementation source; skill examples point to the
  CLI instead of copying templates.
