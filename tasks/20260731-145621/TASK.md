# Cut v0.2.0: bump version and compact the CHANGELOG

- STATUS: CLOSED
- PRIORITY: 90
- TAGS: release, docs, build

## Story

As the tatr maintainer, I want v0.2.0 cut - version constants bumped, the
Unreleased changelog turned into a compact `## v0.2.0` section, and a local
annotated tag - so that pushing the tag later is the only step left to publish.

## Steps

- [x] Bump the version: `#define TATR_VERSION "0.2.0"` in `tatr.c`, both
  `version = "0.1.0";` lines in `flake.nix`.
- [x] Rewrite the `## Unreleased` section of `CHANGELOG.md` as
  `## v0.2.0 - 2026-07-31`, compacting 171 lines of prose to under 60. Keep
  Added/Changed/Fixed, one to three lines per entry, both **Breaking:**
  markers, and every command/rule name; drop rationale and design narrative
  (that lives in the task records). Do not leave an empty `## Unreleased`
  heading.
- [x] Run `nix develop -c make` and `nix develop -c ./checker.sh`.
- [x] Run the release workflow's own gates by hand against the bumped tree: its
  `Validate tag version` greps and its `awk` changelog extraction for
  `version=0.2.0`, whose output must be non-empty.
- [x] After the branch lands on `master`, create the annotated tag on the
  landed commit: `git tag -a v0.2.0 -m "tatr v0.2.0"`. Do not push it; the
  release publishes only when the tag is pushed.

## Definition of Done

- `tatr.c` and `flake.nix` both declare 0.2.0. (cmd:
  `grep -F '#define TATR_VERSION "0.2.0"' tatr.c && test "$(grep -c 'version = "0.2.0";' flake.nix)" = 2`)
- `CHANGELOG.md` has a `## v0.2.0 - 2026-07-31` section and no `## Unreleased`
  heading. (cmd:
  `grep -qx '## v0.2.0 - 2026-07-31' CHANGELOG.md && ! grep -q '^## Unreleased' CHANGELOG.md`)
- That section is non-empty and under 60 lines. (cmd:
  `n=$(awk '/^## v0.2.0/{s=1;next} /^## v/{s=0} s' CHANGELOG.md | wc -l); test "$n" -gt 0 && test "$n" -lt 60`)
- The suite passes on the bumped tree. (cmd: `nix develop -c ./checker.sh`)
- The release workflow's tag validation and changelog extraction succeed for
  0.2.0. (cmd:
  `grep -F '#define TATR_VERSION "0.2.0"' tatr.c && grep -F 'version = "0.2.0";' flake.nix && test "$(awk -v version=0.2.0 '/^## v/ && seen { exit } /^## v/ && $2 == "v" version { seen = 1; next } seen { print }' CHANGELOG.md | wc -l)" -gt 5`)
- An annotated `v0.2.0` tag exists locally on the landed commit. (cmd:
  `test "$(git cat-file -t v0.2.0)" = tag && git merge-base --is-ancestor v0.2.0^{commit} master`)
- The tag is not pushed and no release is published. (manual: user judgement)

## Notes

- Release trigger is `.github/workflows/release.yml` on a pushed `v*` tag. It
  already validates `tatr.c` and `flake.nix` against the tag and builds the
  release notes by `awk`-extracting the `## v<version>` section, so no new
  local check is added; the workflow stays the only gate.
- `CHANGELOG.md` currently carries 171 lines under `## Unreleased`; `v0.1.0`
  below it is already terse and is the compaction target style.
- Version sources found by grep: `tatr.c:13`, `flake.nix:38`, `flake.nix:61`.
  `README.md:220` mentions `v0.1.0` only as a filter-syntax example and must
  not change.
- `dist/` is gitignored; release artifacts are built by CI, not committed.
- Tag creation is deliberately the last step, after landing, so the tag names
  the commit that is actually released.

## Implementation Notes

- `tatr.c:13` and both `flake.nix` version lines now read 0.2.0; `dist/tatr
  version` prints `tatr 0.2.0`. `README.md:220`'s `v0.1.0` is filter-syntax
  prose and was left alone.
- The `## Unreleased` section (171 lines) became `## v0.2.0 - 2026-07-31` at 59
  lines. Every command and rule slug survived; what was cut is rationale and
  design narrative, which is already in the per-task records this repository
  keeps under `tasks/`.
- No new local version check was added: `.github/workflows/release.yml` already
  greps `tatr.c` and `flake.nix` against the tag and awk-extracts the notes, and
  the user asked to keep this simple. Those two gates were run by hand here
  instead.
- Landed as d4e976c on master; `git tag -a v0.2.0` names that commit and is
  not pushed, so the release workflow has not run.
- The suite passes 107/107 in the worktree. `checker.sh`'s windows test runs
  `make clean`, so `dist/tatr` has to be rebuilt after a suite run before
  invoking the binary - the `serialize-build-artifact-checks` lesson again.

## Reflection

- The tradeoff in compacting was rationale versus reference value. The rule and
  command names are what a reader greps a changelog for; the "why" belongs to
  the task records, so the cut fell entirely on narrative.
- Verifying the release workflow by running its own grep and awk lines against
  the tree is cheaper than a test that restates them, and it cannot drift from
  the workflow the way a copy would.
