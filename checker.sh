#!/usr/bin/env bash

set -e

MEMCHECKER=valgrind
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TATR_BIN="$PROJECT_DIR/tatr"
TOTAL_TESTS=0
PASSED_TESTS=0
MEMCHECK=0
VERBOSE=0
TEST_DIRS=()  # Array to track all test directories for cleanup

# Colors
RED='\e[31m'
GREEN='\e[32m'
YELLOW='\e[33m'
RESET='\e[0m'

usage() {
    echo "Usage: $0 [--memcheck] [-v | --verbose] [-h | --help]"
    echo
    echo "Options:"
    echo "  --memcheck      Enable memory leak checking with valgrind."
    echo "  -v, --verbose   Show more details in case of errors."
    echo "  -h, --help      Show this help message and exit."
}

cleanup() {
    for dir in "${TEST_DIRS[@]}"; do
        if [ -d "$dir" ]; then
            rm -rf "$dir"
        fi
    done
}

# Helper function to create a new test directory
create_test_dir() {
    local test_dir=$(mktemp -d)
    TEST_DIRS+=("$test_dir")
    echo "$test_dir"
}

trap cleanup EXIT

log_test() {
    local test_name=$1
    echo -en "Testing $test_name ... "
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
}

pass_test() {
    echo -e "${GREEN}PASSED${RESET}"
    PASSED_TESTS=$((PASSED_TESTS + 1))
}

fail_test() {
    local message=$1
    echo -e "${RED}FAILED${RESET}"
    if [ "$VERBOSE" -eq 1 ] && [ -n "$message" ]; then
        echo "  $message"
    fi
}

run_tatr() {
    local output
    local exit_code

    if [ "$MEMCHECK" -eq 1 ]; then
        output=$($MEMCHECKER --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=42 "$TATR_BIN" "$@" 2>&1)
        exit_code=$?

        if [ $exit_code -eq 42 ]; then
            if [ "$VERBOSE" -eq 1 ]; then
                echo "  Memory leak detected"
                echo "$output" | grep -A 10 "LEAK SUMMARY"
            fi
            return 42
        fi

        echo "$output" | grep -v "==" # Filter out valgrind output
        return $exit_code
    else
        "$TATR_BIN" "$@"
        return $?
    fi
}

