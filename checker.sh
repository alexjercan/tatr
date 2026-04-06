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

echo
echo "Passed $PASSED_TESTS/$TOTAL_TESTS tests"

if [ $PASSED_TESTS -eq $TOTAL_TESTS ]; then
    echo -e "${GREEN}All tests passed${RESET}"
    exit 0
else
    echo -e "${RED}Some tests failed${RESET}"
    exit 1
fi
