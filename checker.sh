#!/usr/bin/env bash

set -e

MEMCHECKER=valgrind
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TATR_BIN="$PROJECT_DIR/dist/tatr"
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
# A task is always born at the bottom of the lifecycle. The workflow fields
# are not settable at creation: every state above BACKLOG is reached by
# walking `tatr flow`, which is what makes the guards unavoidable.
test_new_is_born_backlog() {
    log_test "new task (born BACKLOG/DRAFT/OPEN)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local output=$(run_tatr new "Fresh task" 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ]; then
        local task_file=$(find tasks -name "TASK.md" | head -1)
        if [ -f "$task_file" ] \
            && grep -q "^- STATUS: OPEN$" "$task_file" \
            && grep -q "^- FLOW STEP: BACKLOG$" "$task_file" \
            && grep -q "^- PLAN STATUS: DRAFT$" "$task_file"; then
            pass_test
        else
            fail_test "Fresh task not born BACKLOG/DRAFT/OPEN: $(cat "$task_file")"
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

    local output=$(run_tatr new "Full task" -p 50 -t test -t complete 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ]; then
        local task_file=$(find tasks -name "TASK.md" | head -1)
        if [ -f "$task_file" ] && \
           grep -q "PRIORITY: 50" "$task_file" && \
           grep -q "TAGS: test, complete" "$task_file" && \
           grep -q "STATUS: OPEN" "$task_file"; then
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

    run_tatr new "Format test task" -p 42 -t tag1 -t tag2 > /dev/null 2>&1

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

# Helper: echo a task's current flow step.
flow_step_of() {
    sed -n 's/^- FLOW STEP: //p' "tasks/$1/TASK.md" | head -1
}

# Helper: walk a task up to <step> through `tatr flow`, scaffolding whatever
# each gate demands on the way. Nothing seeds a workflow field any more, so a
# fixture that wants a task in a given state has to earn it - which means the
# suite exercises the guards on the way to every fixture.
#
# EPIC containers are exempt from the review, retro and Steps requirements, so
# nothing is scaffolded for them. Dependencies are NOT handled here: a blocker
# must already be CLOSED before its dependant can reach WORKING, so drive the
# blocker first. Returns non-zero when the walk is refused.
drive_task_to() {
    local id=$1
    local target=$2
    local dir="tasks/$id"
    local step

    while true; do
        step=$(flow_step_of "$id")
        if [ "$step" = "$target" ]; then
            return 0
        fi
        if ! grep -q '^- KIND: EPIC' "$dir/TASK.md"; then
            case "$step" in
                REVIEWING)
                    [ -f "$dir/REVIEW.md" ] || printf -- '- VERDICT: APPROVE\n' > "$dir/REVIEW.md"
                    ;;
                COMPOUNDING)
                    [ -f "$dir/RETRO.md" ] || printf '# Retro\n' > "$dir/RETRO.md"
                    # Ticks every "- [ ]": the close gate counts the ones under
                    # "## Steps", and a fixture body carries no others.
                    sed -i 's/^- \[ \]/- [x]/' "$dir/TASK.md"
                    ;;
            esac
        fi
        if ! run_tatr flow "$id" > /dev/null 2>&1; then
            return 1
        fi
    done
}

test_show_existing() {
    log_test "show existing task"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local id=$(new_task_id "Show me" -p 55 -t alpha -t beta)
    drive_task_to "$id" WORKING
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

    local id=$(new_task_id "Keep me" -p 42 -t keep)
    local task_file="tasks/$id/TASK.md"
    printf '\nImportant body text.\n' >> "$task_file"
    drive_task_to "$id" WORKING

    run_tatr edit "$id" -T "Kept me" > /dev/null 2>&1

    # The workflow fields belong to `tatr flow`, so an edit must carry them
    # through untouched rather than resetting them to their born values.
    if grep -q "^- STATUS: IN_PROGRESS$" "$task_file" && \
       grep -q "^- FLOW STEP: WORKING$" "$task_file" && \
       grep -q "^- PLAN STATUS: APPROVED$" "$task_file" && \
       grep -q "^- PRIORITY: 42$" "$task_file" && \
       grep -q "^- TAGS: keep$" "$task_file" && \
       grep -q "^# Kept me$" "$task_file" && \
       grep -q "Important body text." "$task_file"; then
        pass_test
    else
        fail_test "Partial edit clobbered other fields or body"
    fi
}

