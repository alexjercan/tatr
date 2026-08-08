# Document new body input

- STATUS: CLOSED
- PRIORITY: 100
- TAGS: docs

Document `tatr new -b, --body` in agent guidance, user documentation, and the
bundled tatr skill.

## Plan

- [x] Add the body-input invariant to `AGENTS.md`.
- [x] Add file and stdin examples plus failure semantics to `README.md`.
- [x] Add body usage to `skills/tatr/SKILL.md` and validate the skill.

## Done when

- All three documentation surfaces describe file and stdin input.
- Documentation matches implemented behavior.
- Skill content is valid for this repository and repository checks pass.

## Result

- Added body-input guidance to `AGENTS.md`.
- Expanded `README.md` with file and stdin examples and read-failure semantics.
- Updated the tatr skill command examples and operating guidance.

## Evidence

- `nix develop -c make clean all`: pass.
- `nix develop -c ./checker.sh`: 8/8 pass.
- `git diff --check`: pass.
- Generic skill validator reached frontmatter validation, then rejected the existing project-specific `disable-model-invocation` key. Preserved the key because it controls skill invocation behavior.

## Reflection

- Rebuild main before integration checks after landing a worktree. The existing `dist/tatr` can predate the landed source.
