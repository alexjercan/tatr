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
test_ls_empty
test_ls_with_tasks
test_ls_sort_priority
test_ls_sort_title
test_ls_recursive
test_root_flag
test_error_no_tasks_dir
test_task_format
test_filter_status_eq
test_filter_status_in
test_filter_tags_contains
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

echo
echo "Passed $PASSED_TESTS/$TOTAL_TESTS tests"

if [ $PASSED_TESTS -eq $TOTAL_TESTS ]; then
    echo -e "${GREEN}All tests passed${RESET}"
    exit 0
else
    echo -e "${RED}Some tests failed${RESET}"
    exit 1
fi
