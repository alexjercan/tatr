# Add typed v2 workflow schema and correct tatr history by hand

- STATUS: CLOSED
- PRIORITY: 100
- TAGS: feature, flow, schema, breaking

## Story

As a flow-suite maintainer, I want tatr records to expose typed workflow and
relationship metadata, so lifecycle and Epic/Story rules are enforced from
structured data instead of headings parsed from prose.

## Steps

- [x] Record the v2 task format in `DECISION.md`: one flat metadata block
      carrying STATUS/PRIORITY/TAGS plus KIND (`TASK|EPIC|STORY|SPIKE`), FLOW
      STEP (`BACKLOG|UNDERSTANDING|PLANNING|PLANNED|WORKING|REVIEWING|
      COMPOUNDING|DONE`), PLAN STATUS (`DRAFT|APPROVED|NOT_REQUIRED`), optional
      PARENT and optional comma-separated DEPENDS ON; no migration mode.
- [x] Replace the parser/serializer with the v2 model: strict per-enum
      `*_from_string` that reports failure instead of defaulting, fixed field
      order, optional PARENT/DEPENDS ON serialized only when set, description
      body preserved byte for byte.
- [x] Reject legacy and malformed records before writing anything, with a
      diagnostic naming the file, the offending field and the accepted values.
      Add no compatibility path and no migration command.
- [x] Wire the v2 fields through `new` (defaults `TASK`/`BACKLOG`/`DRAFT`),
      `edit` (set every field; clear PARENT/DEPENDS ON with an empty value),
      `show`, `ls`, and the filter language (`kind`, `flow_step`,
      `plan_status`, `parent`, `depends`).
- [x] Retire the `check_flow_state` prose scan and its `bad-flow-state`
      finding; read `unplanned-in-progress` from the typed field, and re-key
      the container exemptions on `KIND == EPIC` (they are documented in
      AGENTS.md/README but were dropped from the code in 9303caf).
- [x] Hand-correct all 31 existing `TASK.md` records in `tasks/` to v2 - no
      tool, no script - classifying the two `goal` containers as `KIND: EPIC`
      and pre-flow closed work as `DONE` / `NOT_REQUIRED`.
- [x] Add integration tests for v2 round trips, legacy rejection, malformed
      value atomicity, new/edit field coverage, filtering, and EPIC exemptions.
- [x] Update README.md, AGENTS.md, CHANGELOG.md, and `skills/tatr/SKILL.md`
      from the implemented CLI and schema.

## Definition of Done

- New task records round-trip every v2 field, with optional fields absent and
  present, without changing the description body.
  (test: `test_v2_task_round_trip`)
- A legacy v1 record is rejected with a diagnostic naming the file and the
  missing field, and nothing is written.
  (test: `test_v2_rejects_legacy_record`)
- Unknown or malformed v2 values leave files untouched.
  (test: `test_v2_rejects_invalid_metadata_atomically`)
- `new` and `edit` set every v2 field, and `edit` clears the optional ones.
  (test: `test_v2_new_and_edit_fields`)
- The filter language selects on kind, flow step, plan status, parent and
  dependencies. (test: `test_v2_filter_fields`)
- A `KIND: EPIC` container is exempt from the record-completeness rules and an
  ordinary task is not. (test: `test_check_epic_exemptions`)
- Every tatr repository task parses and passes the v2 baseline checks
  (cmd: `./dist/tatr check --ledger LESSONS.md`).
- Full native and memory-check suites pass
  (cmd: `nix develop -c ./checker.sh && nix develop -c ./checker.sh --memcheck`).

## Notes

- Parent Epic: nix.dotfiles task 20260730-153122.
- Breaking compatibility is explicitly accepted. Do not retain legacy parsing,
  and do not add a migration command: correcting records is a hand edit.
- This task owns the schema. Later tasks own transition (20260730-154657),
  graph (20260730-154740), artifact (20260730-154745), and lesson
  (20260730-154756) semantics built on it. Relationship fields are stored and
  syntax-checked here; reference resolution and cycle detection are not.
- The flow/work/plan skills in nix.dotfiles still reference `## Flow State`;
  updating them belongs to the parent Epic.
