# Decision: v2 task record schema, no in-tool migration

- DATE: 20260730-160000
- STATUS: ACCEPTED
- TASK: 20260730-153325
- TAGS: decision,flow,schema,breaking

## Context

The flow suite currently enforces lifecycle rules by grepping prose: `tatr
check` scans TASK.md line by line for a `## Flow State` heading and validates
the marker text under it, while "this is an epic container" is signalled by the
string `goal` appearing in the TAGS list. The downstream tasks in this epic
(lifecycle guards 20260730-154657, Epic graph 20260730-154740, artifact schemas
20260730-154745, ledger dispositions 20260730-154756) all need to read workflow
state and Epic/Story relationships as data, not as text.

Four load-bearing forks were confirmed with the user before planning:

1. Where the new typed fields live on disk.
2. Whether `KIND: EPIC` or the `goal` tag identifies a container.
3. Whether tatr ships a migration mode for legacy records.
4. How pre-flow records without plan state are classified.

## Decision

**The v2 record.** All metadata is one flat block of `- KEY: value` lines
directly under the title, parsed in a fixed order. `## Flow State` is retired
as a parsed construct.

```markdown
# Title

- STATUS: OPEN | IN_PROGRESS | CLOSED
- PRIORITY: <non-negative integer>
- TAGS: <comma-separated, may be empty>
- KIND: TASK | EPIC | STORY | SPIKE
- FLOW STEP: BACKLOG | UNDERSTANDING | PLANNING | PLANNED | WORKING | REVIEWING | COMPOUNDING | DONE
- PLAN STATUS: DRAFT | APPROVED | NOT_REQUIRED
- PARENT: <huid>
- DEPENDS ON: <huid>, <huid>

<description body, byte-preserved>
```

STATUS, PRIORITY, TAGS, KIND, FLOW STEP and PLAN STATUS are required and always
serialized. PARENT and DEPENDS ON are optional: they are serialized only when
set, so a task with no relationships has no empty relationship lines. DEPENDS ON
is a single line of comma-separated HUIDs, formatted exactly like TAGS.

**Epic identity is KIND, not a tag.** `KIND: EPIC` is the only container
signal. The `goal` tag stops being load-bearing, and every check exemption keys
on `KIND == EPIC`.

**No migration mode.** tatr ships no migrate command and no v1 compatibility
path. A record missing the v2 fields fails to parse with a diagnostic naming
the file and the missing field. Existing records are corrected by hand.

**Values are validated on the exact parsed token.** Every enum has a
strict `*_from_string` that reports failure rather than defaulting, and a bad
value aborts the load before anything is written.

**The body is opaque; the write side carries the invariant.** An earlier
revision of this branch also rejected a metadata-shaped line at the start of
the body, to catch a misspelled field. Review showed that guard was both too
strict and too weak: it made `tatr new -b` write a record tatr itself could
not read (a body opening with `- NOTE: ...`), while a misspelled field one
line deeper was still swallowed. The guard is gone. In its place, `task_save`
re-parses the serialized bytes before writing, so tatr can never write a
record it cannot read back - which is the invariant the guard was reaching
for, enforced where it actually holds.

## Alternatives considered

- **Keep `## Flow State` and parse the section** - rejected. It keeps metadata
  half prose-shaped and means the description body is no longer opaque to the
  parser, which is exactly the coupling this epic exists to remove.
- **Nested markdown list for DEPENDS ON** - rejected. It buys rendering
  niceness at the cost of a nested-list parse case no other field needs.
- **`goal` tag keeps granting exemptions** - rejected. Two overlapping notions
  of "container", and any task can grant itself an exemption by editing a tag.
- **`tatr migrate --dry-run` / `tatr check --fix`** - rejected by the user. A
  migration mode is code that runs exactly once against one repository and then
  has to be maintained and tested forever. Hand-correcting 31 files is a
  bounded, reviewable diff.
- **Classify pre-flow records automatically (CLOSED -> DONE + NOT_REQUIRED)** -
  rejected as the automatic half of the same migration mode. The classification
  still happens, but as a deliberate hand edit reviewed in the diff.

## Consequences

- The change is breaking by design. Any TASK.md not carrying the v2 fields
  stops parsing the moment this lands, which is why the 31 records in this
  repository are corrected by hand inside the same branch, before it lands.
- `check_flow_state` and its `bad-flow-state` finding are retired: the parser
  now owns value validity, and an invalid FLOW STEP or PLAN STATUS surfaces as
  `malformed-header` instead. README, AGENTS.md and skills/tatr/SKILL.md must
  be updated accordingly.
- The two legacy containers had their `- TAGS: goal` rewritten to
  `- TAGS: flow` during the hand correction, not merely deprecated: with the
  tag no longer load-bearing, leaving it would suggest it still meant
  something. A saved `:tags contains goal` query therefore stops matching;
  `:kind eq EPIC` is its replacement.
- The parent Epic (nix.dotfiles `20260730-153122`) lives in another
  repository, and `PARENT` only expresses same-tree references, so the five
  task records in this epic carry `DEPENDS ON` but no `PARENT`, and keep the
  cross-repo pointer as body prose. `KIND: STORY` likewise has no user in this
  repository yet; it exists for the Epic graph task (20260730-154740).
- The `goal`-tag exemptions that AGENTS.md and README still describe were
  removed from the code in 9303caf without the docs following. Re-keying them
  on `KIND == EPIC` restores the documented behavior and makes the docs true
  again; on the current tree it is a no-op, since both containers already carry
  a REVIEW.md and RETRO.md and have no unchecked steps.
- The flow, work and plan skills in nix.dotfiles reference `## Flow State`.
  Updating them belongs to the parent Epic (20260730-153122), not to this task.
- Downstream tasks read typed fields instead of re-parsing prose. This task
  deliberately does not validate relationships (missing references, cycles,
  parent/child consistency): storage and syntax only. Graph semantics belong to
  20260730-154740.