# STATUS, FLOW STEP and PLAN STATUS are unsettable through `new` and `edit`:
# the bypass is gone by removal, not by shared validation. The retired
# spellings fail with a pointer to `tatr flow` rather than argparse's generic
# unknown-argument message, so the rejection is actionable.
test_edit_status_uses_transition_guards() {
    log_test "new/edit refuse the retired workflow flags"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local id=$(new_task_id "Guard me")
    cp "tasks/$id/TASK.md" before.md

    set +e
    local out_s out_f out_plan out_long out_new
    out_s=$(run_tatr edit "$id" -s CLOSED 2>&1); local code_s=$?
    out_f=$(run_tatr edit "$id" -f WORKING 2>&1); local code_f=$?
    out_plan=$(run_tatr edit "$id" -S APPROVED 2>&1); local code_plan=$?
    out_long=$(run_tatr edit "$id" --status CLOSED 2>&1); local code_long=$?
    out_new=$(run_tatr new "Seeded closed" -s CLOSED 2>&1); local code_new=$?
    set -e

    if [ $code_s -ne 0 ] && [ $code_f -ne 0 ] && [ $code_plan -ne 0 ] \
        && [ $code_long -ne 0 ] && [ $code_new -ne 0 ] \
        && echo "$out_s" | grep -q 'tatr flow' \
        && echo "$out_f" | grep -q 'tatr flow' \
        && echo "$out_plan" | grep -q 'tatr flow' \
        && echo "$out_long" | grep -q 'tatr flow' \
        && echo "$out_new" | grep -q 'tatr flow' \
        && [ "$(find tasks -name 'TASK.md' | wc -l)" -eq 1 ] \
        && cmp -s before.md "tasks/$id/TASK.md"; then
        pass_test
    else
        fail_test "s($code_s): $out_s | f($code_f): $out_f | S($code_plan): $out_plan | new($code_new): $out_new"
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

# Corrupts one metadata line of $task_file, then proves `edit` refuses it, says
# why, and writes nothing. Sets bad_ok=0 and appends to bad_detail on failure.
# Callers set task_file / pristine.md / bad_ok / bad_detail first.
reject_bad_value() {
    local field=$1 value=$2 expected=$3
    cp pristine.md "$task_file"
    sed -i "s/^- $field: .*/- $field: $value/" "$task_file"
    cp "$task_file" tampered.md

    set +e
    local output
    output=$(run_tatr edit "$id" -T "Renamed" 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -eq 0 ] \
        || ! echo "$output" | grep -q "invalid $field '$value'" \
        || ! echo "$output" | grep -q "$expected" \
        || ! cmp -s tampered.md "$task_file"; then
        bad_ok=0
        bad_detail="$bad_detail; $field=$value (exit $exit_code): $output"
    fi
}

# --- v2 schema tests ---
# The v2 record is one flat metadata block: STATUS, PRIORITY, TAGS, KIND,
# FLOW STEP, PLAN STATUS, then the optional PARENT and DEPENDS ON. There is no
# migration path: a record that does not carry the required fields is rejected.

test_v2_task_round_trip() {
    log_test "v2 (round trip preserves every field and the body)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    # The body starts with an uppercase bullet and mentions a field inline:
    # both are ordinary prose, and the whole body must survive byte for byte.
    # (A body whose first line is metadata-shaped used to be written by `new`
    # and then rejected by every later read.)
    printf -- '- NOTE: a bullet may open the body\n\n## Story\n\nMentions "- KIND: EPIC" in prose.\n\n## Steps\n\n- [ ] one\n' > body.md

    # The dependencies are real records driven to DONE: a task cannot reach
    # WORKING over an open blocker, so the round-trip fixture earns its state.
    local dep_one=$(new_task_id "Blocker one")
    sleep 1
    local dep_two=$(new_task_id "Blocker two")
    sleep 1
    drive_task_to "$dep_one" DONE
    drive_task_to "$dep_two" DONE

    local full_id=$(new_task_id "Every field" -p 42 -t alpha -t beta \
        -k STORY -P 20260101-000000 \
        -d "$dep_one" -d "$dep_two" -b body.md)
    drive_task_to "$full_id" WORKING
    sleep 1 # IDs have second resolution and `new` refuses a same-second collision
    local bare_id=$(new_task_id "No relationships" -b body.md)

    local full_file="tasks/$full_id/TASK.md"
    local bare_file="tasks/$bare_id/TASK.md"
    cp "$full_file" full_before.md
    cp "$bare_file" bare_before.md

    # A no-op edit reloads and rewrites through the same spine, so an identical
    # file proves the parse/serialize pair is lossless.
    run_tatr edit "$full_id" -T "Every field" > /dev/null 2>&1
    run_tatr edit "$bare_id" -T "No relationships" > /dev/null 2>&1

    if grep -q "^- STATUS: IN_PROGRESS$" "$full_file" \
        && grep -q "^- PRIORITY: 42$" "$full_file" \
        && grep -q "^- TAGS: alpha, beta$" "$full_file" \
        && grep -q "^- KIND: STORY$" "$full_file" \
        && grep -q "^- FLOW STEP: WORKING$" "$full_file" \
        && grep -q "^- PLAN STATUS: APPROVED$" "$full_file" \
        && grep -q "^- PARENT: 20260101-000000$" "$full_file" \
        && grep -q "^- DEPENDS ON: $dep_one, $dep_two$" "$full_file" \
        && cmp -s full_before.md "$full_file" \
        && grep -q "^- KIND: TASK$" "$bare_file" \
        && grep -q "^- FLOW STEP: BACKLOG$" "$bare_file" \
        && grep -q "^- PLAN STATUS: DRAFT$" "$bare_file" \
        && ! grep -q "^- PARENT:" "$bare_file" \
        && ! grep -q "^- DEPENDS ON:" "$bare_file" \
        && cmp -s bare_before.md "$bare_file" \
        && grep -q '^Mentions "- KIND: EPIC" in prose\.$' "$bare_file" \
        && grep -q '^- NOTE: a bullet may open the body$' "$bare_file" \
        && sed -n '/^- NOTE: a bullet may open the body$/,$p' "$full_file" | diff -q - body.md > /dev/null; then
        pass_test
    else
        fail_test "Round trip lost a field or rewrote the body: $(cat "$full_file")"
    fi
}

test_v2_never_writes_an_unreadable_record() {
    log_test "v2 (a record tatr writes always reads back)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    # A newline in the title would split the record across the metadata block.
    # `new` must refuse it and leave nothing behind, rather than reporting
    # success and stranding a file that no later command can read.
    set +e
    local output
    output=$(run_tatr new "$(printf 'one\ntwo')" 2>&1)
    local exit_code=$?
    set -e

    local dirs_after=$(ls tasks | wc -l)

    # The same guard on the edit path: an existing task stays readable.
    local id=$(new_task_id "Good title")
    set +e
    local edit_output
    edit_output=$(run_tatr edit "$id" -T "$(printf 'three\nfour')" 2>&1)
    local edit_code=$?
    set -e

    set +e
    run_tatr show "$id" > /dev/null 2>&1
    local show_code=$?
    set -e

    if [ $exit_code -ne 0 ] \
        && [ "$dirs_after" -eq 0 ] \
        && echo "$output" | grep -q "would not parse back" \
        && [ $edit_code -ne 0 ] \
        && [ $show_code -eq 0 ] \
        && grep -q "^# Good title$" "tasks/$id/TASK.md"; then
        pass_test
    else
        fail_test "new($exit_code, $dirs_after dirs): $output | edit($edit_code): $edit_output | show($show_code)"
    fi
}

test_v2_ls_skips_unreadable_records() {
    log_test "v2 (ls lists the readable tasks, names the rest, exits non-zero)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local id=$(new_task_id "Readable task")
    # A pre-v2 record next to it: hand correction is the only migration path,
    # so `ls` is how a user finds what still needs correcting. It must not
    # swallow the rest of the backlog, nor report success.
    mkdir -p tasks/20260101-090000
    printf '# Legacy\n\n- STATUS: OPEN\n- PRIORITY: 1\n- TAGS: x\n\nbody\n' > tasks/20260101-090000/TASK.md

    set +e
    local output
    output=$(run_tatr ls 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -ne 0 ] \
        && echo "$output" | grep -q "Readable task" \
        && echo "$output" | grep -q "Skipping unreadable task '20260101-090000'" \
        && echo "$output" | grep -q "1 task(s) could not be read"; then
        pass_test
    else
        fail_test "Exit: $exit_code, output: $output"
    fi
}

test_v2_rejects_legacy_record() {
    log_test "v2 (legacy v1 record is rejected, naming file and field)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks/20260101-090000

    # A pre-v2 record: title, STATUS, PRIORITY, TAGS, then straight to the body.
    printf '# Legacy\n\n- STATUS: OPEN\n- PRIORITY: 10\n- TAGS: feature\n\n## Steps\n\n- [ ] one\n' \
        > tasks/20260101-090000/TASK.md
    cp tasks/20260101-090000/TASK.md legacy_before.md

    set +e
    local output
    output=$(run_tatr show 20260101-090000 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -ne 0 ] \
        && echo "$output" | grep -q "tasks/20260101-090000/TASK.md" \
        && echo "$output" | grep -q "expected '- KIND: '" \
        && echo "$output" | grep -q "correct the record by hand" \
        && cmp -s legacy_before.md tasks/20260101-090000/TASK.md; then
        pass_test
    else
        fail_test "Exit: $exit_code, output: $output"
    fi
}

test_v2_rejects_invalid_metadata_atomically() {
    log_test "v2 (invalid metadata values leave the file untouched)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local id=$(new_task_id "Valid" -k TASK -P 20260101-000000 -d 20260102-000000)
    local task_file="tasks/$id/TASK.md"

    cp "$task_file" pristine.md

    bad_ok=1
    bad_detail=""

    reject_bad_value "STATUS" "DONE" "OPEN, IN_PROGRESS or CLOSED"
    reject_bad_value "KIND" "EPICS" "TASK, EPIC, STORY or SPIKE"
    reject_bad_value "FLOW STEP" "READY" "BACKLOG, UNDERSTANDING"
    reject_bad_value "PLAN STATUS" "MAYBE" "DRAFT, APPROVED or NOT_REQUIRED"
    reject_bad_value "PARENT" "not-an-id" "task ID"

    # A trailing space is part of the token the parser consumes, not noise.
    cp pristine.md "$task_file"
    sed -i 's/^- KIND: TASK$/- KIND: TASK /' "$task_file"
    cp "$task_file" tampered.md
    set +e
    local output
    output=$(run_tatr edit "$id" -T "Renamed" 2>&1)
    local exit_code=$?
    set -e
    if [ $exit_code -eq 0 ] || ! cmp -s tampered.md "$task_file"; then
        bad_ok=0
        bad_detail="$bad_detail; trailing space accepted (exit $exit_code): $output"
    fi

    # A CRLF record: the tail is part of every token, so it does not parse.
    cp pristine.md "$task_file"
    sed -i 's/$/\r/' "$task_file"
    cp "$task_file" tampered.md
    set +e
    output=$(run_tatr show "$id" 2>&1)
    exit_code=$?
    set -e
    if [ $exit_code -eq 0 ] \
        || ! echo "$output" | grep -q "whitespace and line endings count" \
        || ! cmp -s tampered.md "$task_file"; then
        bad_ok=0
        bad_detail="$bad_detail; CRLF accepted or unexplained (exit $exit_code): $output"
    fi

    # A key written with no value is a hand-editing slip, and says so rather
    # than silently becoming the first line of the body.
    # Both spellings, with and without the trailing space: two hand-edits that
    # look identical must not behave differently on an invisible byte.
    local field suffix
    for field in "PARENT" "DEPENDS ON"; do
        for suffix in "" " "; do
            cp pristine.md "$task_file"
            sed -i "s/^- $field: .*/- $field:$suffix/" "$task_file"
            cp "$task_file" tampered.md
            set +e
            output=$(run_tatr show "$id" 2>&1)
            exit_code=$?
            set -e
            if [ $exit_code -eq 0 ] \
                || ! echo "$output" | grep -q "$field has no value" \
                || ! cmp -s tampered.md "$task_file"; then
                bad_ok=0
                bad_detail="$bad_detail; empty '$field:$suffix' accepted (exit $exit_code): $output"
            fi
        done
    done

    cp pristine.md "$task_file"
    if [ $bad_ok -eq 1 ]; then
        pass_test
    else
        fail_test "$bad_detail"
    fi
}

test_v2_new_and_edit_fields() {
    log_test "v2 (new and edit set kind and relationships; edit clears the optional ones)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local id=$(new_task_id "Wire me" -k EPIC -P 20260101-000000 -d 20260102-000000)
    local task_file="tasks/$id/TASK.md"

    run_tatr edit "$id" -k STORY \
        -P 20260104-000000 -d 20260105-000000 -d 20260106-000000 > /dev/null 2>&1
    local set_ok=0
    # Kind and relationships are `edit`'s to set; the workflow fields are not,
    # and an edit must leave them exactly where `tatr flow` put them.
    if grep -q "^- KIND: STORY$" "$task_file" \
        && grep -q "^- FLOW STEP: BACKLOG$" "$task_file" \
        && grep -q "^- PLAN STATUS: DRAFT$" "$task_file" \
        && grep -q "^- PARENT: 20260104-000000$" "$task_file" \
        && grep -q "^- DEPENDS ON: 20260105-000000, 20260106-000000$" "$task_file"; then
        set_ok=1
    fi

    # An empty value clears an optional relationship field.
    run_tatr edit "$id" -P "" -d "" > /dev/null 2>&1

    if [ $set_ok -eq 1 ] \
        && ! grep -q "^- PARENT:" "$task_file" \
        && ! grep -q "^- DEPENDS ON:" "$task_file" \
        && grep -q "^- KIND: STORY$" "$task_file"; then
        pass_test
    else
        fail_test "set_ok=$set_ok, file: $(cat "$task_file")"
    fi
}

# PLAN STATUS: NOT_REQUIRED is unreachable through the CLI by design - it is
# how a record says its cycle predated plan state, and it is written by hand.
# It stays a legal parsed value, and the live backlog depends on that.
test_v2_plan_status_not_required() {
    log_test "v2 (hand-written NOT_REQUIRED parses, lists, filters, round-trips)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local id=$(new_task_id "Pre-flow record")
    local task_file="tasks/$id/TASK.md"
    sed -i 's/^- PLAN STATUS: DRAFT$/- PLAN STATUS: NOT_REQUIRED/' "$task_file"

    # Split declaration: `local x=$(cmd)` reports local's status, not the
    # command's (AGENTS.md, checker.sh gotcha).
    local listed
    listed=$(run_tatr ls 2>&1)
    local listed_code=$?
    local filtered=$(run_tatr ls -f ':plan_status eq NOT_REQUIRED' 2>&1)
    run_tatr edit "$id" -p 7 > /dev/null 2>&1
    local shown=$(run_tatr show "$id" 2>&1)

    if [ $listed_code -eq 0 ] \
        && echo "$listed" | grep -q "Pre-flow record" \
        && [ "$(echo "$filtered" | grep -c 'Pre-flow record')" -eq 1 ] \
        && echo "$shown" | grep -q "^- PLAN STATUS: NOT_REQUIRED$" \
        && grep -q "^- PLAN STATUS: NOT_REQUIRED$" "$task_file" \
        && grep -q "^- PRIORITY: 7$" "$task_file"; then
        pass_test
    else
        fail_test "listed($listed_code): $listed | filtered: $filtered | file: $(cat "$task_file")"
    fi
}

test_v2_filter_fields() {
    log_test "v2 (filter selects on kind, flow step, plan status, parent, depends)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local epic_id=$(new_task_id "Epic container" -k EPIC)
    sleep 1
    local blocker_id=$(new_task_id "Blocking task" -k TASK)
    sleep 1
    local draft_id=$(new_task_id "Draft task" -k TASK)
    sleep 1
    # A real parent/child pair, so ":parent eq" is answered by the Epic's own
    # generated ID rather than by an ID that happens to match nothing. The
    # blocker is driven to DONE first: the child cannot start over an open
    # dependency.
    local story_id=$(new_task_id "Child story" -k STORY \
        -P "$epic_id" -d "$blocker_id")

    drive_task_to "$epic_id" PLANNED
    drive_task_to "$blocker_id" DONE
    drive_task_to "$story_id" WORKING

    local by_kind=$(run_tatr ls -f ':kind eq EPIC' 2>&1)
    local by_kind_list=$(run_tatr ls -f ':kind in [STORY, TASK]' 2>&1)
    local by_step=$(run_tatr ls -f ':flow_step eq WORKING' 2>&1)
    local by_plan=$(run_tatr ls -f ':plan_status eq DRAFT' 2>&1)
    local by_parent=$(run_tatr ls -f ":parent eq $epic_id" 2>&1)
    local by_depends=$(run_tatr ls -f ":depends contains $blocker_id" 2>&1)

    set +e
    local bad
    bad=$(run_tatr ls -f ':kind eq NOPE' 2>&1)
    local bad_code=$?
    set -e

    if [ "$(echo "$by_kind" | grep -c 'Epic container')" -eq 1 ] \
        && [ "$(echo "$by_kind" | wc -l)" -eq 1 ] \
        && [ "$(echo "$by_kind_list" | grep -c 'Child story')" -eq 1 ] \
        && [ "$(echo "$by_kind_list" | grep -c 'Blocking task')" -eq 1 ] \
        && [ "$(echo "$by_kind_list" | grep -c 'Draft task')" -eq 1 ] \
        && [ "$(echo "$by_kind_list" | grep -c 'Epic container')" -eq 0 ] \
        && [ "$(echo "$by_step" | grep -c 'Child story')" -eq 1 ] \
        && [ "$(echo "$by_step" | wc -l)" -eq 1 ] \
        && [ "$(echo "$by_plan" | grep -c 'Draft task')" -eq 1 ] \
        && [ "$(echo "$by_plan" | wc -l)" -eq 1 ] \
        && [ "$(echo "$by_parent" | grep -c 'Child story')" -eq 1 ] \
        && [ "$(echo "$by_parent" | wc -l)" -eq 1 ] \
        && [ "$(echo "$by_depends" | grep -c 'Child story')" -eq 1 ] \
        && [ "$(echo "$by_depends" | wc -l)" -eq 1 ] \
        && [ $bad_code -ne 0 ] \
        && echo "$bad" | grep -q "invalid kind value 'NOPE'"; then
        pass_test
    else
        fail_test "kind: $by_kind | step: $by_step | plan: $by_plan | parent: $by_parent | depends: $by_depends | bad($bad_code): $bad"
    fi
}

test_transition_state_machine() {
    log_test "flow (walks every legal edge, refuses every skip)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local id=$(new_task_id "Walk me")
    local task_file="tasks/$id/TASK.md"
    local ok=1
    local seen=""

    # A bare `tatr flow` takes the single successor of the current step, so
    # the chain walks itself from BACKLOG up to REVIEWING.
    local want
    for want in UNDERSTANDING PLANNING PLANNED WORKING REVIEWING; do
        run_tatr flow "$id" > /dev/null 2>&1 || ok=0
        local got=$(flow_step_of "$id")
        seen="$seen $got"
        [ "$got" = "$want" ] || ok=0
    done

    # The plan gate is the only writer of PLAN STATUS: APPROVED, and STATUS is
    # derived from the step rather than chosen.
    grep -q "^- PLAN STATUS: APPROVED$" "$task_file" || ok=0
    grep -q "^- STATUS: IN_PROGRESS$" "$task_file" || ok=0

    # REVIEWING is the only step with two successors: the fix loop back to
    # WORKING has to be asked for by name, and the default stays COMPOUNDING.
    run_tatr flow "$id" --to WORKING > /dev/null 2>&1 || ok=0
    [ "$(flow_step_of "$id")" = "WORKING" ] || ok=0
    run_tatr flow "$id" > /dev/null 2>&1 || ok=0
    [ "$(flow_step_of "$id")" = "REVIEWING" ] || ok=0

    # Every other edge out of REVIEWING is refused, byte-identically, and the
    # refusal names the move that IS legal from here.
    cp "$task_file" before.md
    set +e
    local skip_out self_out back_out bogus_out
    skip_out=$(run_tatr flow "$id" --to DONE 2>&1); local skip_code=$?
    self_out=$(run_tatr flow "$id" --to REVIEWING 2>&1); local self_code=$?
    back_out=$(run_tatr flow "$id" --to PLANNING 2>&1); local back_code=$?
    bogus_out=$(run_tatr flow "$id" --to BOGUS 2>&1); local bogus_code=$?
    set -e

    [ $skip_code -ne 0 ] && [ $self_code -ne 0 ] && [ $back_code -ne 0 ] && [ $bogus_code -ne 0 ] || ok=0
    echo "$skip_out" | grep -q "Illegal transition" || ok=0
    echo "$skip_out" | grep -q "COMPOUNDING" || ok=0
    echo "$bogus_out" | grep -q "Invalid flow step" || ok=0
    cmp -s before.md "$task_file" || ok=0

    # DONE is terminal: a bare walk off the end says so instead of looping.
    printf -- '- VERDICT: APPROVE\n' > "tasks/$id/REVIEW.md"
    printf '# Retro\n' > "tasks/$id/RETRO.md"
    run_tatr flow "$id" > /dev/null 2>&1 || ok=0
    run_tatr flow "$id" > /dev/null 2>&1 || ok=0
    [ "$(flow_step_of "$id")" = "DONE" ] || ok=0
    set +e
    local terminal_out
    terminal_out=$(run_tatr flow "$id" 2>&1); local terminal_code=$?
    set -e
    [ $terminal_code -ne 0 ] || ok=0
    echo "$terminal_out" | grep -q "terminal" || ok=0

    if [ $ok -eq 1 ]; then
        pass_test
    else
        fail_test "walk:$seen | skip($skip_code): $skip_out | terminal($terminal_code): $terminal_out"
    fi
}

test_transition_start_guards() {
    log_test "flow (start needs an approved plan and CLOSED dependencies)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local blocker=$(new_task_id "Blocker")
    sleep 1
    local id=$(new_task_id "Start me" -d "$blocker")
    sleep 1
    local dangling=$(new_task_id "Depends on nothing real" -d 20260101-000000)
    local task_file="tasks/$id/TASK.md"
    local ok=1

    drive_task_to "$id" PLANNED || ok=0
    drive_task_to "$dangling" PLANNED || ok=0

    # An open dependency blocks the start and the diagnostic names which one.
    set +e
    local dep_out
    dep_out=$(run_tatr flow "$id" --to WORKING 2>&1); local dep_code=$?
    set -e
    [ $dep_code -ne 0 ] || ok=0
    echo "$dep_out" | grep -q "$blocker is not CLOSED" || ok=0

    # A dependency that does not resolve at all is refused too.
    set +e
    local dangling_out
    dangling_out=$(run_tatr flow "$dangling" --to WORKING 2>&1); local dangling_code=$?
    set -e
    [ $dangling_code -ne 0 ] || ok=0
    echo "$dangling_out" | grep -q "20260101-000000 does not exist" || ok=0

    # PLAN STATUS: DRAFT at PLANNED only exists by hand correction - the guard
    # still refuses it, and reports it together with the dependency rather
    # than one failure per round trip.
    sed -i 's/^- PLAN STATUS: APPROVED$/- PLAN STATUS: DRAFT/' "$task_file"
    cp "$task_file" before.md
    set +e
    local both_out
    both_out=$(run_tatr flow "$id" --to WORKING 2>&1); local both_code=$?
    set -e
    [ $both_code -ne 0 ] || ok=0
    echo "$both_out" | grep -q "2 precondition(s) not met" || ok=0
    echo "$both_out" | grep -q "the plan is not approved" || ok=0
    echo "$both_out" | grep -q "$blocker is not CLOSED" || ok=0
    cmp -s before.md "$task_file" || ok=0

    # Both requirements met, the start goes through and derives IN_PROGRESS.
    sed -i 's/^- PLAN STATUS: DRAFT$/- PLAN STATUS: APPROVED/' "$task_file"
    drive_task_to "$blocker" DONE || ok=0
    run_tatr flow "$id" --to WORKING > /dev/null 2>&1 || ok=0
    [ "$(flow_step_of "$id")" = "WORKING" ] || ok=0
    grep -q "^- STATUS: IN_PROGRESS$" "$task_file" || ok=0

    if [ $ok -eq 1 ]; then
        pass_test
    else
        fail_test "dep($dep_code): $dep_out | dangling($dangling_code): $dangling_out | both($both_code): $both_out"
    fi
}

test_transition_close_is_atomic() {
    log_test "flow (a refused close reports everything and writes nothing)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local id=$(new_task_id "Close me")
    local task_file="tasks/$id/TASK.md"
    local dir="tasks/$id"
    local ok=1

    printf '\n## Steps\n\n- [ ] the work itself\n' >> "$task_file"
    drive_task_to "$id" REVIEWING || ok=0

    # The review gate: an APPROVE verdict is not enough while a BLOCKER or
    # MAJOR finding is still open.
    printf -- '- VERDICT: APPROVE\n\n- [ ] R1.1 (MAJOR) tatr.c:1 - unresolved\n' > "$dir/REVIEW.md"
    set +e
    local open_out
    open_out=$(run_tatr flow "$id" 2>&1); local open_code=$?
    set -e
    [ $open_code -ne 0 ] || ok=0
    echo "$open_out" | grep -q "1 open BLOCKER/MAJOR finding" || ok=0

    printf -- '- VERDICT: APPROVE\n\n- [x] R1.1 (MAJOR) tatr.c:1 - fixed\n' > "$dir/REVIEW.md"
    run_tatr flow "$id" > /dev/null 2>&1 || ok=0
    [ "$(flow_step_of "$id")" = "COMPOUNDING" ] || ok=0

    # The close gate: an unchecked step, a missing retro and an invalid
    # DECISION.md status are all reported in one run, and nothing is written.
    printf -- '- STATUS: MAYBE\n' > "$dir/DECISION.md"
    cp "$task_file" before.md
    set +e
    local close_out
    close_out=$(run_tatr flow "$id" 2>&1); local close_code=$?
    set -e
    [ $close_code -ne 0 ] || ok=0
    echo "$close_out" | grep -q "3 precondition(s) not met" || ok=0
    echo "$close_out" | grep -q "1 unchecked Steps item" || ok=0
    echo "$close_out" | grep -q "there is no RETRO.md" || ok=0
    echo "$close_out" | grep -q "invalid STATUS 'MAYBE'" || ok=0
    cmp -s before.md "$task_file" || ok=0

    # Satisfied, the close writes DONE and CLOSED in the same record, and the
    # state it produces is one the lint calls clean.
    sed -i 's/^- \[ \] the work itself$/- [x] the work itself/' "$task_file"
    printf '# Retro\n' > "$dir/RETRO.md"
    printf -- '- STATUS: ACCEPTED\n' > "$dir/DECISION.md"
    run_tatr flow "$id" > /dev/null 2>&1 || ok=0
    grep -q "^- FLOW STEP: DONE$" "$task_file" || ok=0
    grep -q "^- STATUS: CLOSED$" "$task_file" || ok=0

    set +e
    local check_out
    check_out=$(run_tatr check 2>&1); local check_code=$?
    set -e
    [ $check_code -eq 0 ] || ok=0

    if [ $ok -eq 1 ]; then
        pass_test
    else
        fail_test "open($open_code): $open_out | close($close_code): $close_out | check($check_code): $check_out"
    fi
}

test_transition_epic_exemptions() {
    log_test "flow (EPIC is exempt from plan, review, retro and Steps)"

    local test_dir=$(create_test_dir)
    cd "$test_dir"
    mkdir -p tasks

    local epic=$(new_task_id "Epic container" -k EPIC)
    sleep 1
    local blocker=$(new_task_id "Blocker")
    sleep 1
    local blocked_epic=$(new_task_id "Blocked epic" -k EPIC -d "$blocker")
    sleep 1
    local epic_file="tasks/$epic/TASK.md"
    local ok=1

    # A frozen container's step boxes stay verbatim: a dropped child is honest
    # history, not something to tick to satisfy a gate.
    printf '\n## Steps\n\n- [ ] a child that was dropped\n' >> "$epic_file"

    # It walks the whole chain with no REVIEW.md, no RETRO.md and an unchecked
    # step, exactly as `tatr check` exempts it afterwards.
    drive_task_to "$epic" DONE || ok=0
    grep -q "^- FLOW STEP: DONE$" "$epic_file" || ok=0
    grep -q "^- STATUS: CLOSED$" "$epic_file" || ok=0
    grep -q "^- \[ \] a child that was dropped$" "$epic_file" || ok=0
    [ ! -f "tasks/$epic/REVIEW.md" ] || ok=0
    [ ! -f "tasks/$epic/RETRO.md" ] || ok=0

    # Only the PRESENCE of a REVIEW.md is exempt. One an Epic does carry is
    # held to the same verdict as anyone's, or the close would produce exactly
    # the `closed-not-approved` state the lint flags.
    local reviewed_epic=$(new_task_id "Reviewed epic" -k EPIC)
    printf -- '- VERDICT: REQUEST_CHANGES\n' > "tasks/$reviewed_epic/REVIEW.md"
    drive_task_to "$reviewed_epic" REVIEWING || ok=0
    set +e
    local verdict_out
    verdict_out=$(run_tatr flow "$reviewed_epic" 2>&1); local verdict_code=$?
    set -e
    [ $verdict_code -ne 0 ] || ok=0
    echo "$verdict_out" | grep -q "verdict is 'REQUEST_CHANGES'" || ok=0

    # The exemption is exactly the four `tatr check` grants. Dependencies are
    # not among them: an Epic waits for its blockers like anyone else.
    drive_task_to "$blocked_epic" PLANNED || ok=0
    set +e
    local dep_out
    dep_out=$(run_tatr flow "$blocked_epic" --to WORKING 2>&1); local dep_code=$?
    set -e
    [ $dep_code -ne 0 ] || ok=0
    echo "$dep_out" | grep -q "$blocker is not CLOSED" || ok=0

    set +e
    local check_out
    check_out=$(run_tatr check "$epic" 2>&1); local check_code=$?
    set -e
    [ $check_code -eq 0 ] || ok=0

    if [ $ok -eq 1 ]; then
        pass_test
    else
        fail_test "dep($dep_code): $dep_out | verdict($verdict_code): $verdict_out | check($check_code): $check_out"
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

    new_task_id "Open task" > /dev/null
    sleep 1
    local closed_id=$(new_task_id "Closed task")
    sleep 1
    local working_id=$(new_task_id "In progress task")
    drive_task_to "$closed_id" DONE
    drive_task_to "$working_id" WORKING

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

    new_task_id "Open task" > /dev/null
    sleep 1
    local closed_id=$(new_task_id "Closed task")
    sleep 1
    local working_id=$(new_task_id "In progress task")
    drive_task_to "$closed_id" DONE
    drive_task_to "$working_id" WORKING

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

# Writes a v2 TASK.md for check tests: dir, id, status, then the optional
# kind / flow step / plan status; body from stdin.
write_check_task() {
    local dir=$1 id=$2 status=$3
    local kind=${4:-TASK} flow_step=${5:-BACKLOG} plan_status=${6:-DRAFT}
    mkdir -p "$dir/tasks/$id"
    {
        echo "# Task $id"
        echo
        echo "- STATUS: $status"
        echo "- PRIORITY: 10"
        echo "- TAGS: feature"
        echo "- KIND: $kind"
        echo "- FLOW STEP: $flow_step"
        echo "- PLAN STATUS: $plan_status"
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
    printf '# R\n\n## Round 1\n\n- VERDICT: APPROVE\n' > "$test_dir/tasks/20260101-100001/RETRO.md"

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
    printf '\n## Round 2\n\n- VERDICT: APPROVE\n' >> "$test_dir/tasks/20260101-120000/RETRO.md"

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
    log_test "check (malformed-header: unparseable header and invalid STATUS)"
    local test_dir=$(create_test_dir)
    mkdir -p "$test_dir/tasks/20260101-140000"
    printf '# No priority line\n\n- STATUS: OPEN\n- TAGS: x\n' > "$test_dir/tasks/20260101-140000/TASK.md"
    # An invalid enum value is a parse failure under v2, not a separate rule.
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
        && echo "$output" | grep -q "20260101-140001: malformed-header: TASK.md failed to parse"; then
        pass_test
    else
        fail_test "Exit: $exit_code, output: $output"
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

    printf '# R\n\n## Round 1\n\n- VERDICT: APPROVE\n' > "$test_dir/tasks/20260101-160001/REVIEW.md"
    printf '# R\n\n## Round 1\n\n- VERDICT: APPROVE\n' > "$test_dir/tasks/20260101-160001/RETRO.md"

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
    # Trailing space after CLOSED: part of the token the parser consumes, so
    # the record does not parse at all.
    mkdir -p "$test_dir/tasks/20260101-180000"
    printf '# Trailing space\n\n- STATUS: CLOSED \n- PRIORITY: 1\n- TAGS: x\n- KIND: TASK\n- FLOW STEP: DONE\n- PLAN STATUS: APPROVED\n\n## Steps\n\n- [ ] never done\n' > "$test_dir/tasks/20260101-180000/TASK.md"
    # Prose checkbox starting with R must not be a severity finding; verdict
    # with a tail must still read APPROVE.
    write_check_task "$test_dir" "20260101-180001" "CLOSED" <<'BODY'
## Steps

- [x] done
BODY
    printf '# R\n\n## Round 1\n\n- VERDICT: APPROVE (1 round)\n\n- [ ] Rebase onto master (before merging)\n' > "$test_dir/tasks/20260101-180001/REVIEW.md"
    printf '# R\n\n## Round 1\n\n- VERDICT: APPROVE (1 round)\n' > "$test_dir/tasks/20260101-180001/RETRO.md"

    set +e
    local output
    output=$(run_tatr -r "$test_dir" check 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -eq 1 ] \
        && echo "$output" | grep -q "20260101-180000: malformed-header: TASK.md failed to parse" \
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

# --- Flow-state checks (unplanned-in-progress / EPIC exemptions) ---

test_check_unplanned_in_progress() {
    log_test "check (unplanned-in-progress requires approved plan marker)"
    local test_dir=$(create_test_dir)
    write_check_task "$test_dir" "20260101-195000" "IN_PROGRESS" TASK WORKING APPROVED <<'BODY'
## Steps

- [ ] planned work may be in progress
BODY
    write_check_task "$test_dir" "20260101-195001" "IN_PROGRESS" <<'BODY'
## Steps

- [ ] checkbox alone is not an approved plan
BODY
    write_check_task "$test_dir" "20260101-195002" "OPEN" <<'BODY'
## Steps

- [ ] ordinary backlog stays clean before work starts
BODY

    set +e
    local output
    output=$(run_tatr -r "$test_dir" check 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -eq 1 ] \
        && echo "$output" | grep -q "20260101-195001: unplanned-in-progress: IN_PROGRESS task lacks PLAN STATUS: APPROVED" \
        && ! echo "$output" | grep -q "20260101-195000" \
        && ! echo "$output" | grep -q "20260101-195002"; then
        pass_test
    else
        fail_test "Exit: $exit_code, output: $output"
    fi
}

test_check_epic_exemptions() {
    log_test "check (KIND: EPIC is exempt from the record-completeness rules)"
    local test_dir=$(create_test_dir)

    # An EPIC container: its aggregate record lives in its own TASK.md, its
    # child tasks carry review/retro, and its frozen step boxes stay verbatim.
    write_check_task "$test_dir" "20260101-196000" "CLOSED" EPIC DONE APPROVED <<'BODY'
## Child Tasks

- [x] 20260101-196001 shipped
- [ ] 20260101-196002 dropped as superseded
BODY
    # An EPIC may also sit IN_PROGRESS without an approved plan of its own.
    write_check_task "$test_dir" "20260101-196003" "IN_PROGRESS" EPIC WORKING DRAFT <<'BODY'
## Child Tasks

- [ ] 20260101-196004 in flight
BODY
    # The identical shape on an ordinary TASK is still a finding, three times
    # over: no review, no retro, and an unchecked step on a CLOSED task.
    write_check_task "$test_dir" "20260101-196005" "CLOSED" TASK DONE APPROVED <<'BODY'
## Steps

- [x] done
- [ ] not done
BODY
    # The exemption keys on KIND alone. A `goal` tag used to grant it and must
    # not any more, or a task could still exempt itself by editing a tag.
    write_check_task "$test_dir" "20260101-196006" "CLOSED" TASK DONE APPROVED <<'BODY'
## Steps

- [ ] not done
BODY
    sed -i 's/^- TAGS: feature$/- TAGS: goal, historical/' "$test_dir/tasks/20260101-196006/TASK.md"

    set +e
    local output
    output=$(run_tatr -r "$test_dir" check 2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -eq 1 ] \
        && ! echo "$output" | grep -q "20260101-196000" \
        && ! echo "$output" | grep -q "20260101-196003" \
        && echo "$output" | grep -q "20260101-196005: closed-unchecked: 1 unchecked" \
        && echo "$output" | grep -q "20260101-196005: closed-missing-review" \
        && echo "$output" | grep -q "20260101-196005: closed-missing-retro" \
        && echo "$output" | grep -q "20260101-196006: closed-unchecked: 1 unchecked" \
        && echo "$output" | grep -q "20260101-196006: closed-missing-review"; then
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

test_windows_build_target() {
    log_test "windows build target (produces PE tatr.exe)"

    if ! command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
        echo -e "${YELLOW}SKIPPED${RESET} (x86_64-w64-mingw32-gcc not found)"
        PASSED_TESTS=$((PASSED_TESTS + 1))
        return
    fi

    set +e
    local output
    output=$(
        make -C "$PROJECT_DIR" clean >/dev/null &&
        make -C "$PROJECT_DIR" windows &&
        file "$PROJECT_DIR/dist/tatr.exe"
    2>&1)
    local exit_code=$?
    set -e

    if [ $exit_code -eq 0 ] &&
        echo "$output" | grep -q "dist/tatr.exe" &&
        echo "$output" | grep -Eq "PE32|PE32\\+"; then
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
test_new_is_born_backlog
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
test_edit_priority
test_edit_tags
test_edit_title
test_edit_partial_preserves_fields
test_edit_status_uses_transition_guards
test_edit_missing_id
test_v2_task_round_trip
test_v2_never_writes_an_unreadable_record
test_v2_ls_skips_unreadable_records
test_v2_rejects_legacy_record
test_v2_rejects_invalid_metadata_atomically
test_v2_new_and_edit_fields
test_v2_plan_status_not_required
test_v2_filter_fields
test_transition_state_machine
test_transition_start_guards
test_transition_close_is_atomic
test_transition_epic_exemptions
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
test_check_closed_not_approved
test_check_bad_severity
test_check_malformed_header
test_check_ledger
test_check_per_id
test_check_exit_codes
test_check_scanner_edges
test_check_missing_artifacts
test_check_unplanned_in_progress
test_check_epic_exemptions
test_check_bad_decision_status
test_check_good_decision_status
test_check_dangling_supersede
test_check_resolving_supersede
test_check_decision_absent_unaffected
test_build_guard_bare_shell_fails
test_build_guard_nix_shell_passes
test_build_guard_override_passes
test_windows_build_target

echo
echo "Passed $PASSED_TESTS/$TOTAL_TESTS tests"

if [ $PASSED_TESTS -eq $TOTAL_TESTS ]; then
    echo -e "${GREEN}All tests passed${RESET}"
    exit 0
else
    echo -e "${RED}Some tests failed${RESET}"
    exit 1
fi
