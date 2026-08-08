#!/usr/bin/env bash

set -eu

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TATR_BIN="$PROJECT_DIR/dist/tatr"
MEMCHECK=0
VERBOSE=0
TOTAL=0
PASSED=0
TEST_DIRS=()

usage() {
    echo "Usage: $0 [--memcheck] [-v | --verbose] [-h | --help]"
}

cleanup() {
    local dir
    for dir in "${TEST_DIRS[@]}"; do
        [ ! -d "$dir" ] || rm -rf "$dir"
    done
}

trap cleanup EXIT

while [ "$#" -gt 0 ]; do
    case "$1" in
        --memcheck) MEMCHECK=1 ;;
        -v|--verbose) VERBOSE=1 ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; exit 1 ;;
    esac
    shift
done

run_tatr() {
    if [ "$MEMCHECK" -eq 1 ]; then
        valgrind --quiet --leak-check=full --show-leak-kinds=all \
            --errors-for-leak-kinds=all --error-exitcode=42 "$TATR_BIN" "$@"
    else
        "$TATR_BIN" "$@"
    fi
}

new_project() {
    local dir
    dir="$(mktemp -d)"
    TEST_DIRS+=("$dir")
    mkdir "$dir/tasks"
    printf '%s\n' "$dir"
}

check() {
    local name="$1"
    shift
    TOTAL=$((TOTAL + 1))
    if "$@"; then
        PASSED=$((PASSED + 1))
        printf 'ok %d - %s\n' "$TOTAL" "$name"
    else
        printf 'not ok %d - %s\n' "$TOTAL" "$name"
        return 1
    fi
}

test_help_surface() {
    local output
    output="$(run_tatr help 2>&1)"
    printf '%s\n' "$output" | grep -q '  new '
    printf '%s\n' "$output" | grep -q '  ls '
    printf '%s\n' "$output" | grep -q '  edit '
    ! printf '%s\n' "$output" | grep -Eq '  (show|rm|check|flow|migrate|scaffold|proofs|claim|release|frontier|context) '
}

test_new() {
    local dir file
    dir="$(new_project)"
    run_tatr -r "$dir" new 'Ship small tatr' -s IN_PROGRESS -p 42 -t cli -t simple >/dev/null
    file="$(find "$dir/tasks" -name TASK.md)"
    grep -qx '# Ship small tatr' "$file"
    grep -qx -- '- STATUS: IN_PROGRESS' "$file"
    grep -qx -- '- PRIORITY: 42' "$file"
    grep -qx -- '- TAGS: cli, simple' "$file"
}

write_tasks() {
    local dir="$1"
    mkdir "$dir/tasks/20260101-000001" "$dir/tasks/20260101-000002" "$dir/tasks/20260101-000003"
    printf '# Low\n\n- STATUS: OPEN\n- PRIORITY: 1\n- TAGS: cli\n\nbody\n' > "$dir/tasks/20260101-000001/TASK.md"
    printf '# High\n\n- STATUS: IN_PROGRESS\n- PRIORITY: 100\n- TAGS: cli, bug\n\nbody\n' > "$dir/tasks/20260101-000002/TASK.md"
    printf '# Closed\n\n- STATUS: CLOSED\n- PRIORITY: 20\n- TAGS: docs\n\nbody\n' > "$dir/tasks/20260101-000003/TASK.md"
}

test_ls_sort_and_query() {
    local dir output
    dir="$(new_project)"
    write_tasks "$dir"
    output="$(run_tatr -r "$dir" ls --sort priority --filter '(:status eq IN_PROGRESS) or (:tags contains docs)')"
    [ "$(printf '%s\n' "$output" | grep -c 'High')" -eq 1 ]
    [ "$(printf '%s\n' "$output" | grep -c 'Closed')" -eq 1 ]
    ! printf '%s\n' "$output" | grep -q 'Low'
    [ "$(printf '%s\n' "$output" | sed 's/.*] //' | paste -sd, -)" = 'High,Closed' ]
}

test_edit() {
    local dir file
    dir="$(new_project)"
    write_tasks "$dir"
    file="$dir/tasks/20260101-000001/TASK.md"
    run_tatr -r "$dir" edit 20260101-000001 --title Changed --status CLOSED --priority 77 --tags done >/dev/null
    grep -qx '# Changed' "$file"
    grep -qx -- '- STATUS: CLOSED' "$file"
    grep -qx -- '- PRIORITY: 77' "$file"
    grep -qx -- '- TAGS: done' "$file"
    grep -qx 'body' "$file"
}

test_invalid_task_fails_ls() {
    local dir output exit_code
    dir="$(new_project)"
    mkdir "$dir/tasks/20260101-000001"
    printf '# Bad\n\n- STATUS: MAYBE\n- PRIORITY: 1\n- TAGS: bad\n' > "$dir/tasks/20260101-000001/TASK.md"
    set +e
    output="$(run_tatr -r "$dir" ls 2>&1)"
    exit_code=$?
    set -e
    [ "$exit_code" -ne 0 ]
    printf '%s\n' "$output" | grep -q '20260101-000001/TASK.md'
}

test_removed_commands_fail() {
    local command exit_code
    for command in show rm check flow migrate scaffold proofs claim release claims frontier context; do
        set +e
        run_tatr "$command" >/dev/null 2>&1
        exit_code=$?
        set -e
        [ "$exit_code" -ne 0 ]
    done
}

test_invalid_new_values_fail() {
    local dir exit_code
    dir="$(new_project)"
    set +e
    run_tatr -r "$dir" new Bad --status MAYBE >/dev/null 2>&1
    exit_code=$?
    set -e
    [ "$exit_code" -ne 0 ]
    set +e
    run_tatr -r "$dir" new Bad --priority -1 >/dev/null 2>&1
    exit_code=$?
    set -e
    [ "$exit_code" -ne 0 ]
    [ "$(find "$dir/tasks" -name TASK.md | wc -l)" -eq 0 ]
}

check 'help has only retained commands' test_help_surface
check 'new writes the three metadata fields' test_new
check 'ls sorts and queries' test_ls_sort_and_query
check 'edit updates fields and keeps body' test_edit
check 'ls fails on invalid TASK.md' test_invalid_task_fails_ls
check 'removed commands fail' test_removed_commands_fail
check 'new rejects invalid metadata' test_invalid_new_values_fail

printf '%d/%d tests passed\n' "$PASSED" "$TOTAL"
