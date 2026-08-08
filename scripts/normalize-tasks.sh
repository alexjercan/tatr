#!/usr/bin/env bash

set -eu

root="${1:-.}"
tasks_dir="$root/tasks"

if [ ! -d "$tasks_dir" ]; then
    echo "error: tasks directory not found: $tasks_dir" >&2
    exit 1
fi

find "$tasks_dir" -mindepth 2 -maxdepth 2 -name TASK.md -print0 |
while IFS= read -r -d '' task; do
    status="$(sed -n 's/^- STATUS: //p' "$task" | head -n 1)"
    priority="$(sed -n 's/^- PRIORITY: //p' "$task" | head -n 1)"
    tags="$(sed -n 's/^- TAGS: //p' "$task" | head -n 1)"
    activity="$(sed -n 's/^- ACTIVITY: //p' "$task" | head -n 1)"
    resolution="$(sed -n 's/^- RESOLUTION: //p' "$task" | head -n 1)"

    if [ -z "$status" ]; then
        if [ -n "$resolution" ] && [ "$resolution" != "-" ]; then
            status=CLOSED
        else
            case "$activity" in
                WORKING|REVIEWING|COMPOUNDING) status=IN_PROGRESS ;;
                *) status=OPEN ;;
            esac
        fi
    fi

    case "$status" in
        OPEN|IN_PROGRESS|CLOSED) ;;
        *) echo "error: cannot infer status for $task" >&2; exit 1 ;;
    esac

    case "$priority" in
        ''|*[!0-9]*) echo "error: invalid priority in $task" >&2; exit 1 ;;
    esac

    tmp="$(mktemp "$task.tmp.XXXXXX")"
    awk -v status="$status" -v priority="$priority" -v tags="$tags" '
        NR == 1 {
            print
            print ""
            print "- STATUS: " status
            print "- PRIORITY: " priority
            print "- TAGS: " tags
            next
        }
        !body && ($0 == "" || $0 ~ /^- [A-Z][A-Z ]*: /) { next }
        {
            if (!body) {
                print ""
                body = 1
            }
            print
        }
    ' "$task" > "$tmp"
    mv "$tmp" "$task"
    echo "$task: $status"
done
