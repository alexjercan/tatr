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
    run_tatr -r "$dir" new 'Ship small tatr' -s CLOSED -p 42 -t cli -t simple >/dev/null
    file="$(find "$dir/tasks" -name TASK.md)"
    grep -qx '# Ship small tatr' "$file"
    grep -qx -- '- STATUS: CLOSED' "$file"
    grep -qx -- '- PRIORITY: 42' "$file"
    grep -qx -- '- TAGS: cli, simple' "$file"
}

test_new_body() {
    local dir file output exit_code
    dir="$(new_project)"
    printf '# File body\n\nContent from file.\n' > "$dir/body.md"
    run_tatr -r "$dir" new 'File input' --body "$dir/body.md" >/dev/null || return 1
    file="$(find "$dir/tasks" -name TASK.md)"
    [ -n "$file" ] || return 1
    tail -n 3 "$file" | grep -qx 'Content from file.' || return 1

    dir="$(new_project)"
    printf '# Stdin body\n\nContent from stdin.\n' > "$dir/stdin.md"
    run_tatr -r "$dir" new 'Stdin input' -b - < "$dir/stdin.md" >/dev/null || return 1
    file="$(find "$dir/tasks" -name TASK.md)"
    [ -n "$file" ] || return 1
    tail -n 3 "$file" | grep -qx 'Content from stdin.' || return 1

    dir="$(new_project)"
    set +e
    output="$(run_tatr -r "$dir" new 'Missing body' --body "$dir/missing.md" 2>&1)"
    exit_code=$?
    set -e
    [ "$exit_code" -ne 0 ] || return 1
    printf '%s\n' "$output" | grep -qx ".*Failed to read body from '$dir/missing.md': Failed to open file '$dir/missing.md' for reading" || return 1
    [ "$(find "$dir/tasks" -name TASK.md | wc -l)" -eq 0 ] || return 1
}

write_tasks() {
    local dir="$1"
    mkdir "$dir/tasks/20260101-000001" "$dir/tasks/20260101-000002" "$dir/tasks/20260101-000003"
    printf '# Low\n\n- STATUS: OPEN\n- PRIORITY: 1\n- TAGS: cli\n\nbody\n' > "$dir/tasks/20260101-000001/TASK.md"
    printf '# High\n\n- STATUS: OPEN\n- PRIORITY: 100\n- TAGS: cli, bug\n\nbody\n' > "$dir/tasks/20260101-000002/TASK.md"
    printf '# Closed\n\n- STATUS: CLOSED\n- PRIORITY: 20\n- TAGS: docs\n\nbody\n' > "$dir/tasks/20260101-000003/TASK.md"
}

test_ls_sort_and_query() {
    local dir output
    dir="$(new_project)"
    write_tasks "$dir"
    output="$(run_tatr -r "$dir" ls --sort priority --filter '((:status eq OPEN) and (:priority eq 100)) or (:tags contains docs)')"
    [ "$(printf '%s\n' "$output" | grep -c 'High')" -eq 1 ]
    [ "$(printf '%s\n' "$output" | grep -c 'Closed')" -eq 1 ]
    ! printf '%s\n' "$output" | grep -q 'Low'
    [ "$(printf '%s\n' "$output" | sed 's/.*] //' | paste -sd, -)" = 'High,Closed' ]
}

test_filter_tag_literal_punctuation() {
    local dir dotted_output dashed_output output exit_code
    dir="$(new_project)"
    mkdir "$dir/tasks/20000101-000001"
    printf '# Unrelated\n\n- STATUS: OPEN\n- PRIORITY: 0\n- TAGS: other\n' > "$dir/tasks/20000101-000001/TASK.md"
    run_tatr -r "$dir" new Punctuation -t v0.1.0 -t release-candidate >/dev/null

    dotted_output="$(run_tatr -r "$dir" ls --filter ':tags contains v0.1.0')"
    printf '%s\n' "$dotted_output" | grep -qx '.*\[PRIORITY: 0, TAGS: v0.1.0, release-candidate\] Punctuation' || return 1

    dashed_output="$(run_tatr -r "$dir" ls --filter ':tags contains release-candidate')"
    printf '%s\n' "$dashed_output" | grep -qx '.*\[PRIORITY: 0, TAGS: v0.1.0, release-candidate\] Punctuation' || return 1

    set +e
    output="$(run_tatr -r "$dir" ls --filter ':tags contains .hidden' 2>&1)"
    exit_code=$?
    set -e
    [ "$exit_code" -ne 0 ] || return 1
    printf '%s\n' "$output" | grep -qx ".*Filter error: line 1, col 16: expected field, identifier, list, or '(', got invalid token" || return 1
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

test_in_progress_is_rejected() {
    local dir file output exit_code
    dir="$(new_project)"

    set +e
    output="$(run_tatr -r "$dir" new Bad --status IN_PROGRESS 2>&1)"
    exit_code=$?
    set -e
    [ "$exit_code" -ne 0 ] || return 1
    printf '%s\n' "$output" | grep -qx ".*Invalid status 'IN_PROGRESS': expected OPEN or CLOSED" || return 1
    [ "$(find "$dir/tasks" -name TASK.md | wc -l)" -eq 0 ] || return 1

    mkdir "$dir/tasks/20260101-000001"
    file="$dir/tasks/20260101-000001/TASK.md"
    printf '# Open\n\n- STATUS: OPEN\n- PRIORITY: 1\n- TAGS: test\n' > "$file"
    set +e
    output="$(run_tatr -r "$dir" edit 20260101-000001 --status IN_PROGRESS 2>&1)"
    exit_code=$?
    set -e
    [ "$exit_code" -ne 0 ] || return 1
    printf '%s\n' "$output" | grep -qx ".*Invalid status 'IN_PROGRESS': expected OPEN or CLOSED" || return 1
    grep -qx -- '- STATUS: OPEN' "$file" || return 1

    set +e
    output="$(run_tatr -r "$dir" ls --filter ':status eq IN_PROGRESS' 2>&1)"
    exit_code=$?
    set -e
    [ "$exit_code" -ne 0 ] || return 1
    printf '%s\n' "$output" | grep -qx ".*Filter error: line 1, col 12: invalid status value 'IN_PROGRESS' (must be OPEN or CLOSED)" || return 1

    printf '# Bad\n\n- STATUS: IN_PROGRESS\n- PRIORITY: 1\n- TAGS: bad\n' > "$file"
    set +e
    output="$(run_tatr -r "$dir" ls 2>&1)"
    exit_code=$?
    set -e
    [ "$exit_code" -ne 0 ] || return 1
    printf '%s\n' "$output" | grep -qx '.*Invalid status in TASK.md: expected OPEN or CLOSED' || return 1
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
check 'new reads body from a file or stdin' test_new_body
check 'ls sorts and queries' test_ls_sort_and_query
check 'ls filters tag literals containing dots and dashes' test_filter_tag_literal_punctuation
check 'edit updates fields and keeps body' test_edit
check 'IN_PROGRESS is rejected everywhere' test_in_progress_is_rejected
check 'removed commands fail' test_removed_commands_fail
check 'new rejects invalid metadata' test_invalid_new_values_fail

printf '%d/%d tests passed\n' "$PASSED" "$TOTAL"
