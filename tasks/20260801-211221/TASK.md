# Remove lesson ledger ownership from tatr

- STATUS: OPEN
- PRIORITY: 70
- TAGS: tatr, knowledge, lessons, migration, tooling
- KIND: TASK
- FLOW STEP: PLANNED
- PLAN STATUS: APPROVED

## Story

As a tatr user, I want the tracker to own only task and record lifecycle so
knowledge schema, checks, decisions, and release cadence remain in the
agent-knowledge repository.

## Steps

- [ ] In `/home/alex/personal/tatr`, remove `check --ledger`, the `ledger`
  command, parser/rendering helpers, disposition state, help/options, check
  findings, and their integration fixtures. Add negative help/argument tests
  and keep shared task collectors unchanged.
- [ ] Remove ledger contracts from tatr README, CHANGELOG unreleased notes,
  AGENTS.md, and the tool-owned tatr skill references. State only project task
  and record ownership; point general knowledge maintenance to global agent
  configuration rather than teaching the external schema.
- [ ] Run the full native, verbose, and memcheck integration suites plus a
  mutation that restores one removed dispatch/flag and makes its absence test
  fail. Release a ledger-free tatr version.
- [ ] In `/home/alex/personal/nix.dotfiles`, pin that release, refresh
  `flake.lock`, and update the
  imported tatr skill/deployment checks. Sweep live files for old command and
  ledger terminology without rewriting historical `tasks/` records.
- [ ] Re-run tatr, nix flake, skill conformance, deployment-tree, and central
  knowledge checks across the final dependency seam.

## Definition of Done

- Tatr help and argument parsing expose no ledger command or option. (test: `ledger_interface_is_absent`)
- Tatr contains no live lesson-ledger implementation or documentation.
  (cmd: `! rg -n 'tatr ledger|--ledger|LESSONS\.md|lesson ledger' /home/alex/personal/tatr/tatr.c /home/alex/personal/tatr/checker.sh /home/alex/personal/tatr/README.md /home/alex/personal/tatr/skills/tatr`)
- Task checks, proof listing, lifecycle gates, and record scaffolding retain
  their existing behavior. (test: `task_features_survive_ledger_removal`)
- The removal passes native and memcheck integration suites. (test: `ledger_removal_full_suite`)
- Nix pins and deploys the released ledger-free tatr package and skill. (test: `ledger_free_tatr_input_is_deployed`)
- No live nix.dotfiles skill or AGENTS contract calls a tatr ledger interface.
  (cmd: `! rg -n 'tatr ledger|--ledger' /home/alex/personal/nix.dotfiles/AGENTS.md /home/alex/personal/nix.dotfiles/home/modules/agents --glob '*.md' --glob '*.sh' --glob '*.nix'`)

## Notes

- Ownership: tatr implementation/release first, then the nix.dotfiles input
  pin. Historical task records and changelog history remain unchanged.
- The removed parent and dependency IDs belong to the external orchestration
  source, not this repository's task graph.
- This story lands last so every project has already stopped calling the old
  interface. No compatibility shim: there is no remaining caller after the
  dependency chain.
- Final regression commands: `nix develop -c ./checker.sh`,
  `nix develop -c ./checker.sh --memcheck`, nix.dotfiles `nix flake check`,
  `bash home/modules/agents/skills/check.sh`, and central `knowledge check`.