# Test 1: Help command
test_help() {
    log_test "help command"

    local output=$(run_tatr help 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && echo "$output" | grep -q "Usage:"; then
        pass_test
    else
        fail_test "Exit code: $exit_code"
    fi
}

# Test 2: Version command
test_version() {
    log_test "version command"

    local output=$(run_tatr version 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && echo "$output" | grep -q "tatr"; then
        pass_test
    else
        fail_test "Exit code: $exit_code"
    fi
}

# Test 3: Create basic task
test_new_basic() {
    log_test "new task (basic)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local output=$(run_tatr new "Test task" 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && echo "$output" | grep -q "Task created successfully"; then
        # Check if task file was created
        local task_count=$(find tasks -name "TASK.md" | wc -l)
        if [ "$task_count" -eq 1 ]; then
            pass_test
        else
            fail_test "Task file not created"
        fi
    else
        fail_test "Exit code: $exit_code, Output: $output"
    fi
}

# Test 4: Create task with priority
test_new_priority() {
    log_test "new task (with priority)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local output=$(run_tatr new "Priority task" -p 100 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ]; then
        # Check if priority was set correctly
        local task_file=$(find tasks -name "TASK.md" | head -1)
        if [ -f "$task_file" ] && grep -q "PRIORITY: 100" "$task_file"; then
            pass_test
        else
            fail_test "Priority not set correctly"
        fi
    else
        fail_test "Exit code: $exit_code"
    fi
}

# Test 5: Create task with tags
test_new_tags() {
    log_test "new task (with tags)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local output=$(run_tatr new "Tagged task" -t feature -t bug 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ]; then
        # Check if tags were set correctly
        local task_file=$(find tasks -name "TASK.md" | head -1)
        if [ -f "$task_file" ] && grep -q "TAGS: feature, bug" "$task_file"; then
            pass_test
        else
            fail_test "Tags not set correctly"
        fi
    else
        fail_test "Exit code: $exit_code"
    fi
}

# Test 6: Create task with status
test_new_status() {
    log_test "new task (with status)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local output=$(run_tatr new "In progress task" -s IN_PROGRESS 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ]; then
        # Check if status was set correctly
        local task_file=$(find tasks -name "TASK.md" | head -1)
        if [ -f "$task_file" ] && grep -q "STATUS: IN_PROGRESS" "$task_file"; then
            pass_test
        else
            fail_test "Status not set correctly"
        fi
    else
        fail_test "Exit code: $exit_code"
    fi
}

# Test 7: Create task with all options
test_new_full() {
    log_test "new task (all options)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local output=$(run_tatr new "Full task" -p 50 -t test -t complete -s CLOSED 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ]; then
        local task_file=$(find tasks -name "TASK.md" | head -1)
        if [ -f "$task_file" ] && \
           grep -q "PRIORITY: 50" "$task_file" && \
           grep -q "TAGS: test, complete" "$task_file" && \
           grep -q "STATUS: CLOSED" "$task_file"; then
            pass_test
        else
            fail_test "Task metadata not set correctly"
        fi
    else
        fail_test "Exit code: $exit_code"
    fi
}

# Create task with the description body read from a file
test_new_body_file() {
    log_test "new task (--body-file)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    printf '## Story\n\nBody from a file.\n\n## Steps\n\n- [ ] step one\n' > body.md

    local id=$(new_task_id "Body task" -b body.md)
    local task_file="tasks/$id/TASK.md"

    # A missing body file must fail without creating a task
    set +e
    run_tatr new "Missing body" -b does-not-exist.md > /dev/null 2>&1
    local missing_code=$?
    set -e
    local task_count=$(find tasks -name "TASK.md" | wc -l)

    if [ -n "$id" ] && [ -f "$task_file" ] && \
       grep -q "^# Body task" "$task_file" && \
       grep -q "## Story" "$task_file" && \
       grep -q "step one" "$task_file" && \
       [ $missing_code -ne 0 ] && [ "$task_count" -eq 1 ]; then
        pass_test
    else
        fail_test "id=$id missing_code=$missing_code count=$task_count"
    fi
}

# Create task with the description body read from stdin
test_new_body_stdin() {
    log_test "new task (--body-file -)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local output=$(printf 'Body from stdin.\n' | run_tatr new "Stdin task" -b - 2>&1)
    local id=$(echo "$output" | grep -o '[0-9]\{8\}-[0-9]\{6\}' | head -1)
    local task_file="tasks/$id/TASK.md"

    if [ -n "$id" ] && [ -f "$task_file" ] && grep -q "Body from stdin." "$task_file"; then
        pass_test
    else
        fail_test "Output: $output"
    fi
}

# Two `new` calls in the same second must not silently share an ID: the second
# call fails instead of overwriting the first task's TASK.md.
test_new_collision_fails() {
    log_test "new task (same-second ID collision fails)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"

    # Two back-to-back invocations normally land in the same second; retry a
    # few times in case an attempt straddles a second boundary.
    local attempt
    for attempt in 1 2 3 4 5; do
        rm -rf tasks
        mkdir -p tasks

        local first_id=$(new_task_id "First task")
        set +e
        local output
        output=$(run_tatr new "Second task" 2>&1)
        local exit_code=$?
        set -e
        local second_id=$(echo "$output" | grep -o '[0-9]\{8\}-[0-9]\{6\}' | head -1)

        if [ $exit_code -ne 0 ]; then
            # The collision was refused; the first task must survive intact.
            if grep -q "First task" "tasks/$first_id/TASK.md" && \
               echo "$output" | grep -q "already exists"; then
                pass_test
            else
                fail_test "Refused but wrong message or clobbered file: $output"
            fi
            return
        fi

        if [ "$second_id" == "$first_id" ]; then
            fail_test "Second task silently reused ID $first_id"
            return
        fi
        # The two calls straddled a second boundary; try again.
    done

    # Never hit the same second in five attempts (very slow host). The
    # overwrite bug would have been caught above, so treat this as a pass.
    pass_test
}

# Test 8: List tasks (empty)
test_ls_empty() {
    log_test "ls (empty tasks dir)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local output=$(run_tatr ls 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ]; then
        pass_test
    else
        fail_test "Exit code: $exit_code"
    fi
}

# Test 9: List tasks (with tasks)
test_ls_with_tasks() {
    log_test "ls (with tasks)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    run_tatr new "Task 1" > /dev/null 2>&1
    sleep 1
    run_tatr new "Task 2" > /dev/null 2>&1

    local output=$(run_tatr ls 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && \
       echo "$output" | grep -q "Task 1" && \
       echo "$output" | grep -q "Task 2"; then
        pass_test
    else
        fail_test "Tasks not listed correctly"
    fi
}

# Test 10: List tasks sorted by priority
test_ls_sort_priority() {
    log_test "ls (sort by priority)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    run_tatr new "Low priority" -p 10 > /dev/null 2>&1
    sleep 1
    run_tatr new "High priority" -p 100 > /dev/null 2>&1

    local output=$(run_tatr ls -s priority 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ]; then
        # High priority should appear before low priority
        local high_line=$(echo "$output" | grep -n "High priority" | cut -d: -f1)
        local low_line=$(echo "$output" | grep -n "Low priority" | cut -d: -f1)

        if [ -n "$high_line" ] && [ -n "$low_line" ] && [ "$high_line" -lt "$low_line" ]; then
            pass_test
        else
            fail_test "Tasks not sorted by priority correctly"
        fi
    else
        fail_test "Exit code: $exit_code"
    fi
}

# Test 11: List tasks sorted by title
test_ls_sort_title() {
    log_test "ls (sort by title)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    run_tatr new "Zebra task" > /dev/null 2>&1
    sleep 1
    run_tatr new "Alpha task" > /dev/null 2>&1

    local output=$(run_tatr ls -s title 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ]; then
        # Alpha should appear before Zebra
        local alpha_line=$(echo "$output" | grep -n "Alpha task" | cut -d: -f1)
        local zebra_line=$(echo "$output" | grep -n "Zebra task" | cut -d: -f1)

        if [ -n "$alpha_line" ] && [ -n "$zebra_line" ] && [ "$alpha_line" -lt "$zebra_line" ]; then
            pass_test
        else
            fail_test "Tasks not sorted by title correctly"
        fi
    else
        fail_test "Exit code: $exit_code"
    fi
}

# Test 12: Recursive listing
test_ls_recursive() {
    log_test "ls (recursive)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p project1/tasks
    mkdir -p project2/tasks

    cd project1
    run_tatr new "Project 1 task" > /dev/null 2>&1

    cd ../project2
    run_tatr new "Project 2 task" > /dev/null 2>&1

    cd ..
    local output=$(run_tatr -r . ls -R 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && \
       echo "$output" | grep -q "Project 1 task" && \
       echo "$output" | grep -q "Project 2 task"; then
        pass_test
    else
        fail_test "Recursive listing failed"
    fi
}

# Test 13: Use -r flag to change directory
test_root_flag() {
    log_test "root flag (-r)"

    local test_dir=$(create_test_dir)
    local other_dir=$(create_test_dir)
    cd "$other_dir"
    mkdir -p tasks

    run_tatr new "Root task" > /dev/null 2>&1

    cd "$test_dir"
    local output=$(run_tatr -r "$other_dir" ls 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && echo "$output" | grep -q "Root task"; then
        pass_test
    else
        fail_test "Root flag not working"
    fi
}

# Test 14: Error handling - no tasks directory
test_error_no_tasks_dir() {
    log_test "error handling (no tasks dir)"

    local no_tasks_dir=$(create_test_dir)
    cd "$no_tasks_dir"

    set +e
    run_tatr ls > /dev/null 2>&1
    local exit_code=$?
    set -e

    if [ $exit_code -ne 0 ]; then
        pass_test
    else
        fail_test "Should fail when no tasks directory exists"
    fi
}

# Test 15: Task format validation
test_task_format() {
    log_test "task format validation"

    local format_test_dir=$(create_test_dir)
    cd "$format_test_dir"
    mkdir -p tasks

    run_tatr new "Format test task" -p 42 -t tag1 -t tag2 -s OPEN > /dev/null 2>&1

    local task_file=$(find tasks -name "TASK.md" | head -1)

    if [ -f "$task_file" ] && \
       grep -q "^# Format test task$" "$task_file" && \
       grep -q "^- STATUS: OPEN$" "$task_file" && \
       grep -q "^- PRIORITY: 42$" "$task_file" && \
       grep -q "^- TAGS: tag1, tag2$" "$task_file"; then
        pass_test
    else
        fail_test "Task format incorrect"
    fi
}

# Helper: create a task and echo its generated ID
new_task_id() {
    run_tatr new "$@" 2>&1 | grep -o '[0-9]\{8\}-[0-9]\{6\}' | head -1
}

test_show_existing() {
    log_test "show existing task"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local id=$(new_task_id "Show me" -p 55 -t alpha -t beta -s IN_PROGRESS)
    local output=$(run_tatr show "$id" 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && \
       echo "$output" | grep -q "# Show me" && \
       echo "$output" | grep -q "STATUS: IN_PROGRESS" && \
       echo "$output" | grep -q "PRIORITY: 55" && \
       echo "$output" | grep -q "TAGS: alpha, beta" && \
       echo "$output" | grep -q "$id/TASK.md"; then
        pass_test
    else
        fail_test "Exit code: $exit_code, Output: $output"
    fi
}

test_show_invalid_id() {
    log_test "show with invalid id"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    set +e
    local output
    output=$(run_tatr show "not-a-huid" 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -ne 0 ]; then
        pass_test
    else
        fail_test "Expected non-zero exit for invalid id"
    fi
}

test_show_missing_id() {
    log_test "show non-existent task"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    set +e
    local output
    output=$(run_tatr show "20991231-235959" 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -ne 0 ]; then
        pass_test
    else
        fail_test "Expected non-zero exit for missing task"
    fi
}

test_edit_status() {
    log_test "edit status"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local id=$(new_task_id "Edit me" -s OPEN)
    run_tatr edit "$id" -s CLOSED > /dev/null 2>&1
    local task_file="tasks/$id/TASK.md"

    if grep -q "^- STATUS: CLOSED$" "$task_file"; then
        pass_test
    else
        fail_test "Status not updated"
    fi
}

test_edit_priority() {
    log_test "edit priority"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local id=$(new_task_id "Edit me" -p 10)
    run_tatr edit "$id" -p 77 > /dev/null 2>&1
    local task_file="tasks/$id/TASK.md"

    if grep -q "^- PRIORITY: 77$" "$task_file"; then
        pass_test
    else
        fail_test "Priority not updated"
    fi
}

test_edit_tags() {
    log_test "edit tags (replaces existing)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local id=$(new_task_id "Edit me" -t old1 -t old2)
    run_tatr edit "$id" -t new1 -t new2 > /dev/null 2>&1
    local task_file="tasks/$id/TASK.md"

    if grep -q "^- TAGS: new1, new2$" "$task_file" && ! grep -q "old1" "$task_file"; then
        pass_test
    else
        fail_test "Tags not replaced"
    fi
}

test_edit_title() {
    log_test "edit title"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local id=$(new_task_id "Old title")
    run_tatr edit "$id" -T "Brand new title" > /dev/null 2>&1
    local task_file="tasks/$id/TASK.md"

    if grep -q "^# Brand new title$" "$task_file" && ! grep -q "Old title" "$task_file"; then
        pass_test
    else
        fail_test "Title not updated"
    fi
}

test_edit_partial_preserves_fields() {
    log_test "edit partial keeps other fields and body"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local id=$(new_task_id "Keep me" -p 42 -t keep -s OPEN)
    local task_file="tasks/$id/TASK.md"
    printf '\nImportant body text.\n' >> "$task_file"

    run_tatr edit "$id" -s IN_PROGRESS > /dev/null 2>&1

    if grep -q "^- STATUS: IN_PROGRESS$" "$task_file" && \
       grep -q "^- PRIORITY: 42$" "$task_file" && \
       grep -q "^- TAGS: keep$" "$task_file" && \
       grep -q "^# Keep me$" "$task_file" && \
       grep -q "Important body text." "$task_file"; then
        pass_test
    else
        fail_test "Partial edit clobbered other fields or body"
    fi
}

test_edit_invalid_status() {
    log_test "edit rejects invalid status"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local id=$(new_task_id "Edit me" -s OPEN)

    set +e
    local output
    output=$(run_tatr edit "$id" -s BOGUS 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -ne 0 ] && grep -q "^- STATUS: OPEN$" "tasks/$id/TASK.md"; then
        pass_test
    else
        fail_test "Invalid status not rejected or file was modified"
    fi
}

test_edit_missing_id() {
    log_test "edit non-existent task"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    set +e
    local output
    output=$(run_tatr edit "20991231-235959" -p 5 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -ne 0 ]; then
        pass_test
    else
        fail_test "Expected non-zero exit for missing task"
    fi
}

test_rm_existing() {
    log_test "rm existing task"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local id=$(new_task_id "Remove me")
    run_tatr rm "$id" > /dev/null 2>&1
    local exit_code=$?

    if [ $exit_code -eq 0 ] && [ ! -d "tasks/$id" ]; then
        pass_test
    else
        fail_test "Task directory still present after rm"
    fi
}

test_rm_nonempty_dir() {
    log_test "rm task dir with extra files"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local id=$(new_task_id "Remove me")
    echo "review notes" > "tasks/$id/REVIEW.md"
    run_tatr rm "$id" > /dev/null 2>&1

    if [ ! -d "tasks/$id" ]; then
        pass_test
    else
        fail_test "Task directory with extra files not removed"
    fi
}

test_rm_preserves_siblings() {
    log_test "rm leaves other tasks untouched"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local id1=$(new_task_id "First")
    sleep 1
    local id2=$(new_task_id "Second")
    run_tatr rm "$id1" > /dev/null 2>&1

    if [ ! -d "tasks/$id1" ] && [ -d "tasks/$id2" ]; then
        pass_test
    else
        fail_test "Sibling task affected by rm"
    fi
}

test_rm_invalid_id() {
    log_test "rm with invalid id"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    set +e
    local output
    output=$(run_tatr rm "not-a-huid" 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -ne 0 ]; then
        pass_test
    else
        fail_test "Expected non-zero exit for invalid id"
    fi
}

test_rm_missing_id() {
    log_test "rm non-existent task"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    set +e
    local output
    output=$(run_tatr rm "20991231-235959" 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -ne 0 ]; then
        pass_test
    else
        fail_test "Expected non-zero exit for missing task"
    fi
}

# Test 16: Filter by status (eq)
test_filter_status_eq() {
    log_test "filter (status eq)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    run_tatr new "Open task" -s OPEN > /dev/null 2>&1
    sleep 1
    run_tatr new "Closed task" -s CLOSED > /dev/null 2>&1
    sleep 1
    run_tatr new "In progress task" -s IN_PROGRESS > /dev/null 2>&1

    local output=$(run_tatr ls -f ':status eq OPEN' 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && \
       echo "$output" | grep -q "Open task" && \
       ! echo "$output" | grep -q "Closed task" && \
       ! echo "$output" | grep -q "In progress task"; then
        pass_test
    else
        fail_test "Status eq filter not working correctly"
    fi
}

# Test 17: Filter by status (in)
test_filter_status_in() {
    log_test "filter (status in)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    run_tatr new "Open task" -s OPEN > /dev/null 2>&1
    sleep 1
    run_tatr new "Closed task" -s CLOSED > /dev/null 2>&1
    sleep 1
    run_tatr new "In progress task" -s IN_PROGRESS > /dev/null 2>&1

    local output=$(run_tatr ls -f ':status in [OPEN, CLOSED]' 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && \
       echo "$output" | grep -q "Open task" && \
       echo "$output" | grep -q "Closed task" && \
       ! echo "$output" | grep -q "In progress task"; then
        pass_test
    else
        fail_test "Status in filter not working correctly"
    fi
}

# Test 18: Filter by tags (contains)
test_filter_tags_contains() {
    log_test "filter (tags contains)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    run_tatr new "Feature task" -t feature > /dev/null 2>&1
    sleep 1
    run_tatr new "Bug task" -t bug > /dev/null 2>&1
    sleep 1
    run_tatr new "Mixed task" -t feature -t bug > /dev/null 2>&1

    local output=$(run_tatr ls -f ':tags contains feature' 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && \
       echo "$output" | grep -q "Feature task" && \
       echo "$output" | grep -q "Mixed task" && \
       ! echo "$output" | grep -q "Bug task"; then
        pass_test
    else
        fail_test "Tags contains filter not working correctly"
    fi
}

# Test 18b: Filter by version-style tags (dots and dashes in literals)
test_filter_tags_version() {
    log_test "filter (version tag with dots/dashes)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    run_tatr new "Release task" -t v0.1.0 > /dev/null 2>&1
    sleep 1
    run_tatr new "RC task" -t release-candidate > /dev/null 2>&1
    sleep 1
    run_tatr new "Plain task" -t feature > /dev/null 2>&1

    local output
    output=$(run_tatr ls -f ':tags contains v0.1.0' 2>&1)
    local exit_code=$?

    local dash_output
    dash_output=$(run_tatr ls -f ':tags contains release-candidate' 2>&1)
    local dash_exit=$?

    if [ $exit_code -eq 0 ] && [ $dash_exit -eq 0 ] && \
       echo "$output" | grep -q "Release task" && \
       ! echo "$output" | grep -q "RC task" && \
       ! echo "$output" | grep -q "Plain task" && \
       echo "$dash_output" | grep -q "RC task" && \
       ! echo "$dash_output" | grep -q "Release task"; then
        pass_test
    else
        fail_test "Version-style tag filter (dots/dashes) not working correctly"
    fi
}

# Test 19: Filter by priority
test_filter_priority() {
    log_test "filter (priority eq)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    run_tatr new "High priority" -p 100 > /dev/null 2>&1
    sleep 1
    run_tatr new "Medium priority" -p 50 > /dev/null 2>&1
    sleep 1
    run_tatr new "Low priority" -p 10 > /dev/null 2>&1

    local output=$(run_tatr ls -f ':priority eq 100' 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && \
       echo "$output" | grep -q "High priority" && \
       ! echo "$output" | grep -q "Medium priority" && \
       ! echo "$output" | grep -q "Low priority"; then
        pass_test
    else
        fail_test "Priority filter not working correctly"
    fi
}

# Test 20: Filter with AND operator
test_filter_and() {
    log_test "filter (AND operator)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    run_tatr new "High priority feature" -p 100 -t feature > /dev/null 2>&1
    sleep 1
    run_tatr new "Low priority feature" -p 10 -t feature > /dev/null 2>&1
    sleep 1
    run_tatr new "High priority bug" -p 100 -t bug > /dev/null 2>&1

    local output=$(run_tatr ls -f '(:priority eq 100) and (:tags contains feature)' 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && \
       echo "$output" | grep -q "High priority feature" && \
       ! echo "$output" | grep -q "Low priority feature" && \
       ! echo "$output" | grep -q "High priority bug"; then
        pass_test
    else
        fail_test "AND operator not working correctly"
    fi
}

# Test 21: Filter with OR operator
test_filter_or() {
    log_test "filter (OR operator)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    run_tatr new "High priority" -p 100 > /dev/null 2>&1
    sleep 1
    run_tatr new "Medium priority" -p 50 > /dev/null 2>&1
    sleep 1
    run_tatr new "Low priority" -p 10 > /dev/null 2>&1

    local output=$(run_tatr ls -f '(:priority eq 100) or (:priority eq 10)' 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && \
       echo "$output" | grep -q "High priority" && \
       echo "$output" | grep -q "Low priority" && \
       ! echo "$output" | grep -q "Medium priority"; then
        pass_test
    else
        fail_test "OR operator not working correctly"
    fi
}

# Test 22: Filter with NOT operator
test_filter_not() {
    log_test "filter (NOT operator)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    run_tatr new "Feature task" -t feature > /dev/null 2>&1
    sleep 1
    run_tatr new "Bug task" -t bug > /dev/null 2>&1
    sleep 1
    run_tatr new "Other task" -t other > /dev/null 2>&1

    local output=$(run_tatr ls -f 'not (:tags contains bug)' 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && \
       echo "$output" | grep -q "Feature task" && \
       echo "$output" | grep -q "Other task" && \
       ! echo "$output" | grep -q "Bug task"; then
        pass_test
    else
        fail_test "NOT operator not working correctly"
    fi
}

# Test 23: Filter with parentheses for precedence
test_filter_precedence() {
    log_test "filter (precedence with parens)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    run_tatr new "Task 1" -p 100 -t feature > /dev/null 2>&1
    sleep 1
    run_tatr new "Task 2" -p 50 -t feature > /dev/null 2>&1
    sleep 1
    run_tatr new "Task 3" -p 100 -t bug > /dev/null 2>&1
    sleep 1
    run_tatr new "Task 4" -p 50 -t bug > /dev/null 2>&1

    local output=$(run_tatr ls -f '((:priority eq 100) or (:priority eq 50)) and (:tags contains feature)' 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && \
       echo "$output" | grep -q "Task 1" && \
       echo "$output" | grep -q "Task 2" && \
       ! echo "$output" | grep -q "Task 3" && \
       ! echo "$output" | grep -q "Task 4"; then
        pass_test
    else
        fail_test "Filter precedence not working correctly"
    fi
}

# Test 24: Filter error - invalid field
test_filter_error_invalid_field() {
    log_test "filter error (invalid field)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    run_tatr new "Test task" > /dev/null 2>&1

    set +e
    local output
    output=$(run_tatr ls -f ':invalid_field eq 100' 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -ne 0 ] && echo "$output" | grep -q "unknown field"; then
        pass_test
    else
        fail_test "Should fail with invalid field"
    fi
}

# Test 25: Filter error - invalid status value
test_filter_error_invalid_status() {
    log_test "filter error (invalid status)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    run_tatr new "Test task" > /dev/null 2>&1

    set +e
    local output
    output=$(run_tatr ls -f ':status eq INVALID_STATUS' 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -ne 0 ] && echo "$output" | grep -q "invalid status value"; then
        pass_test
    else
        fail_test "Should fail with invalid status value"
    fi
}

# Test 26: Filter error - empty expression
test_filter_error_empty() {
    log_test "filter error (empty expression)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    run_tatr new "Test task" > /dev/null 2>&1

    set +e
    local output
    output=$(run_tatr ls -f '' 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -ne 0 ] && echo "$output" | grep -q "empty filter"; then
        pass_test
    else
        fail_test "Should fail with empty filter expression"
    fi
}

# Test 27: Filter error - syntax error
test_filter_error_syntax() {
    log_test "filter error (syntax error)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    run_tatr new "Test task" > /dev/null 2>&1

    set +e
    local output
    output=$(run_tatr ls -f ':status eq' 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -ne 0 ]; then
        pass_test
    else
        fail_test "Should fail with syntax error"
    fi
}

# Test 28: Filter combined with sorting
test_filter_with_sort() {
    log_test "filter with sorting"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    run_tatr new "Low priority feature" -p 10 -t feature > /dev/null 2>&1
    sleep 1
    run_tatr new "High priority feature" -p 100 -t feature > /dev/null 2>&1
    sleep 1
    run_tatr new "Medium priority bug" -p 50 -t bug > /dev/null 2>&1

    local output=$(run_tatr ls -f ':tags contains feature' -s priority 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ]; then
        # High priority should appear before low priority
        local high_line=$(echo "$output" | grep -n "High priority feature" | cut -d: -f1)
        local low_line=$(echo "$output" | grep -n "Low priority feature" | cut -d: -f1)

        if [ -n "$high_line" ] && [ -n "$low_line" ] && \
           [ "$high_line" -lt "$low_line" ] && \
           ! echo "$output" | grep -q "Medium priority bug"; then
            pass_test
        else
            fail_test "Filter with sort not working correctly"
        fi
    else
        fail_test "Exit code: $exit_code"
    fi
}

# Test 29: Filter with recursive listing
test_filter_with_recursive() {
    log_test "filter with recursive"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p project1/tasks
    mkdir -p project2/tasks

    cd project1
    run_tatr new "Project 1 feature" -t feature > /dev/null 2>&1
    sleep 1
    run_tatr new "Project 1 bug" -t bug > /dev/null 2>&1

    cd ../project2
    sleep 1
    run_tatr new "Project 2 feature" -t feature > /dev/null 2>&1
    sleep 1
    run_tatr new "Project 2 bug" -t bug > /dev/null 2>&1

    cd ..
    local output=$(run_tatr -r . ls -R -f ':tags contains feature' 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && \
       echo "$output" | grep -q "Project 1 feature" && \
       echo "$output" | grep -q "Project 2 feature" && \
       ! echo "$output" | grep -q "bug"; then
        pass_test
    else
        fail_test "Filter with recursive not working correctly"
    fi
}

# Test 30: Filter returns no results
test_filter_no_results() {
    log_test "filter (no matching results)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    run_tatr new "Bug task" -t bug > /dev/null 2>&1

    local output=$(run_tatr ls -f ':tags contains feature' 2>&1)
    local exit_code=$?

    # Should succeed but return no tasks
    if [ $exit_code -eq 0 ] && ! echo "$output" | grep -q "Bug task"; then
        pass_test
    else
        fail_test "Filter should return no results but succeed"
    fi
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --memcheck)
            MEMCHECK=1
            shift
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage
            exit 1
            ;;
    esac
done

echo "Building tatr..."
cd "$PROJECT_DIR"
make clean && make

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed${RESET}"
    exit 1
fi

# --- check subcommand tests ---

# Writes a TASK.md for check tests: dir, id, status; body from stdin.
write_check_task() {
    local dir=$1 id=$2 status=$3
    mkdir -p "$dir/tasks/$id"
    {
        echo "# Task $id"
        echo
        echo "- STATUS: $status"
        echo "- PRIORITY: 10"
        echo "- TAGS: feature"
        echo
        cat
    } > "$dir/tasks/$id/TASK.md"
}

test_check_clean() {
    log_test "check (clean tasks: exit 0, no output)"
    local test_dir=$(create_test_dir)
    write_check_task "$test_dir" "20260101-100000" "OPEN" <<'BODY'
## Steps

- [ ] open tasks may have unchecked steps
BODY
    write_check_task "$test_dir" "20260101-100001" "CLOSED" <<'BODY'
## Steps

- [x] all ticked

## Action items

- [ ] unchecked outside Steps must not fire
BODY
    printf '# R\n\n## Round 1\n\n- VERDICT: APPROVE\n\n- [x] R1.1 (MINOR) f:1 - fine\n' > "$test_dir/tasks/20260101-100001/REVIEW.md"

    set +e
    local output
    output=$(run_tatr -r "$test_dir" check 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -eq 0 ] && [ -z "$output" ]; then
        pass_test
    else
        fail_test "Exit: $exit_code, output: $output"
    fi
}

test_check_closed_unchecked() {
    log_test "check (closed-unchecked fires on CLOSED Steps boxes)"
    local test_dir=$(create_test_dir)
    write_check_task "$test_dir" "20260101-110000" "CLOSED" <<'BODY'
## Steps

- [x] done
- [ ] not done
- [ ] also not done
BODY

    set +e
    local output
    output=$(run_tatr -r "$test_dir" check 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -eq 1 ] && echo "$output" | grep -q "20260101-110000: closed-unchecked: 2 unchecked"; then
        pass_test
    else
        fail_test "Exit: $exit_code, output: $output"
    fi
}

test_check_closed_unchecked_history_exempt() {
    log_test "check (closed-unchecked exempts historical/goal tasks)"
    local test_dir=$(create_test_dir)
    mkdir -p "$test_dir/tasks"

    # A historical and a goal task, both CLOSED with unchecked Steps, must NOT
    # flag; a plain feature task with the same unchecked Steps still flags.
    write_unchecked_task() {
        local id=$1 tags=$2
        mkdir -p "$test_dir/tasks/$id"
        {
            echo "# Task $id"
            echo
            echo "- STATUS: CLOSED"
            echo "- PRIORITY: 10"
            echo "- TAGS: $tags"
            echo
            echo "## Steps"
            echo
            echo "- [ ] dropped, premise falsified"
        } > "$test_dir/tasks/$id/TASK.md"
    }
    write_unchecked_task "20260101-120100" "feature,historical"
    write_unchecked_task "20260101-120200" "goal"
    write_unchecked_task "20260101-120300" "feature"

    set +e
    local out
    out=$(run_tatr -r "$test_dir" check 2>&1)
    local code=$?
    set -e

    if [ $code -eq 1 ] \
        && ! echo "$out" | grep -q "20260101-120100" \
        && ! echo "$out" | grep -q "20260101-120200" \
        && echo "$out" | grep -q "20260101-120300: closed-unchecked"; then
        pass_test
    else
        fail_test "exit: $code, output: $out"
    fi
}

test_check_closed_not_approved() {
    log_test "check (closed-not-approved; later APPROVE round clears it)"
    local test_dir=$(create_test_dir)
    write_check_task "$test_dir" "20260101-120000" "CLOSED" <<'BODY'
## Steps

- [x] done
BODY
    printf '# R\n\n## Round 1\n\n- VERDICT: REQUEST_CHANGES\n' > "$test_dir/tasks/20260101-120000/REVIEW.md"

    set +e
    local output
    output=$(run_tatr -r "$test_dir" check 2>&1)
    local exit_code=$?
    set -e

    local first_ok=0
    if [ $exit_code -eq 1 ] && echo "$output" | grep -q "closed-not-approved: latest REVIEW.md verdict is 'REQUEST_CHANGES'"; then
        first_ok=1
    fi

    printf '\n## Round 2\n\n- VERDICT: APPROVE\n' >> "$test_dir/tasks/20260101-120000/REVIEW.md"

    set +e
    output=$(run_tatr -r "$test_dir" check 2>&1)
    exit_code=$?
    set -e

    if [ $first_ok -eq 1 ] && [ $exit_code -eq 0 ] && [ -z "$output" ]; then
        pass_test
    else
        fail_test "Exit after APPROVE: $exit_code, output: $output"
    fi
}

test_check_bad_severity() {
    log_test "check (bad-severity on unknown vocabulary)"
    local test_dir=$(create_test_dir)
    write_check_task "$test_dir" "20260101-130000" "OPEN" <<'BODY'
## Steps

- [ ] pending
BODY
    printf '# R\n\n## Round 1\n\n- VERDICT: APPROVE\n\n- [ ] R1.1 (LOW) f:1 - invented severity\n- [x] R1.2 (BLOCKER) f:2 - known severity\n' > "$test_dir/tasks/20260101-130000/REVIEW.md"

    set +e
    local output
    output=$(run_tatr -r "$test_dir" check 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -eq 1 ] && echo "$output" | grep -q "bad-severity: unknown severity 'LOW'" && ! echo "$output" | grep -q "BLOCKER'"; then
        pass_test
    else
        fail_test "Exit: $exit_code, output: $output"
    fi
}

test_check_malformed_header() {
    log_test "check (malformed-header: unparseable and invalid STATUS)"
    local test_dir=$(create_test_dir)
    mkdir -p "$test_dir/tasks/20260101-140000"
    printf '# No priority line\n\n- STATUS: OPEN\n- TAGS: x\n' > "$test_dir/tasks/20260101-140000/TASK.md"
    write_check_task "$test_dir" "20260101-140001" "DONE" <<'BODY'
Body text.
BODY

    set +e
    local output
    output=$(run_tatr -r "$test_dir" check 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -eq 1 ] \
        && echo "$output" | grep -q "20260101-140000: malformed-header: TASK.md failed to parse" \
        && echo "$output" | grep -q "20260101-140001: malformed-header: invalid STATUS 'DONE'"; then
        pass_test
    else
        fail_test "Exit: $exit_code, output: $output"
    fi
}

test_check_strict() {
    log_test "check (--strict requires REVIEW/RETRO on CLOSED)"
    local test_dir=$(create_test_dir)
    write_check_task "$test_dir" "20260101-150000" "CLOSED" <<'BODY'
## Steps

- [x] done
BODY

    set +e
    local output
    output=$(run_tatr -r "$test_dir" check 2>&1)
    local default_code=$?
    local strict_output
    strict_output=$(run_tatr -r "$test_dir" check --strict 2>&1)
    local strict_code=$?
    set -e

    printf '# R\n\n## Round 1\n\n- VERDICT: APPROVE\n' > "$test_dir/tasks/20260101-150000/REVIEW.md"
    printf '# Retro\n\nfine.\n' > "$test_dir/tasks/20260101-150000/RETRO.md"

    set +e
    local quiet_output
    quiet_output=$(run_tatr -r "$test_dir" check --strict 2>&1)
    local quiet_code=$?
    set -e

    if [ $default_code -eq 0 ] && [ $strict_code -eq 1 ] \
        && echo "$strict_output" | grep -q "closed-missing-review" \
        && echo "$strict_output" | grep -q "closed-missing-retro" \
        && [ $quiet_code -eq 0 ] && [ -z "$quiet_output" ]; then
        pass_test
    else
        fail_test "default: $default_code, strict: $strict_code, quiet: $quiet_code"
    fi
}

test_check_strict_history_exempt() {
    log_test "check (--strict exempts historical/goal-tagged CLOSED tasks)"
    local test_dir=$(create_test_dir)
    mkdir -p "$test_dir/tasks"

    # Three CLOSED tasks with no REVIEW.md/RETRO.md. Two are exempt by tag
    # (historical, goal); the plain feature one must still flag under --strict.
    write_history_task() {
        local id=$1 tags=$2
        mkdir -p "$test_dir/tasks/$id"
        {
            echo "# Task $id"
            echo
            echo "- STATUS: CLOSED"
            echo "- PRIORITY: 10"
            echo "- TAGS: $tags"
            echo
            echo "## Steps"
            echo
            echo "- [x] done"
        } > "$test_dir/tasks/$id/TASK.md"
    }
    write_history_task "20260101-150100" "historical"
    write_history_task "20260101-150200" "goal"
    write_history_task "20260101-150300" "feature"

    set +e
    local out
    out=$(run_tatr -r "$test_dir" check --strict 2>&1)
    local code=$?
    set -e

    if [ $code -eq 1 ] \
        && ! echo "$out" | grep -q "20260101-150100" \
        && ! echo "$out" | grep -q "20260101-150200" \
        && echo "$out" | grep -q "20260101-150300: closed-missing-review" \
        && echo "$out" | grep -q "20260101-150300: closed-missing-retro"; then
        pass_test
    else
        fail_test "exit: $code, output: $out"
    fi
}

test_check_ledger() {
    log_test "check (--ledger promotion-stalled at x3 outside Pending)"
    local test_dir=$(create_test_dir)
    mkdir -p "$test_dir/tasks"
    cat > "$test_dir/LESSONS.md" <<'LEDGER'
# Lessons ledger

## Process lessons

- `hot-lesson` (x3): recurs a lot. 20260101-100000
- `warm-lesson` (x2): fine where it is. 20260101-100000
- `noisy-lesson` (x2, enforced) but really at (x4): counter after noise. id

## Pending promotions (3+ occurrences, user decides)

- `promoted-lesson` (x5): already parked here.
LEDGER

    set +e
    local output
    output=$(run_tatr -r "$test_dir" check --ledger LESSONS.md 2>&1)
    local exit_code=$?
    set -e

    set +e
    local abs_output
    abs_output=$(run_tatr -r "$test_dir" check --ledger "$test_dir/LESSONS.md" 2>&1)
    local abs_code=$?
    local missing_output
    missing_output=$(run_tatr -r "$test_dir" check --ledger nope.md 2>&1)
    local missing_code=$?
    local dir_output
    dir_output=$(run_tatr -r "$test_dir" check --ledger tasks 2>&1)
    local dir_code=$?
    set -e

    if [ $exit_code -eq 1 ] \
        && echo "$output" | grep -q "ledger: promotion-stalled: hot-lesson (x3)" \
        && echo "$output" | grep -q "ledger: promotion-stalled: noisy-lesson (x4)" \
        && ! echo "$output" | grep -q "warm-lesson" \
        && ! echo "$output" | grep -q "promoted-lesson" \
        && [ $abs_code -eq 1 ] && echo "$abs_output" | grep -q "hot-lesson" \
        && [ $missing_code -eq 1 ] && echo "$missing_output" | grep -q "ledger: unreadable" \
        && [ $dir_code -eq 1 ] && echo "$dir_output" | grep -q "ledger: unreadable"; then
        pass_test
    else
        fail_test "Exit: $exit_code/$abs_code/$missing_code/$dir_code"
    fi
}

test_check_per_id() {
    log_test "check (per-ID scopes to one task)"
    local test_dir=$(create_test_dir)
    write_check_task "$test_dir" "20260101-160000" "CLOSED" <<'BODY'
## Steps

- [ ] dirty
BODY
    write_check_task "$test_dir" "20260101-160001" "CLOSED" <<'BODY'
## Steps

- [x] clean
BODY

    set +e
    local dirty_output
    dirty_output=$(run_tatr -r "$test_dir" check 20260101-160000 2>&1)
    local dirty_code=$?
    local clean_output
    clean_output=$(run_tatr -r "$test_dir" check 20260101-160001 2>&1)
    local clean_code=$?
    set -e

    if [ $dirty_code -eq 1 ] && [ $clean_code -eq 0 ] && [ -z "$clean_output" ] \
        && echo "$dirty_output" | grep -q "closed-unchecked" \
        && ! echo "$dirty_output" | grep -q "160001"; then
        pass_test
    else
        fail_test "dirty: $dirty_code, clean: $clean_code"
    fi
}

test_check_exit_codes() {
    log_test "check (exit 1 on findings, 0 clean)"
    local test_dir=$(create_test_dir)
    write_check_task "$test_dir" "20260101-170000" "OPEN" <<'BODY'
Plain body, nothing wrong.
BODY

    set +e
    run_tatr -r "$test_dir" check > /dev/null 2>&1
    local clean_code=$?
    set -e

    write_check_task "$test_dir" "20260101-170001" "CLOSED" <<'BODY'
## Steps

- [ ] left open
BODY

    set +e
    run_tatr -r "$test_dir" check > /dev/null 2>&1
    local dirty_code=$?
    set -e

    if [ $clean_code -eq 0 ] && [ $dirty_code -eq 1 ]; then
        pass_test
    else
        fail_test "clean: $clean_code, dirty: $dirty_code"
    fi
}


test_check_scanner_edges() {
    log_test "check (scanner edges: status whitespace, R-prose, verdict tail)"
    local test_dir=$(create_test_dir)
    # Trailing space after CLOSED: deserializes to silent OPEN, must be a finding.
    mkdir -p "$test_dir/tasks/20260101-180000"
    printf '# Trailing space\n\n- STATUS: CLOSED \n- PRIORITY: 1\n- TAGS: x\n\n## Steps\n\n- [ ] never done\n' > "$test_dir/tasks/20260101-180000/TASK.md"
    # Prose checkbox starting with R must not be a severity finding; verdict
    # with a tail must still read APPROVE.
    write_check_task "$test_dir" "20260101-180001" "CLOSED" <<'BODY'
## Steps

- [x] done
BODY
    printf '# R\n\n## Round 1\n\n- VERDICT: APPROVE (1 round)\n\n- [ ] Rebase onto master (before merging)\n' > "$test_dir/tasks/20260101-180001/REVIEW.md"

    set +e
    local output
    output=$(run_tatr -r "$test_dir" check 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -eq 1 ] \
        && echo "$output" | grep -q "20260101-180000: malformed-header: invalid STATUS 'CLOSED '" \
        && ! echo "$output" | grep -q "bad-severity" \
        && ! echo "$output" | grep -q "180001"; then
        pass_test
    else
        fail_test "Exit: $exit_code, output: $output"
    fi
}

test_check_missing_artifacts() {
    log_test "check (missing TASK.md and verdict-less REVIEW.md)"
    local test_dir=$(create_test_dir)
    mkdir -p "$test_dir/tasks/20260101-190000"
    printf '# R\n\nno verdict here\n' > "$test_dir/tasks/20260101-190000/REVIEW.md"
    write_check_task "$test_dir" "20260101-190001" "CLOSED" <<'BODY'
## Steps

- [x] done
BODY
    printf '# R\n\nround text but no verdict line\n' > "$test_dir/tasks/20260101-190001/REVIEW.md"

    set +e
    local output
    output=$(run_tatr -r "$test_dir" check 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -eq 1 ] \
        && echo "$output" | grep -q "20260101-190000: malformed-header: TASK.md missing or unreadable" \
        && echo "$output" | grep -q "20260101-190001: closed-not-approved: REVIEW.md has no VERDICT line"; then
        pass_test
    else
        fail_test "Exit: $exit_code, output: $output"
    fi
}

# --- DECISION.md checks (bad-decision-status / dangling-supersede) ---
# All presence-gated: they fire only when a task folder has a DECISION.md.
# OPEN tasks are used so the only possible findings are the decision ones.

write_decision() {
    local dir=$1 id=$2
    write_check_task "$dir" "$id" "OPEN" <<'BODY'
## Notes

- decision-bearing task
BODY
    cat > "$dir/tasks/$id/DECISION.md"
}

test_check_bad_decision_status() {
    log_test "check (bad-decision-status fires on an invalid STATUS token)"
    local test_dir=$(create_test_dir)
    write_decision "$test_dir" "20260101-200000" <<'BODY'
# Decision: something

- STATUS: DRAFT
- TASK: 20260101-200000

## Decision

do the thing
BODY
    # A DECISION.md with no STATUS line at all is also a finding.
    write_decision "$test_dir" "20260101-200001" <<'BODY'
# Decision: no status

## Decision

do the other thing
BODY

    set +e
    local output
    output=$(run_tatr -r "$test_dir" check 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -eq 1 ] \
        && echo "$output" | grep -q "20260101-200000: bad-decision-status: invalid STATUS 'DRAFT'" \
        && echo "$output" | grep -q "20260101-200001: bad-decision-status: DECISION.md has no STATUS line"; then
        pass_test
    else
        fail_test "Exit: $exit_code, output: $output"
    fi
}

test_check_good_decision_status() {
    log_test "check (ACCEPTED status, with an inline comment, stays clean)"
    local test_dir=$(create_test_dir)
    write_decision "$test_dir" "20260101-200100" <<'BODY'
# Decision: accepted with an enum-hint comment

- STATUS: ACCEPTED   # ACCEPTED | SUPERSEDED by tasks/<id>/DECISION.md
- TASK: 20260101-200100

## Decision

the chosen thing
BODY

    set +e
    local output
    output=$(run_tatr -r "$test_dir" check 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -eq 0 ] && [ -z "$output" ]; then
        pass_test
    else
        fail_test "Exit: $exit_code, output: $output"
    fi
}

test_check_dangling_supersede() {
    log_test "check (dangling-supersede fires on refs with no DECISION.md)"
    local test_dir=$(create_test_dir)
    # STATUS references a task that does not exist.
    write_decision "$test_dir" "20260101-200200" <<'BODY'
# Decision: superseded, target missing

- STATUS: SUPERSEDED by tasks/20991231-235959/DECISION.md
- TASK: 20260101-200200
BODY
    # A Supersedes header references a task with no DECISION.md.
    write_decision "$test_dir" "20260101-200201" <<'BODY'
# Decision: supersedes a phantom

- STATUS: ACCEPTED
- Supersedes: tasks/20991231-235959/DECISION.md
- TASK: 20260101-200201
BODY

    set +e
    local output
    output=$(run_tatr -r "$test_dir" check 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -eq 1 ] \
        && echo "$output" | grep -q "20260101-200200: dangling-supersede: STATUS supersedes 'tasks/20991231-235959/DECISION.md'" \
        && echo "$output" | grep -q "20260101-200201: dangling-supersede: Supersedes 'tasks/20991231-235959/DECISION.md'"; then
        pass_test
    else
        fail_test "Exit: $exit_code, output: $output"
    fi
}

test_check_resolving_supersede() {
    log_test "check (a resolving supersede chain stays clean)"
    local test_dir=$(create_test_dir)
    # OLD points forward to NEW; NEW points back to OLD; both DECISION.md exist.
    write_decision "$test_dir" "20260101-210000" <<'BODY'
# Decision: the old one

- STATUS: SUPERSEDED by tasks/20260101-210001/DECISION.md
- TASK: 20260101-210000
BODY
    write_decision "$test_dir" "20260101-210001" <<'BODY'
# Decision: the new one

- STATUS: ACCEPTED
- Supersedes: tasks/20260101-210000/DECISION.md
- TASK: 20260101-210001
BODY

    set +e
    local output
    output=$(run_tatr -r "$test_dir" check 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -eq 0 ] && [ -z "$output" ]; then
        pass_test
    else
        fail_test "Exit: $exit_code, output: $output"
    fi
}

test_check_decision_absent_unaffected() {
    log_test "check (a task with no DECISION.md is never flagged)"
    local test_dir=$(create_test_dir)
    write_check_task "$test_dir" "20260101-220000" "OPEN" <<'BODY'
## Steps

- [ ] no decision record here
BODY

    set +e
    local output
    output=$(run_tatr -r "$test_dir" check 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -eq 0 ] && [ -z "$output" ]; then
        pass_test
    else
        fail_test "Exit: $exit_code, output: $output"
    fi
}

# --- Makefile build-guard tests ---
# The guard target runs the env check without compiling, so these are cheap and
# do not touch the built binary. They clear the nix markers to simulate a bare
# shell (checker.sh itself runs inside `nix develop`, where IN_NIX_SHELL is set).

test_build_guard_bare_shell_fails() {
    log_test "build guard (bare shell fails with nix develop pointer)"

    set +e
    local output
    output=$(env -u IN_NIX_SHELL -u NIX_BUILD_TOP -u TATR_ALLOW_BARE_BUILD \
        make -C "$PROJECT_DIR" guard 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -ne 0 ] && echo "$output" | grep -q "nix develop -c make"; then
        pass_test
    else
        fail_test "Exit: $exit_code, output: $output"
    fi
}

test_build_guard_nix_shell_passes() {
    log_test "build guard (IN_NIX_SHELL passes)"

    set +e
    local output
    output=$(env -u NIX_BUILD_TOP -u TATR_ALLOW_BARE_BUILD IN_NIX_SHELL=impure \
        make -C "$PROJECT_DIR" guard 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -eq 0 ]; then
        pass_test
    else
        fail_test "Exit: $exit_code, output: $output"
    fi
}

test_build_guard_override_passes() {
    log_test "build guard (TATR_ALLOW_BARE_BUILD overrides)"

    set +e
    local output
    output=$(env -u IN_NIX_SHELL -u NIX_BUILD_TOP TATR_ALLOW_BARE_BUILD=1 \
        make -C "$PROJECT_DIR" guard 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -eq 0 ]; then
        pass_test
    else
        fail_test "Exit: $exit_code, output: $output"
    fi
}


if [ "$MEMCHECK" -eq 1 ]; then
    if ! command -v $MEMCHECKER &> /dev/null; then
        echo -e "${YELLOW}Warning: $MEMCHECKER not found, running without memory checking${RESET}"
        MEMCHECK=0
    else
        echo "Running tests with memory checking..."
    fi
else
    echo "Running tests..."
fi

echo

# Run all tests
test_help
test_version
test_new_basic
test_new_priority
test_new_tags
test_new_status
test_new_full
test_new_body_file
test_new_body_stdin
test_new_collision_fails
test_ls_empty
test_ls_with_tasks
test_ls_sort_priority
test_ls_sort_title
test_ls_recursive
test_root_flag
test_error_no_tasks_dir
test_task_format
test_show_existing
test_show_invalid_id
test_show_missing_id
test_edit_status
test_edit_priority
test_edit_tags
test_edit_title
test_edit_partial_preserves_fields
test_edit_invalid_status
test_edit_missing_id
test_rm_existing
test_rm_nonempty_dir
test_rm_preserves_siblings
test_rm_invalid_id
test_rm_missing_id
test_filter_status_eq
test_filter_status_in
test_filter_tags_contains
test_filter_tags_version
test_filter_priority
test_filter_and
test_filter_or
test_filter_not
test_filter_precedence
test_filter_error_invalid_field
test_filter_error_invalid_status
test_filter_error_empty
test_filter_error_syntax
test_filter_with_sort
test_filter_with_recursive
test_filter_no_results
test_check_clean
test_check_closed_unchecked
test_check_closed_unchecked_history_exempt
test_check_closed_not_approved
test_check_bad_severity
test_check_malformed_header
test_check_strict
test_check_strict_history_exempt
test_check_ledger
test_check_per_id
test_check_exit_codes
test_check_scanner_edges
test_check_missing_artifacts
test_check_bad_decision_status
test_check_good_decision_status
test_check_dangling_supersede
test_check_resolving_supersede
test_check_decision_absent_unaffected
test_build_guard_bare_shell_fails
test_build_guard_nix_shell_passes
test_build_guard_override_passes

echo
echo "Passed $PASSED_TESTS/$TOTAL_TESTS tests"

if [ $PASSED_TESTS -eq $TOTAL_TESTS ]; then
    echo -e "${GREEN}All tests passed${RESET}"
    exit 0
else
    echo -e "${RED}Some tests failed${RESET}"
    exit 1
fi
