# Remove IN_PROGRESS task status

- STATUS: CLOSED
- PRIORITY: 0
- TAGS: cli

Remove IN_PROGRESS from Tatr status validation and documentation. Update Tatr skills under ~/personal to list only OPEN and CLOSED. Change current IN_PROGRESS tasks in those projects to OPEN.

## Verification

- `nix develop -c make clean all`: passed.
- `nix develop -c ./checker.sh`: 9/9 tests passed.
- Scanned all `SKILL.md` files under `~/personal`: no IN_PROGRESS references remain.
- Scanned all task status fields under `~/personal`: no IN_PROGRESS tasks remain.
