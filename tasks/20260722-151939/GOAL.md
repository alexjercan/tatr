# Goal: DECISION.md well-formedness checks in tatr check

- DATE: 20260722
- UMBRELLA TASK: 20260722-151939
- LANDING SCOPE: squash-merge each task to tatr `master` via sprout, do NOT
  push. The tatr skill doc (`home/modules/agents/skills/tatr/SKILL.md`) lives
  in the separate nix.dotfiles repo and is updated as a PAIRED change committed
  on nix.dotfiles `master` (also not pushed) at Finish.

## Goal

Add two DEFAULT `tatr check` rules that lint a task's `DECISION.md` when one
exists (validate-if-present, mirroring `bad-severity`):

- `bad-decision-status`: a `DECISION.md` whose `- STATUS:` value is not exactly
  `ACCEPTED` nor `SUPERSEDED by <ref>` (or which has no STATUS line) is a
  finding.
- `dangling-supersede`: a `- STATUS: SUPERSEDED by <ref>` or `- Supersedes:
  <ref>` reference whose `<ref>` does not resolve to an existing
  `tasks/<id>/DECISION.md` is a finding.

Neither is a presence rule: a task with no `DECISION.md` is never flagged, so
past tasks need zero migration and no `historical`/`goal` exemption is required.

## Done means

1. A `DECISION.md` with a bad STATUS token (e.g. `DRAFT`) yields a
   `bad-decision-status` finding on plain `tatr check`
   (test: `test_check_bad_decision_status`).
2. A `DECISION.md` with `STATUS: ACCEPTED`, or `STATUS: SUPERSEDED by` a ref
   that resolves, yields no `bad-decision-status` finding
   (test: `test_check_good_decision_status`).
3. A `Supersedes:` or `SUPERSEDED by` ref to a nonexistent DECISION.md yields a
   `dangling-supersede` finding (test: `test_check_dangling_supersede`).
4. A ref that resolves to an existing `tasks/<id>/DECISION.md` yields no
   `dangling-supersede` finding (test: `test_check_resolving_supersede`).
5. A task with no `DECISION.md` is unaffected - the existing suite stays green
   and both rules fire only when the file exists
   (cmd: `nix develop -c ./checker.sh`).
6. `tatr check` on tatr's own backlog stays clean
   (cmd: `cd ~/personal/tatr && ./tatr check`).
7. Docs list the two new default rules: tatr `README.md` Default-rules section
   (cmd: `grep -n "bad-decision-status\|dangling-supersede" README.md`) and the
   tatr skill's rule list in nix.dotfiles
   (cmd: `grep -n "bad-decision-status\|dangling-supersede" ~/personal/nix.dotfiles/home/modules/agents/skills/tatr/SKILL.md`).

Overall: `nix develop -c ./checker.sh` passes (including `--memcheck` clean) and
`tatr check`/`tatr check --ledger LESSONS.md` are clean on the tatr backlog.

## Tasks

Updated as tasks land (one line per land, like a spike's Fix record).

- [x] 20260722-152010 (p100, tatr) Implement bad-decision-status + dangling-supersede rules, tests, tatr-repo docs
      landed c2721ef; 1 review round (APPROVE, out-of-context); 70/70 tests + memcheck clean; 3 NITs left as-is

## Decisions (load-bearing, architectural)

- (none expected: this follows the established `bad-severity` validate-if-present
  pattern; any load-bearing deviation gets a DECISION.md here.)

## Manual acceptance (batched for the user at Finish)

- (none expected: all criteria are test:/cmd: provable.)
