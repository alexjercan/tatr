# Release process

GitHub publishes a release when a `v*` tag is pushed. The workflow is
`.github/workflows/release.yml`. It builds Linux and Windows archives, writes
`SHA256SUMS`, runs the integration suite, and uses the matching `CHANGELOG.md`
section as release notes.

Use an annotated tag. Do not move or reuse a pushed release tag.

## 1. Prepare

Requires: Git, Nix, GitHub CLI (`gh`), `tar`, `unzip`, and `sha256sum`.
Start on `master` with the intended changes committed and a clean worktree.
Choose the version and release date. Confirm that the tag does not exist.

```bash
version=2.0.2
release_date=2026-08-09

git status --short
! git rev-parse -q --verify "refs/tags/v$version"
```

Create a release task and record the checks in its body.

```bash
nix develop -c make
dist/tatr new "Release v$version" -p 100 -t release
task_id=YYYYMMDD-HHMMSS  # Use the ID printed by tatr.
```

## 2. Update release files

Update all version surfaces:

- `TATR_VERSION` in `tatr.c`.
- Both package `version` values in `flake.nix`.

Add a top-level changelog section:

```markdown
## v2.0.2 - 2026-08-09

### Fixed

- Concise user-visible change.
```

Use `Added`, `Changed`, `Fixed`, or `Removed` as needed. Keep release notes
short and user-facing. Do not add an empty `Unreleased` section.

Verify the version surfaces and the exact changelog heading.

```bash
grep -Fx "#define TATR_VERSION \"$version\"" tatr.c
test "$(grep -Fc "version = \"$version\";" flake.nix)" -eq 2
grep -Fx "## v$version - $release_date" CHANGELOG.md
```

## 3. Run release checks

Run every supported build and the integration suite.

```bash
nix develop -c make clean all
nix develop -c ./checker.sh
nix develop -c ./checker.sh --memcheck
nix develop -c make clean all CC=gcc
nix develop -c make windows
nix flake check
```

Rebuild the canonical Linux binary, then verify its reported version.

```bash
nix develop -c make clean all
test "$(dist/tatr version)" = "tatr $version"
```

Run the release workflow's version and changelog gates locally.

```bash
notes_file="$(mktemp)"
awk -v version="$version" '
  /^## v/ && seen { exit }
  /^## v/ && $2 == "v" version { seen = 1; next }
  seen { print }
' CHANGELOG.md > "$notes_file"
test -s "$notes_file"
rm -f "$notes_file"
```

Do not commit files under `dist/`. CI rebuilds the release artifacts from the
tagged commit.

## 4. Close and commit

Add results and evidence to the release task. Close it only after every check
passes.

```bash
dist/tatr edit "$task_id" --status CLOSED
git diff --check
git status --short
git add CHANGELOG.md flake.nix tatr.c RELEASE.md AGENTS.md \
  "tasks/$task_id/TASK.md"
git diff --cached --check
git commit -m "release: cut v$version"
```

Include only the release changes and their task record.

## 5. Tag

Create the annotated tag on the release commit and verify its target.

```bash
git tag -a "v$version" -m "tatr v$version"
test "$(git cat-file -t "v$version")" = tag
test "$(git rev-list -n 1 "v$version")" = "$(git rev-parse HEAD)"
```

Before pushing, delete a bad local tag with `git tag -d "v$version"`, fix the
release commit, and create the tag again. Never rewrite a pushed release tag.
Use a new patch version for a published release correction.

## 6. Publish

Push the release commit first. Push the tag only when ready to publish.

```bash
git push origin master
git push origin "v$version"
```

The tag push starts the Release workflow. Wait for it and require success.

```bash
run_id="$(gh run list --workflow Release --branch "v$version" --limit 1 \
  --json databaseId --jq '.[0].databaseId')"
test -n "$run_id"
gh run watch "$run_id" --exit-status
```

## 7. Verify the published release

Confirm the release metadata and all checksums. The archives contain the raw
binaries referenced by `SHA256SUMS`, so extract them before checking.

```bash
gh release view "v$version"
verify_dir="$(mktemp -d)"
mkdir "$verify_dir/dist"
gh release download "v$version" --dir "$verify_dir/dist"
tar -xzf "$verify_dir/dist/tatr-$version-linux-x86_64.tar.gz" \
  -C "$verify_dir/dist"
unzip -q "$verify_dir/dist/tatr-$version-windows-x86_64.zip" \
  -d "$verify_dir/dist"
(cd "$verify_dir" && sha256sum -c dist/SHA256SUMS)
rm -rf "$verify_dir"
```
