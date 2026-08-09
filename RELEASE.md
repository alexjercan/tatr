# Release checklist

Release from `master`. Use semantic versions and annotated tags in the form
`vX.Y.Z`. Never move or reuse a pushed release tag.

## Prepare

- [ ] Confirm the intended work is complete, `master` is green, and the
  worktree is clean.
- [ ] Review everything since the previous tag. Ensure user-visible changes
  are in `CHANGELOG.md` and affected documentation is current.
- [ ] Create a release task and record the release checks in its body.
- [ ] Confirm the intended tag does not already exist.

## Set the version

- [ ] Choose the next version from the scope of the changes.
- [ ] Update `TATR_VERSION` in `tatr.c`.
- [ ] Update both package `version` values in `flake.nix`.
- [ ] Rebuild the canonical Linux binary and confirm `tatr version` reports
  the new version.

## Finish the release notes

- [ ] Add a top-level `## vX.Y.Z - YYYY-MM-DD` section to `CHANGELOG.md`. Do
  not add an empty `Unreleased` section.
- [ ] Use `Added`, `Changed`, `Fixed`, or `Removed` headings as needed. Keep
  entries concise and user-facing.
- [ ] Check every entry against the changes since the previous tag and confirm
  the release section is not empty.
- [ ] Run every check in `AGENTS.md` and `nix flake check`. Confirm the
  worktree contains no files from `dist/`.
- [ ] Add the results to the release task and close it only after every check
  passes.

## Publish

- [ ] Commit the version, changelog, release task, and any final documentation
  updates.
- [ ] Check the final commit and create the annotated `vX.Y.Z` tag on it.
- [ ] Push `master`, then push the tag. The tag starts the GitHub Release
  workflow.
- [ ] Confirm the workflow succeeds and the Linux archive, Windows archive,
  and `SHA256SUMS` are attached to the GitHub release.
- [ ] Download the assets, extract both archives, and verify `SHA256SUMS`.
