#include "aids.h"
#include "argparse.h"
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

#define TATR_VERSION "0.1.0"

#define HUID_FORMAT_CSTR "%Y%m%d-%H%M%S"
#define HUID_LENGTH 16 // "20240630-235959" + null terminator

#define TASKS_PATH_CSTR "tasks"
#define TASK_FILE_NAME_CSTR "TASK.md"

static Aids_String_Slice TASKS_PATH = (Aids_String_Slice) { .str = (unsigned char *)TASKS_PATH_CSTR, .len = sizeof(TASKS_PATH_CSTR) - 1 };
static Aids_String_Slice TASK_FILE_NAME = (Aids_String_Slice) { .str = (unsigned char *)TASK_FILE_NAME_CSTR, .len = sizeof(TASK_FILE_NAME_CSTR) - 1 };

typedef struct {
    Aids_String_Slice cwd;
    int argc;
    char **argv;
} Tatr_Context;

// The v2 metadata enums. Every one of them is looked up with
// enum_from_string, which reports failure instead of falling back to a default:
// a silent default turns a typo into a wrong-but-valid record, and the caller
// can no longer tell "absent" from "misspelled".
#define ENUM_COUNT(table) (sizeof(table) / sizeof((table)[0]))

// Defined below with the other HUID helpers; the parser needs it to validate
// the PARENT and DEPENDS ON references it reads.
static boolean ishuid(const Aids_String_Slice *slice);

static boolean enum_from_string(const Aids_String_Slice *slice,
                                const Aids_String_Slice *table,
                                size_t count,
                                int *out) {
    for (size_t i = 0; i < count; ++i) {
        if (aids_string_slice_compare(slice, &table[i]) == 0) {
            *out = (int)i;
            return true;
        }
    }
    return false;
}

typedef enum {
    Task_Status_OPEN,
    Task_Status_IN_PROGRESS,
    Task_Status_CLOSED
} Task_Status;

static Aids_String_Slice Task_Status_Strings[] = {
    [Task_Status_OPEN] = (Aids_String_Slice) { .str = (unsigned char *)"OPEN", .len = 4 },
    [Task_Status_IN_PROGRESS] = (Aids_String_Slice) { .str = (unsigned char *)"IN_PROGRESS", .len = 11 },
    [Task_Status_CLOSED] = (Aids_String_Slice) { .str = (unsigned char *)"CLOSED", .len = 6 }
};

#define STATUS_VALUES_CSTR "OPEN, IN_PROGRESS or CLOSED"

static boolean task_status_from_string(const Aids_String_Slice *slice, Task_Status *out) {
    int value = 0;
    if (!enum_from_string(slice, Task_Status_Strings, ENUM_COUNT(Task_Status_Strings), &value)) {
        return false;
    }
    *out = (Task_Status)value;
    return true;
}

// KIND is what makes a record an Epic container: it replaces the old `goal`
// tag, so a container cannot be conjured (or revoked) by editing a tag list.
typedef enum {
    Task_Kind_TASK,
    Task_Kind_EPIC,
    Task_Kind_STORY,
    Task_Kind_SPIKE
} Task_Kind;

static Aids_String_Slice Task_Kind_Strings[] = {
    [Task_Kind_TASK] = (Aids_String_Slice) { .str = (unsigned char *)"TASK", .len = 4 },
    [Task_Kind_EPIC] = (Aids_String_Slice) { .str = (unsigned char *)"EPIC", .len = 4 },
    [Task_Kind_STORY] = (Aids_String_Slice) { .str = (unsigned char *)"STORY", .len = 5 },
    [Task_Kind_SPIKE] = (Aids_String_Slice) { .str = (unsigned char *)"SPIKE", .len = 5 }
};

#define KIND_VALUES_CSTR "TASK, EPIC, STORY or SPIKE"

static boolean task_kind_from_string(const Aids_String_Slice *slice, Task_Kind *out) {
    int value = 0;
    if (!enum_from_string(slice, Task_Kind_Strings, ENUM_COUNT(Task_Kind_Strings), &value)) {
        return false;
    }
    *out = (Task_Kind)value;
    return true;
}

typedef enum {
    Flow_Step_BACKLOG,
    Flow_Step_UNDERSTANDING,
    Flow_Step_PLANNING,
    Flow_Step_PLANNED,
    Flow_Step_WORKING,
    Flow_Step_REVIEWING,
    Flow_Step_COMPOUNDING,
    Flow_Step_DONE
} Flow_Step;

static Aids_String_Slice Flow_Step_Strings[] = {
    [Flow_Step_BACKLOG] = (Aids_String_Slice) { .str = (unsigned char *)"BACKLOG", .len = 7 },
    [Flow_Step_UNDERSTANDING] = (Aids_String_Slice) { .str = (unsigned char *)"UNDERSTANDING", .len = 13 },
    [Flow_Step_PLANNING] = (Aids_String_Slice) { .str = (unsigned char *)"PLANNING", .len = 8 },
    [Flow_Step_PLANNED] = (Aids_String_Slice) { .str = (unsigned char *)"PLANNED", .len = 7 },
    [Flow_Step_WORKING] = (Aids_String_Slice) { .str = (unsigned char *)"WORKING", .len = 7 },
    [Flow_Step_REVIEWING] = (Aids_String_Slice) { .str = (unsigned char *)"REVIEWING", .len = 9 },
    [Flow_Step_COMPOUNDING] = (Aids_String_Slice) { .str = (unsigned char *)"COMPOUNDING", .len = 11 },
    [Flow_Step_DONE] = (Aids_String_Slice) { .str = (unsigned char *)"DONE", .len = 4 }
};

#define FLOW_STEP_VALUES_CSTR \
    "BACKLOG, UNDERSTANDING, PLANNING, PLANNED, WORKING, REVIEWING, COMPOUNDING or DONE"

static boolean flow_step_from_string(const Aids_String_Slice *slice, Flow_Step *out) {
    int value = 0;
    if (!enum_from_string(slice, Flow_Step_Strings, ENUM_COUNT(Flow_Step_Strings), &value)) {
        return false;
    }
    *out = (Flow_Step)value;
    return true;
}

// NOT_REQUIRED is a real answer, not a missing one: it records that a record's
// cycle never carried plan approval, rather than back-dating an approval that
// never happened.
typedef enum {
    Plan_Status_DRAFT,
    Plan_Status_APPROVED,
    Plan_Status_NOT_REQUIRED
} Plan_Status;

static Aids_String_Slice Plan_Status_Strings[] = {
    [Plan_Status_DRAFT] = (Aids_String_Slice) { .str = (unsigned char *)"DRAFT", .len = 5 },
    [Plan_Status_APPROVED] = (Aids_String_Slice) { .str = (unsigned char *)"APPROVED", .len = 8 },
    [Plan_Status_NOT_REQUIRED] = (Aids_String_Slice) { .str = (unsigned char *)"NOT_REQUIRED", .len = 12 }
};

#define PLAN_STATUS_VALUES_CSTR "DRAFT, APPROVED or NOT_REQUIRED"

static boolean plan_status_from_string(const Aids_String_Slice *slice, Plan_Status *out) {
    int value = 0;
    if (!enum_from_string(slice, Plan_Status_Strings, ENUM_COUNT(Plan_Status_Strings), &value)) {
        return false;
    }
    *out = (Plan_Status)value;
    return true;
}

typedef struct {
    Task_Status status;
    unsigned int priority;
    Aids_Array tags; /* Aids_String_Slice */
    Task_Kind kind;
    Flow_Step flow_step;
    Plan_Status plan_status;
    Aids_String_Slice parent;    /* len 0 when unset */
    Aids_Array depends_on;       /* Aids_String_Slice */
} Task_Meta;

Aids_String_Slice STATUS_FORMAT = (Aids_String_Slice) { .str = (unsigned char *)"- STATUS: ", .len = 10 };
Aids_String_Slice PRIORITY_FORMAT = (Aids_String_Slice) { .str = (unsigned char *)"- PRIORITY: ", .len = 12 };
Aids_String_Slice TAGS_FORMAT = (Aids_String_Slice) { .str = (unsigned char *)"- TAGS: ", .len = 8 };
Aids_String_Slice KIND_FORMAT = (Aids_String_Slice) { .str = (unsigned char *)"- KIND: ", .len = 8 };
Aids_String_Slice FLOW_STEP_FORMAT = (Aids_String_Slice) { .str = (unsigned char *)"- FLOW STEP: ", .len = 13 };
Aids_String_Slice PLAN_STATUS_FORMAT = (Aids_String_Slice) { .str = (unsigned char *)"- PLAN STATUS: ", .len = 15 };
Aids_String_Slice PARENT_FORMAT = (Aids_String_Slice) { .str = (unsigned char *)"- PARENT: ", .len = 10 };
Aids_String_Slice DEPENDS_ON_FORMAT = (Aids_String_Slice) { .str = (unsigned char *)"- DEPENDS ON: ", .len = 14 };

typedef struct {
    Aids_String_Slice title;
    Aids_String_Slice description;
    Task_Meta meta;
    unsigned char *_buffer; // Internal buffer that owns the memory for title/description/tags
} Task;

static void task_init_empty(Task *task) {
    if (task == NULL) {
        aids_log(AIDS_ERROR, "task_init_empty: task pointer is NULL");
        return;
    }

    task->title = (Aids_String_Slice) {0};
    task->description = (Aids_String_Slice) {0};
    task->meta.status = Task_Status_OPEN;
    task->meta.priority = 0;
    task->meta.kind = Task_Kind_TASK;
    task->meta.flow_step = Flow_Step_BACKLOG;
    task->meta.plan_status = Plan_Status_DRAFT;
    task->meta.parent = (Aids_String_Slice) {0};
    task->_buffer = NULL;
    aids_array_init(&task->meta.tags, sizeof(Aids_String_Slice));
    aids_array_init(&task->meta.depends_on, sizeof(Aids_String_Slice));
}

static void task_cleanup(Task *task) {
    if (task == NULL) {
        return;
    }

    if (task->_buffer != NULL) {
        AIDS_FREE(task->_buffer);
        task->_buffer = NULL;
    }
    aids_array_free(&task->meta.tags);
    aids_array_free(&task->meta.depends_on);
}

// Serializes a "- KEY: " line whose value is a comma-separated list of slices.
// TAGS and DEPENDS ON share the exact same shape on disk.
static Aids_Result task_append_list_field(Aids_String_Builder *builder,
                                          Aids_String_Slice format,
                                          const Aids_Array *items) {
    if (aids_string_builder_append(builder, SS_Fmt, SS_Arg(format)) != AIDS_OK) {
        return AIDS_ERR;
    }
    for (size_t i = 0; i < items->count; ++i) {
        Aids_String_Slice *item = NULL;
        if (aids_array_get((Aids_Array *)items, i, (void **)&item) != AIDS_OK) {
            return AIDS_ERR;
        }
        if (aids_string_builder_append_slice(builder, *item) != AIDS_OK) {
            return AIDS_ERR;
        }
        if (i + 1 < items->count) {
            if (aids_string_builder_append(builder, ", ") != AIDS_OK) {
                return AIDS_ERR;
            }
        }
    }
    return AIDS_OK;
}

// True when the buffer starts with a valueless form of an optional field:
// "- PARENT:" or "- PARENT: " with nothing after it. Worth its own
// diagnostic: hand correction is the only path from a pre-v2 record, and an
// author who writes the key but no value would otherwise see the line
// silently become body text. Both spellings are treated alike, so an
// invisible trailing space cannot flip the behavior.
static boolean starts_with_empty_field(Aids_String_Slice buffer, Aids_String_Slice format) {
    Aids_String_Slice key = format;
    key.len -= 1; // drop the trailing space the format requires after the colon
    if (!aids_string_slice_starts_with(&buffer, key)) {
        return false;
    }
    Aids_String_Slice rest = buffer;
    aids_string_slice_skip(&rest, key.len);
    aids_string_slice_trim_right(&rest);
    Aids_String_Slice line = {0};
    if (!aids_string_slice_tokenize(&rest, '\n', &line)) {
        line = rest;
    }
    aids_string_slice_trim(&line);
    return line.len == 0;
}

// Splits a comma-separated field value into trimmed, non-empty slices.
static Aids_Result task_parse_list_field(Aids_String_Slice value, Aids_Array *out) {
    while (value.len > 0) {
        Aids_String_Slice item;
        if (!aids_string_slice_tokenize(&value, ',', &item)) {
            item = value;
            value.len = 0;
        }
        aids_string_slice_trim(&item);
        if (item.len > 0) {
            if (aids_array_append(out, &item) != AIDS_OK) {
                return AIDS_ERR;
            }
        }
    }
    return AIDS_OK;
}

static Aids_Result task_serialize(Task task, Aids_String_Slice *buffer) {
    Aids_String_Builder builder = {0};
    Aids_Result result = AIDS_OK;

    if (buffer == NULL) {
        aids_log(AIDS_ERROR, "task_serialize: buffer pointer is NULL");
        return AIDS_ERR;
    }

    aids_string_builder_init(&builder);

    // # Title
    if (aids_string_builder_append(&builder, "# ") != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_serialize: Failed to append title prefix: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }
    if (aids_string_builder_append_slice(&builder, task.title) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_serialize: Failed to append title: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }
    if (aids_string_builder_append(&builder, "\n\n") != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_serialize: Failed to append title suffix: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }

    // - STATUS: OPEN | IN_PROGRESS | CLOSED
    if (aids_string_builder_append(&builder, SS_Fmt, SS_Arg(STATUS_FORMAT)) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_serialize: Failed to append status format: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }
    if (aids_string_builder_append(&builder, SS_Fmt, SS_Arg(Task_Status_Strings[task.meta.status])) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_serialize: Failed to append status value: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }
    if (aids_string_builder_append(&builder, "\n") != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_serialize: Failed to append status newline: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }
    // - PRIORITY: 100
    if (aids_string_builder_append(&builder, SS_Fmt, SS_Arg(PRIORITY_FORMAT)) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_serialize: Failed to append priority format: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }
    if (aids_string_builder_append(&builder, "%u", task.meta.priority) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_serialize: Failed to append priority value: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }
    if (aids_string_builder_append(&builder, "\n") != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_serialize: Failed to append priority newline: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }
    // - TAGS: tag1, tag2, tag3
    if (task_append_list_field(&builder, TAGS_FORMAT, &task.meta.tags) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_serialize: Failed to append tags: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }
    if (aids_string_builder_append(&builder, "\n") != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_serialize: Failed to append tags newline: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }

    // - KIND / FLOW STEP / PLAN STATUS: always written, in this order.
    if (aids_string_builder_append(&builder, SS_Fmt SS_Fmt "\n",
                                   SS_Arg(KIND_FORMAT),
                                   SS_Arg(Task_Kind_Strings[task.meta.kind])) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_serialize: Failed to append kind: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }
    if (aids_string_builder_append(&builder, SS_Fmt SS_Fmt "\n",
                                   SS_Arg(FLOW_STEP_FORMAT),
                                   SS_Arg(Flow_Step_Strings[task.meta.flow_step])) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_serialize: Failed to append flow step: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }
    if (aids_string_builder_append(&builder, SS_Fmt SS_Fmt "\n",
                                   SS_Arg(PLAN_STATUS_FORMAT),
                                   SS_Arg(Plan_Status_Strings[task.meta.plan_status])) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_serialize: Failed to append plan status: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }

    // - PARENT / DEPENDS ON: optional, so an unrelated task carries no empty
    // relationship lines at all.
    if (task.meta.parent.len > 0) {
        if (aids_string_builder_append(&builder, SS_Fmt SS_Fmt "\n",
                                       SS_Arg(PARENT_FORMAT),
                                       SS_Arg(task.meta.parent)) != AIDS_OK) {
            aids_log(AIDS_ERROR, "task_serialize: Failed to append parent: %s", aids_failure_reason());
            return_defer(AIDS_ERR);
        }
    }
    if (task.meta.depends_on.count > 0) {
        if (task_append_list_field(&builder, DEPENDS_ON_FORMAT, &task.meta.depends_on) != AIDS_OK) {
            aids_log(AIDS_ERROR, "task_serialize: Failed to append dependencies: %s", aids_failure_reason());
            return_defer(AIDS_ERR);
        }
        if (aids_string_builder_append(&builder, "\n") != AIDS_OK) {
            aids_log(AIDS_ERROR, "task_serialize: Failed to append dependencies newline: %s", aids_failure_reason());
            return_defer(AIDS_ERR);
        }
    }

    if (aids_string_builder_append(&builder, "\n") != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_serialize: Failed to append metadata suffix: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }

    if (aids_string_builder_append_slice(&builder, task.description) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_serialize: Failed to append description: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }

    aids_string_builder_to_slice(&builder, buffer);

defer:
    if (result != AIDS_OK) {
        aids_string_builder_free(&builder);
    }
    return result;
}

static Aids_Result task_deserialize(Aids_String_Slice buffer, Task *task) {
    Aids_Result result = AIDS_OK;

    if (task == NULL) {
        aids_log(AIDS_ERROR, "task_deserialize: task pointer is NULL");
        return AIDS_ERR;
    }

    task_init_empty(task);

    // # Title
    if (!aids_string_slice_tokenize(&buffer, '\n', &task->title)) {
        aids_log(AIDS_ERROR, "task_deserialize: Failed to parse title from buffer");
        return_defer(AIDS_ERR);
    }
    if (!aids_string_slice_starts_with(&task->title, (Aids_String_Slice) { .str = (unsigned char *)"# ", .len = 2 })) {
        aids_log(AIDS_ERROR, "task_deserialize: Title does not start with expected prefix '# '");
        return_defer(AIDS_ERR);
    }
    aids_string_slice_skip(&task->title, 2);
    aids_string_slice_skip_while(&buffer, isspace);

    // - STATUS: OPEN | IN_PROGRESS | CLOSED
    if (!aids_string_slice_starts_with(&buffer, STATUS_FORMAT)) {
        aids_log(AIDS_ERROR, "task_deserialize: Buffer does not start with expected status format");
        return_defer(AIDS_ERR);
    }
    aids_string_slice_skip(&buffer, STATUS_FORMAT.len);
    Aids_String_Slice status_slice;
    if (!aids_string_slice_tokenize(&buffer, '\n', &status_slice)) {
        aids_log(AIDS_ERROR, "task_deserialize: Failed to parse status from buffer");
        return_defer(AIDS_ERR);
    }
    if (!task_status_from_string(&status_slice, &task->meta.status)) {
        aids_log(AIDS_ERROR, "task_deserialize: invalid STATUS '" SS_Fmt "' (use " STATUS_VALUES_CSTR "; whitespace and line endings count)",
                 SS_Arg(status_slice));
        return_defer(AIDS_ERR);
    }
    aids_string_slice_skip_while(&buffer, isspace);

    // - PRIORITY: 100
    if (!aids_string_slice_starts_with(&buffer, PRIORITY_FORMAT)) {
        aids_log(AIDS_ERROR, "task_deserialize: Buffer does not start with expected priority format");
        return_defer(AIDS_ERR);
    }
    aids_string_slice_skip(&buffer, PRIORITY_FORMAT.len);
    Aids_String_Slice priority_slice;
    if (!aids_string_slice_tokenize(&buffer, '\n', &priority_slice)) {
        aids_log(AIDS_ERROR, "task_deserialize: Failed to parse priority from buffer");
        return_defer(AIDS_ERR);
    }
    if (!aids_string_slice_atol(&priority_slice, (long *)&task->meta.priority, 10)) {
        aids_log(AIDS_ERROR, "task_deserialize: Failed to convert priority to number: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }
    aids_string_slice_skip_while(&buffer, isspace);

    // - TAGS: tag1, tag2, tag3
    if (!aids_string_slice_starts_with(&buffer, TAGS_FORMAT)) {
        aids_log(AIDS_ERROR, "task_deserialize: Buffer does not start with expected tags format");
        return_defer(AIDS_ERR);
    }
    aids_string_slice_skip(&buffer, TAGS_FORMAT.len);
    Aids_String_Slice tags_slice;
    if (!aids_string_slice_tokenize(&buffer, '\n', &tags_slice)) {
        aids_log(AIDS_ERROR, "task_deserialize: Failed to parse tags from buffer");
        return_defer(AIDS_ERR);
    }

    if (task_parse_list_field(tags_slice, &task->meta.tags) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_deserialize: Failed to append tag to array: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }
    aids_string_slice_skip_while(&buffer, isspace);

    // - KIND: TASK | EPIC | STORY | SPIKE
    // This is where a pre-v2 record stops looking like a v2 one, so it is the
    // right place to say so: there is no compatibility path to fall back to.
    if (!aids_string_slice_starts_with(&buffer, KIND_FORMAT)) {
        aids_log(AIDS_ERROR, "task_deserialize: expected '" SS_Fmt "' after the TAGS line "
                 "(v2 field order: STATUS, PRIORITY, TAGS, KIND, FLOW STEP, PLAN STATUS, "
                 "[PARENT], [DEPENDS ON]); correct the record by hand", SS_Arg(KIND_FORMAT));
        return_defer(AIDS_ERR);
    }
    aids_string_slice_skip(&buffer, KIND_FORMAT.len);
    Aids_String_Slice kind_slice;
    if (!aids_string_slice_tokenize(&buffer, '\n', &kind_slice)) {
        aids_log(AIDS_ERROR, "task_deserialize: Failed to parse kind from buffer");
        return_defer(AIDS_ERR);
    }
    if (!task_kind_from_string(&kind_slice, &task->meta.kind)) {
        aids_log(AIDS_ERROR, "task_deserialize: invalid KIND '" SS_Fmt "' (use " KIND_VALUES_CSTR "; whitespace and line endings count)",
                 SS_Arg(kind_slice));
        return_defer(AIDS_ERR);
    }
    aids_string_slice_skip_while(&buffer, isspace);

    // - FLOW STEP: BACKLOG | ... | DONE
    if (!aids_string_slice_starts_with(&buffer, FLOW_STEP_FORMAT)) {
        aids_log(AIDS_ERROR, "task_deserialize: expected '" SS_Fmt "' after the KIND line", SS_Arg(FLOW_STEP_FORMAT));
        return_defer(AIDS_ERR);
    }
    aids_string_slice_skip(&buffer, FLOW_STEP_FORMAT.len);
    Aids_String_Slice flow_step_slice;
    if (!aids_string_slice_tokenize(&buffer, '\n', &flow_step_slice)) {
        aids_log(AIDS_ERROR, "task_deserialize: Failed to parse flow step from buffer");
        return_defer(AIDS_ERR);
    }
    if (!flow_step_from_string(&flow_step_slice, &task->meta.flow_step)) {
        aids_log(AIDS_ERROR, "task_deserialize: invalid FLOW STEP '" SS_Fmt "' (use " FLOW_STEP_VALUES_CSTR "; whitespace and line endings count)",
                 SS_Arg(flow_step_slice));
        return_defer(AIDS_ERR);
    }
    aids_string_slice_skip_while(&buffer, isspace);

    // - PLAN STATUS: DRAFT | APPROVED | NOT_REQUIRED
    if (!aids_string_slice_starts_with(&buffer, PLAN_STATUS_FORMAT)) {
        aids_log(AIDS_ERROR, "task_deserialize: expected '" SS_Fmt "' after the FLOW STEP line", SS_Arg(PLAN_STATUS_FORMAT));
        return_defer(AIDS_ERR);
    }
    aids_string_slice_skip(&buffer, PLAN_STATUS_FORMAT.len);
    Aids_String_Slice plan_status_slice;
    if (!aids_string_slice_tokenize(&buffer, '\n', &plan_status_slice)) {
        aids_log(AIDS_ERROR, "task_deserialize: Failed to parse plan status from buffer");
        return_defer(AIDS_ERR);
    }
    if (!plan_status_from_string(&plan_status_slice, &task->meta.plan_status)) {
        aids_log(AIDS_ERROR, "task_deserialize: invalid PLAN STATUS '" SS_Fmt "' (use " PLAN_STATUS_VALUES_CSTR "; whitespace and line endings count)",
                 SS_Arg(plan_status_slice));
        return_defer(AIDS_ERR);
    }
    aids_string_slice_skip_while(&buffer, isspace);

    // - PARENT: <huid>   (optional)
    if (aids_string_slice_starts_with(&buffer, PARENT_FORMAT)) {
        aids_string_slice_skip(&buffer, PARENT_FORMAT.len);
        Aids_String_Slice parent_slice;
        if (!aids_string_slice_tokenize(&buffer, '\n', &parent_slice)) {
            aids_log(AIDS_ERROR, "task_deserialize: Failed to parse parent from buffer");
            return_defer(AIDS_ERR);
        }
        if (parent_slice.len == 0) {
            aids_log(AIDS_ERROR, "task_deserialize: PARENT has no value; omit the line entirely, or write '"
                     SS_Fmt "20240630-235959'", SS_Arg(PARENT_FORMAT));
            return_defer(AIDS_ERR);
        }
        if (!ishuid(&parent_slice)) {
            aids_log(AIDS_ERROR, "task_deserialize: invalid PARENT '" SS_Fmt "' (expected a task ID like 20240630-235959)",
                     SS_Arg(parent_slice));
            return_defer(AIDS_ERR);
        }
        task->meta.parent = parent_slice;
        aids_string_slice_skip_while(&buffer, isspace);
    }

    // - DEPENDS ON: <huid>, <huid>   (optional)
    if (aids_string_slice_starts_with(&buffer, DEPENDS_ON_FORMAT)) {
        aids_string_slice_skip(&buffer, DEPENDS_ON_FORMAT.len);
        Aids_String_Slice depends_slice;
        if (!aids_string_slice_tokenize(&buffer, '\n', &depends_slice)) {
            aids_log(AIDS_ERROR, "task_deserialize: Failed to parse dependencies from buffer");
            return_defer(AIDS_ERR);
        }
        if (task_parse_list_field(depends_slice, &task->meta.depends_on) != AIDS_OK) {
            aids_log(AIDS_ERROR, "task_deserialize: Failed to append dependency to array: %s", aids_failure_reason());
            return_defer(AIDS_ERR);
        }
        // "- DEPENDS ON: " with nothing after it reads the same to a human as
        // "- DEPENDS ON:", so it must not behave differently on that one
        // invisible byte.
        if (task->meta.depends_on.count == 0) {
            aids_log(AIDS_ERROR, "task_deserialize: DEPENDS ON has no value; omit the line entirely, or write '"
                     SS_Fmt "20240630-235959'", SS_Arg(DEPENDS_ON_FORMAT));
            return_defer(AIDS_ERR);
        }
        // Syntax only. Whether the referenced tasks exist, and whether the
        // graph they form is acyclic, is not this layer's question.
        for (size_t i = 0; i < task->meta.depends_on.count; ++i) {
            Aids_String_Slice *dep = NULL;
            if (aids_array_get(&task->meta.depends_on, i, (void **)&dep) != AIDS_OK) {
                return_defer(AIDS_ERR);
            }
            if (!ishuid(dep)) {
                aids_log(AIDS_ERROR, "task_deserialize: invalid DEPENDS ON entry '" SS_Fmt "' (expected a task ID like 20240630-235959)",
                         SS_Arg(*dep));
                return_defer(AIDS_ERR);
            }
        }
        aids_string_slice_skip_while(&buffer, isspace);
    }

    // Everything left is body text, kept byte for byte. The parser does not
    // police it: a bullet is a bullet, even an uppercase one. The single
    // exception is a valueless optional field, which is a hand-editing slip
    // rather than prose.
    if (starts_with_empty_field(buffer, PARENT_FORMAT)) {
        aids_log(AIDS_ERROR, "task_deserialize: PARENT has no value; omit the line entirely, or write '"
                 SS_Fmt "20240630-235959'", SS_Arg(PARENT_FORMAT));
        return_defer(AIDS_ERR);
    }
    if (starts_with_empty_field(buffer, DEPENDS_ON_FORMAT)) {
        aids_log(AIDS_ERROR, "task_deserialize: DEPENDS ON has no value; omit the line entirely, or write '"
                 SS_Fmt "20240630-235959'", SS_Arg(DEPENDS_ON_FORMAT));
        return_defer(AIDS_ERR);
    }

    task->description = buffer;

defer:
    if (result != AIDS_OK) {
        task_cleanup(task);
    }
    return result;
}

static Aids_Result huid(char *huid_str) {
    Aids_Result result = AIDS_OK;
    time_t current_time;
    struct tm *time_info;

    time(&current_time);
    time_info = localtime(&current_time);

    if (time_info == NULL) {
        aids_log(AIDS_ERROR, "huid: Failed to get local time");
        return_defer(AIDS_ERR);
    }

    if (strftime(huid_str, HUID_LENGTH, HUID_FORMAT_CSTR, time_info) == 0) {
        aids_log(AIDS_ERROR, "huid: Failed to format time string");
        return_defer(AIDS_ERR);
    }

defer:
    return result;
}

static boolean ishuid(const Aids_String_Slice *slice) {
    if (slice->len != HUID_LENGTH - 1) {
        return false;
    }

    for (size_t i = 0; i < slice->len; ++i) {
        char c = slice->str[i];
        if (i == 8) {
            if (c != '-') {
                return false;
            }
        } else {
            if (!isdigit(c)) {
                return false;
            }
        }
    }

    return true;
}

static Aids_Result tasks_dir_path_build(const Aids_String_Slice *cwd,
                                        Aids_String_Slice *out_path) {
    Aids_Result result = AIDS_OK;
    Aids_String_Builder path_sb = {0};
    Aids_String_Builder current_dir_sb = {0};
    Aids_String_Slice current_dir = {0};
    Aids_String_Slice candidate_path = {0};

    aids_string_builder_init(&current_dir_sb);
    if (aids_string_builder_append(&current_dir_sb, SS_Fmt, SS_Arg(*cwd)) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to initialize current directory: %s", aids_failure_reason());
        aids_string_builder_free(&current_dir_sb);
        return_defer(AIDS_ERR);
    }
    aids_string_builder_to_slice(&current_dir_sb, &current_dir);

    while (current_dir.len > 0) {
        aids_string_builder_init(&path_sb);
        if (aids_string_builder_append(&path_sb, SS_Fmt "/" SS_Fmt,
                                       SS_Arg(current_dir), SS_Arg(TASKS_PATH)) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to build candidate tasks directory path: %s", aids_failure_reason());
            aids_string_builder_free(&path_sb);
            aids_string_builder_free(&current_dir_sb);
            return_defer(AIDS_ERR);
        }
        aids_string_builder_to_slice(&path_sb, &candidate_path);

        if (aids_io_isdir(&candidate_path)) {
            *out_path = candidate_path;
            aids_string_builder_free(&current_dir_sb);
            return_defer(AIDS_OK);
        }

        aids_string_builder_free(&path_sb);

        size_t last_slash = 0;
        boolean found_slash = false;
        for (size_t i = 0; i < current_dir.len; i++) {
            if (current_dir.str[i] == '/') {
                last_slash = i;
                found_slash = true;
            }
        }

        if (!found_slash || last_slash == 0) {
            aids_log(AIDS_ERROR, "No 'tasks' directory found in hierarchy from " SS_Fmt " to root", SS_Arg(*cwd));
            aids_string_builder_free(&current_dir_sb);
            return_defer(AIDS_ERR);
        }

        current_dir.len = last_slash;
    }

    AIDS_UNREACHABLE("tasks_dir_path_build");

defer:
    return result;
}

static Aids_Result task_dir_path_build(const Aids_String_Slice *tasks_dir,
                                       const Aids_String_Slice *huid,
                                       Aids_String_Slice *out_path) {
    Aids_Result result = AIDS_OK;
    Aids_String_Builder path_sb = {0};

    aids_string_builder_init(&path_sb);
    if (aids_string_builder_append(&path_sb, SS_Fmt "/" SS_Fmt, SS_Arg(*tasks_dir), SS_Arg(*huid)) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to build task directory path: %s", aids_failure_reason());
        aids_string_builder_free(&path_sb);
        return_defer(AIDS_ERR);
    }

    aids_string_builder_to_slice(&path_sb, out_path);

defer:
    return result;
}

static Aids_Result task_file_path_build(const Aids_String_Slice *tasks_dir,
                                       const Aids_String_Slice *huid,
                                       Aids_String_Slice *out_path) {
    Aids_Result result = AIDS_OK;
    Aids_String_Builder path_sb = {0};

    aids_string_builder_init(&path_sb);
    if (aids_string_builder_append(&path_sb, SS_Fmt "/" SS_Fmt "/" SS_Fmt, SS_Arg(*tasks_dir), SS_Arg(*huid), SS_Arg(TASK_FILE_NAME)) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to build task file path: %s", aids_failure_reason());
        aids_string_builder_free(&path_sb);
        return_defer(AIDS_ERR);
    }

    aids_string_builder_to_slice(&path_sb, out_path);

defer:
    return result;
}

static Aids_Result task_save(const Aids_String_Slice *task_file_path, Task *task) {
    Aids_Result result = AIDS_OK;
    Aids_String_Slice serialized_task = {0};
    Task verify = {0};
    boolean verify_initialized = false;

    if (task_serialize(*task, &serialized_task) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to serialize task: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }

    // tatr must never write a record it cannot read back. A title or tag
    // carrying a newline, or any future serializer change that drifts from the
    // parser, would otherwise land on disk and only surface later as a
    // malformed-header finding. Validate before writing, never half-apply.
    if (task_deserialize(serialized_task, &verify) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Refusing to write '" SS_Fmt "': the record would not parse back "
                 "(a newline in the title or a tag is the usual cause)", SS_Arg(*task_file_path));
        return_defer(AIDS_ERR);
    }
    verify_initialized = true;

    if (aids_io_write(task_file_path, &serialized_task, "w") != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to write task to file: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }

defer:
    if (verify_initialized) {
        // The verify task borrows serialized_task; it never owns the buffer.
        verify._buffer = NULL;
        task_cleanup(&verify);
    }
    if (serialized_task.str != NULL) {
        AIDS_FREE(serialized_task.str);
    }
    return result;
}

static Aids_Result task_create(const Aids_String_Slice *cwd, Aids_String_Slice huid, Task *task) {
    Aids_Result result = AIDS_OK;
    Aids_String_Slice tasks_dir = {0};
    Aids_String_Slice task_dir = {0};
    Aids_String_Slice task_file_path = {0};

    if (huid.str == NULL || huid.len == 0) {
        aids_log(AIDS_ERROR, "Invalid huid provided");
        return_defer(AIDS_ERR);
    }

    if (tasks_dir_path_build(cwd, &tasks_dir) != AIDS_OK) {
        return_defer(AIDS_ERR);
    }

    if (task_dir_path_build(&tasks_dir, &huid, &task_dir) != AIDS_OK) {
        return_defer(AIDS_ERR);
    }

    // IDs have second resolution, so a second `new` call in the same second
    // would silently overwrite the first task's TASK.md. Refuse instead.
    if (aids_io_isdir(&task_dir)) {
        aids_log(AIDS_ERROR, "Task '" SS_Fmt "' already exists (same-second ID collision); retry to get a fresh ID", SS_Arg(huid));
        return_defer(AIDS_ERR);
    }

    // Build the file path BEFORE creating anything: a failure here would
    // otherwise strand the empty directory it had already made.
    if (task_file_path_build(&tasks_dir, &huid, &task_file_path) != AIDS_OK) {
        return_defer(AIDS_ERR);
    }

    if (aids_io_mkdir(&task_dir, true) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to create task directory: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }

    if (task_save(&task_file_path, task) != AIDS_OK) {
        // The directory was created a moment ago and the file was never
        // written, so leaving it behind would strand an empty task the tools
        // then have to explain. Undo it; a failed create creates nothing.
        // rmdir only succeeds on an empty directory, which is exactly the
        // state we are undoing.
        char task_dir_buffer[PATH_MAX];
        int written = snprintf(task_dir_buffer, sizeof(task_dir_buffer), SS_Fmt, SS_Arg(task_dir));
        if (written > 0 && (size_t)written < sizeof(task_dir_buffer)) {
            rmdir(task_dir_buffer);
        }
        return_defer(AIDS_ERR);
    }

defer:
    if (tasks_dir.str != NULL) {
        AIDS_FREE(tasks_dir.str);
    }
    if (task_dir.str != NULL) {
        AIDS_FREE(task_dir.str);
    }
    if (task_file_path.str != NULL) {
        AIDS_FREE(task_file_path.str);
    }
    return result;
}

// The workflow fields STATUS, FLOW STEP and PLAN STATUS are written by
// `tatr flow` alone, so their flags are gone from `new` and `edit`. argparse
// rejects an unknown option with a generic message that says nothing about
// where the field went, so both subcommands scan for the retired spellings
// first and point at the lifecycle. The scan is a plain token match over the
// subcommand's argv - the same exact-name match argparse itself does.
// Returns true (after logging) when a retired flag was found.
static boolean argparse_reject_retired_workflow_flags(const Tatr_Context *ctx) {
    static const struct {
        const char *short_name;
        const char *long_name;
        const char *field;
        const char *pointer;
    } retired[] = {
        {"-s", "--status", "STATUS",
         "STATUS is derived from FLOW STEP; move the task with `tatr flow <ID> [--to <STEP>]`"},
        {"-f", "--flow-step", "FLOW STEP",
         "move the task with `tatr flow <ID> [--to <STEP>]`"},
        {"-S", "--plan-status", "PLAN STATUS",
         "PLAN STATUS: APPROVED is written by the plan gate: `tatr flow <ID> --to PLANNED`"},
    };

    for (int i = 1; i < ctx->argc; ++i) {
        for (size_t j = 0; j < sizeof(retired) / sizeof(retired[0]); ++j) {
            if (strcmp(ctx->argv[i], retired[j].short_name) != 0 &&
                strcmp(ctx->argv[i], retired[j].long_name) != 0) {
                continue;
            }
            aids_log(AIDS_ERROR, "'%s' was removed: %s is not settable through `new` or `edit`",
                     ctx->argv[i], retired[j].field);
            fprintf(stderr, "  %s\n", retired[j].pointer);
            return true;
        }
    }
    return false;
}

// Registers the v2 metadata options shared by `new` and `edit`, so the two
// subcommands cannot drift apart in flag names or descriptions. Kind and
// relationships only: the workflow fields belong to `tatr flow`.
static void argparse_add_v2_meta_arguments(Argparse_Parser *parser) {
    argparse_add_argument(parser, (Argparse_Options){
        .short_name = 'k',
        .long_name = "kind",
        .description = "Task kind (" KIND_VALUES_CSTR ")",
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    argparse_add_argument(parser, (Argparse_Options){
        .short_name = 'P',
        .long_name = "parent",
        .description = "Parent task ID (empty value clears it)",
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    argparse_add_argument(parser, (Argparse_Options){
        .short_name = 'd',
        .long_name = "depends-on",
        .description = "Dependency task IDs (replaces existing; empty value clears them)",
        .type = ARGUMENT_TYPE_VALUE_ARRAY,
        .required = 0
    });
}

// Applies whichever v2 metadata options were given to an initialized task.
// Validates before mutating anything the caller will write, so a bad value
// leaves the record on disk untouched. Returns false after logging.
static boolean task_apply_v2_meta_arguments(Argparse_Parser *parser, Task *task) {
    char *kind_str = argparse_get_value(parser, "kind");
    if (kind_str != NULL) {
        Aids_String_Slice slice = aids_string_slice_from_cstr(kind_str);
        if (!task_kind_from_string(&slice, &task->meta.kind)) {
            aids_log(AIDS_ERROR, "Invalid kind '%s': expected " KIND_VALUES_CSTR, kind_str);
            return false;
        }
    }

    // An empty value is how the optional relationship fields are cleared.
    char *parent_str = argparse_get_value(parser, "parent");
    if (parent_str != NULL) {
        Aids_String_Slice slice = aids_string_slice_from_cstr(parent_str);
        if (slice.len == 0) {
            task->meta.parent = (Aids_String_Slice) {0};
        } else if (!ishuid(&slice)) {
            aids_log(AIDS_ERROR, "Invalid parent '%s': expected a task ID like 20240630-235959", parent_str);
            return false;
        } else {
            task->meta.parent = slice;
        }
    }

    char *depends[ARGPARSE_CAPACITY];
    unsigned long depends_count = argparse_get_values(parser, "depends-on", depends);
    if (depends_count > 0) {
        for (unsigned long i = 0; i < depends_count; ++i) {
            Aids_String_Slice slice = aids_string_slice_from_cstr(depends[i]);
            if (slice.len > 0 && !ishuid(&slice)) {
                aids_log(AIDS_ERROR, "Invalid dependency '%s': expected a task ID like 20240630-235959", depends[i]);
                return false;
            }
        }
        task->meta.depends_on.count = 0; // Replaces the existing list
        for (unsigned long i = 0; i < depends_count; ++i) {
            Aids_String_Slice slice = aids_string_slice_from_cstr(depends[i]);
            if (slice.len == 0) {
                continue;
            }
            if (aids_array_append(&task->meta.depends_on, &slice) != AIDS_OK) {
                aids_log(AIDS_ERROR, "Failed to append dependency: %s", aids_failure_reason());
                return false;
            }
        }
    }

    return true;
}

// Reads the whole of stdin into an owned slice. Used by `new --body-file -`.
static Aids_Result read_stdin_all(Aids_String_Slice *out) {
    Aids_Result result = AIDS_OK;
    Aids_String_Builder sb = {0};
    unsigned char chunk[4096];
    size_t n;

    aids_string_builder_init(&sb);
    while ((n = fread(chunk, 1, sizeof(chunk), stdin)) > 0) {
        Aids_String_Slice part = { .str = chunk, .len = n };
        if (aids_string_builder_append_slice(&sb, part) != AIDS_OK) {
            aids_log(AIDS_ERROR, "read_stdin_all: Failed to append stdin chunk: %s", aids_failure_reason());
            return_defer(AIDS_ERR);
        }
    }
    if (ferror(stdin)) {
        aids_log(AIDS_ERROR, "read_stdin_all: Failed to read stdin: %s", strerror(errno));
        return_defer(AIDS_ERR);
    }

    aids_string_builder_to_slice(&sb, out);

defer:
    if (result != AIDS_OK) {
        aids_string_builder_free(&sb);
    }
    return result;
}

static int main_new(const Tatr_Context *ctx) {
    int result = 0;
    Argparse_Parser parser = {0};
    Task task = {0};
    boolean task_initialized = false;
    Aids_String_Slice body = {0};

    if (argparse_reject_retired_workflow_flags(ctx)) {
        return_defer(1);
    }

    argparse_parser_init(&parser, "tatr new", "Create a new task", TATR_VERSION);

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'T',
        .long_name = "title",
        .description = "Task title",
        .type = ARGUMENT_TYPE_POSITIONAL,
        .required = 0
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'p',
        .long_name = "priority",
        .description = "Task priority (default: 0)",
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 't',
        .long_name = "tags",
        .description = "Task tags (comma-separated)",
        .type = ARGUMENT_TYPE_VALUE_ARRAY,
        .required = 0
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'b',
        .long_name = "body-file",
        .description = "Read the description body from a file ('-' reads stdin)",
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    argparse_add_v2_meta_arguments(&parser);

    if (argparse_parse(&parser, ctx->argc, ctx->argv) != ARG_OK) {
        return_defer(1);
    }

    task_init_empty(&task);
    task_initialized = true;

    char *title = argparse_get_value(&parser, "title");
    if (title != NULL) {
        task.title = aids_string_slice_from_cstr(title);
    }

    char *priority_str = argparse_get_value(&parser, "priority");
    if (priority_str != NULL) {
        long priority;
        Aids_String_Slice priority_slice = aids_string_slice_from_cstr(priority_str);
        if (aids_string_slice_atol(&priority_slice, &priority, 10)) {
            if (priority >= 0) {
                task.meta.priority = (unsigned int)priority;
            } else {
                aids_log(AIDS_ERROR, "Priority must be a non-negative number");
                return_defer(1);
            }
        } else {
            aids_log(AIDS_ERROR, "Invalid priority value: %s", priority_str);
            return_defer(1);
        }
    }

    char *tags[ARGPARSE_CAPACITY];
    unsigned long tag_count = argparse_get_values(&parser, "tags", tags);
    for (unsigned long i = 0; i < tag_count; ++i) {
        Aids_String_Slice tag = aids_string_slice_from_cstr(tags[i]);
        if (aids_array_append(&task.meta.tags, &tag) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to append tag: %s", aids_failure_reason());
            return_defer(1);
        }
    }

    if (!task_apply_v2_meta_arguments(&parser, &task)) {
        return_defer(1);
    }

    // Read the body before generating the ID so a bad path fails without
    // creating anything on disk.
    char *body_file = argparse_get_value(&parser, "body-file");
    if (body_file != NULL) {
        if (strcmp(body_file, "-") == 0) {
            if (read_stdin_all(&body) != AIDS_OK) {
                return_defer(1);
            }
        } else {
            Aids_String_Slice body_path = aids_string_slice_from_cstr(body_file);
            if (aids_io_read(&body_path, &body, "r") != AIDS_OK) {
                aids_log(AIDS_ERROR, "Failed to read body file '%s': %s", body_file, aids_failure_reason());
                return_defer(1);
            }
        }
        task.description = body;
    }

    char huid_str[HUID_LENGTH] = {0};
    if (huid(huid_str) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to generate huid");
        return_defer(1);
    }
    Aids_String_Slice id = aids_string_slice_from_cstr(huid_str);

    if (task_create(&ctx->cwd, id, &task) != AIDS_OK) {
        // No aids_failure_reason() here: it is whatever the last aids call
        // left behind, including a successful one, and task_create has
        // already logged the actionable cause.
        aids_log(AIDS_ERROR, "Failed to create new task");
        return_defer(1);
    }

    printf("Task created successfully with ID: " SS_Fmt "\n", SS_Arg(id));

defer:
    if (task_initialized) {
        task_cleanup(&task);
    }
    if (body.str != NULL) {
        AIDS_FREE(body.str);
    }
    argparse_parser_free(&parser);
    return result;
}

// Loads a task and hands back the raw bytes it was parsed from. The task owns
// them through task->_buffer, so *raw stays valid until task_cleanup. Callers
// that need the body verbatim - the close gate counts the unchecked Steps in
// the very bytes the record was parsed from - use this rather than re-reading
// the file behind the parser's back.
static Aids_Result task_load_raw(const Aids_String_Slice *task_file_path, Task *task,
                                 Aids_String_Slice *raw) {
    Aids_Result result = AIDS_OK;
    Aids_String_Slice serialized_task = {0};

    if (aids_io_read(task_file_path, &serialized_task, "r") != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to read task file '" SS_Fmt "': %s",
                SS_Arg(*task_file_path), aids_failure_reason());
        return_defer(AIDS_ERR);
    }

    if (task_deserialize(serialized_task, task) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to deserialize task file '" SS_Fmt "'", SS_Arg(*task_file_path));
        AIDS_FREE(serialized_task.str);
        return_defer(AIDS_ERR);
    }

    // Store the buffer so it can be freed when the task is cleaned up
    task->_buffer = serialized_task.str;
    if (raw != NULL) {
        *raw = serialized_task;
    }

defer:
    return result;
}

static Aids_Result task_load(const Aids_String_Slice *task_file_path, Task *task) {
    return task_load_raw(task_file_path, task, NULL);
}

// Resolves a task HUID argument to the located tasks directory and the task's
// file path, verifying the HUID is well-formed and that the task exists. On
// success the caller owns both *tasks_dir and *task_file_path and must free
// them. Shared by the show, edit and rm subcommands.
static Aids_Result task_resolve(const Aids_String_Slice *cwd,
                                const Aids_String_Slice *huid,
                                Aids_String_Slice *tasks_dir,
                                Aids_String_Slice *task_file_path) {
    Aids_Result result = AIDS_OK;
    Aids_String_Slice td = {0};
    Aids_String_Slice task_dir = {0};
    Aids_String_Slice tfp = {0};

    if (!ishuid(huid)) {
        aids_log(AIDS_ERROR, "Invalid task ID '" SS_Fmt "': expected format YYYYMMDD-HHMMSS", SS_Arg(*huid));
        return_defer(AIDS_ERR);
    }

    if (tasks_dir_path_build(cwd, &td) != AIDS_OK) {
        return_defer(AIDS_ERR);
    }

    if (task_dir_path_build(&td, huid, &task_dir) != AIDS_OK) {
        return_defer(AIDS_ERR);
    }

    if (!aids_io_isdir(&task_dir)) {
        aids_log(AIDS_ERROR, "Task '" SS_Fmt "' not found", SS_Arg(*huid));
        return_defer(AIDS_ERR);
    }

    if (task_file_path_build(&td, huid, &tfp) != AIDS_OK) {
        return_defer(AIDS_ERR);
    }

    *tasks_dir = td;
    td = (Aids_String_Slice){0}; // Ownership transferred to caller
    *task_file_path = tfp;
    tfp = (Aids_String_Slice){0}; // Ownership transferred to caller

defer:
    if (td.str != NULL) {
        AIDS_FREE(td.str);
    }
    if (task_dir.str != NULL) {
        AIDS_FREE(task_dir.str);
    }
    if (tfp.str != NULL) {
        AIDS_FREE(tfp.str);
    }
    return result;
}

static void print_file_path(const char *path) {
    if (isatty(STDOUT_FILENO)) {
        printf(AIDS_TERMINAL_BLUE "\033]8;;editor://%s\033\\%s\033]8;;\033\\" AIDS_TERMINAL_RESET, path, path);
    } else {
        printf("%s", path);
    }
}

static void task_print(Aids_String_Slice tasks_dir, Aids_String_Slice huid, Task task) {
    char path_buffer[PATH_MAX];
    if (snprintf(path_buffer, sizeof(path_buffer), SS_Fmt "/" SS_Fmt "/%s", SS_Arg(tasks_dir), SS_Arg(huid), TASK_FILE_NAME_CSTR) < 0) {
        aids_log(AIDS_ERROR, "Failed to build task file path for printing: %s", strerror(errno));
        return;
    }

    print_file_path(path_buffer);

    printf(": [PRIORITY: %u, KIND: " SS_Fmt ", FLOW STEP: " SS_Fmt ", TAGS: ",
           task.meta.priority,
           SS_Arg(Task_Kind_Strings[task.meta.kind]),
           SS_Arg(Flow_Step_Strings[task.meta.flow_step]));
    for (size_t i = 0; i < task.meta.tags.count; ++i) {
        Aids_String_Slice *tag = NULL;
        AIDS_ASSERT(aids_array_get(&task.meta.tags, i, (void **)&tag) == AIDS_OK,
                   "Failed to get tag at index %zu: %s", i, aids_failure_reason());
        printf(SS_Fmt, SS_Arg(*tag));
        if (i < task.meta.tags.count - 1) {
            printf(", ");
        }
    }
    printf("] " SS_Fmt "\n", SS_Arg(task.title));
}

// Prints the full task: a clickable path header followed by the task fields and
// description body, mirroring the on-disk TASK.md layout.
static Aids_Result task_print_full(Aids_String_Slice task_file_path, Task task) {
    char path_buffer[PATH_MAX];
    if (snprintf(path_buffer, sizeof(path_buffer), SS_Fmt, SS_Arg(task_file_path)) < 0) {
        aids_log(AIDS_ERROR, "Failed to build task file path for printing: %s", strerror(errno));
        return AIDS_ERR;
    }

    print_file_path(path_buffer);
    printf("\n\n");

    // Print what would be written back, so `show` can never drift from the
    // on-disk format the serializer defines.
    Aids_String_Slice serialized = {0};
    if (task_serialize(task, &serialized) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to render task: %s", aids_failure_reason());
        return AIDS_ERR;
    }
    printf(SS_Fmt, SS_Arg(serialized));
    if (serialized.len == 0 || serialized.str[serialized.len - 1] != '\n') {
        printf("\n");
    }
    AIDS_FREE(serialized.str);
    return AIDS_OK;
}

static int main_show(const Tatr_Context *ctx) {
    int result = 0;
    Argparse_Parser parser = {0};
    Task task = {0};
    boolean task_initialized = false;
    Aids_String_Slice tasks_dir = {0};
    Aids_String_Slice task_file_path = {0};

    argparse_parser_init(&parser, "tatr show", "Show a single task by ID", TATR_VERSION);

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'I',
        .long_name = "id",
        .description = "Task ID (format YYYYMMDD-HHMMSS)",
        .type = ARGUMENT_TYPE_POSITIONAL,
        .required = 1
    });

    if (argparse_parse(&parser, ctx->argc, ctx->argv) != ARG_OK) {
        return_defer(1);
    }

    char *id_str = argparse_get_value(&parser, "id");
    if (id_str == NULL) {
        aids_log(AIDS_ERROR, "Missing task ID");
        return_defer(1);
    }
    Aids_String_Slice id = aids_string_slice_from_cstr(id_str);

    if (task_resolve(&ctx->cwd, &id, &tasks_dir, &task_file_path) != AIDS_OK) {
        return_defer(1);
    }

    task_init_empty(&task);
    task_initialized = true;

    if (task_load(&task_file_path, &task) != AIDS_OK) {
        return_defer(1);
    }

    if (task_print_full(task_file_path, task) != AIDS_OK) {
        return_defer(1);
    }

defer:
    if (task_initialized) {
        task_cleanup(&task);
    }
    if (tasks_dir.str != NULL) {
        AIDS_FREE(tasks_dir.str);
    }
    if (task_file_path.str != NULL) {
        AIDS_FREE(task_file_path.str);
    }
    argparse_parser_free(&parser);
    return result;
}

static int main_edit(const Tatr_Context *ctx) {
    int result = 0;
    Argparse_Parser parser = {0};
    Task task = {0};
    boolean task_initialized = false;
    Aids_String_Slice tasks_dir = {0};
    Aids_String_Slice task_file_path = {0};

    if (argparse_reject_retired_workflow_flags(ctx)) {
        return_defer(1);
    }

    argparse_parser_init(&parser, "tatr edit", "Update fields of an existing task", TATR_VERSION);

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'I',
        .long_name = "id",
        .description = "Task ID (format YYYYMMDD-HHMMSS)",
        .type = ARGUMENT_TYPE_POSITIONAL,
        .required = 1
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'T',
        .long_name = "title",
        .description = "New task title",
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'p',
        .long_name = "priority",
        .description = "New task priority",
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 't',
        .long_name = "tags",
        .description = "New task tags (replaces existing, comma-separated)",
        .type = ARGUMENT_TYPE_VALUE_ARRAY,
        .required = 0
    });

    argparse_add_v2_meta_arguments(&parser);

    if (argparse_parse(&parser, ctx->argc, ctx->argv) != ARG_OK) {
        return_defer(1);
    }

    char *id_str = argparse_get_value(&parser, "id");
    if (id_str == NULL) {
        aids_log(AIDS_ERROR, "Missing task ID");
        return_defer(1);
    }
    Aids_String_Slice id = aids_string_slice_from_cstr(id_str);

    if (task_resolve(&ctx->cwd, &id, &tasks_dir, &task_file_path) != AIDS_OK) {
        return_defer(1);
    }

    task_init_empty(&task);
    task_initialized = true;

    if (task_load(&task_file_path, &task) != AIDS_OK) {
        return_defer(1);
    }

    // Apply only the fields that were provided; leave everything else, including
    // the description body, untouched.
    char *title = argparse_get_value(&parser, "title");
    if (title != NULL) {
        task.title = aids_string_slice_from_cstr(title);
    }

    char *priority_str = argparse_get_value(&parser, "priority");
    if (priority_str != NULL) {
        long priority;
        Aids_String_Slice priority_slice = aids_string_slice_from_cstr(priority_str);
        if (aids_string_slice_atol(&priority_slice, &priority, 10)) {
            if (priority >= 0) {
                task.meta.priority = (unsigned int)priority;
            } else {
                aids_log(AIDS_ERROR, "Priority must be a non-negative number");
                return_defer(1);
            }
        } else {
            aids_log(AIDS_ERROR, "Invalid priority value: %s", priority_str);
            return_defer(1);
        }
    }

    if (!task_apply_v2_meta_arguments(&parser, &task)) {
        return_defer(1);
    }

    char *tags[ARGPARSE_CAPACITY];
    unsigned long tag_count = argparse_get_values(&parser, "tags", tags);
    if (tag_count > 0) {
        task.meta.tags.count = 0; // Replace existing tags
        for (unsigned long i = 0; i < tag_count; ++i) {
            Aids_String_Slice tag = aids_string_slice_from_cstr(tags[i]);
            if (aids_array_append(&task.meta.tags, &tag) != AIDS_OK) {
                aids_log(AIDS_ERROR, "Failed to append tag: %s", aids_failure_reason());
                return_defer(1);
            }
        }
    }

    if (task_save(&task_file_path, &task) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to save task"); // task_save logged the cause
        return_defer(1);
    }

    printf("Task updated successfully: " SS_Fmt "\n", SS_Arg(id));

defer:
    if (task_initialized) {
        task_cleanup(&task);
    }
    if (tasks_dir.str != NULL) {
        AIDS_FREE(tasks_dir.str);
    }
    if (task_file_path.str != NULL) {
        AIDS_FREE(task_file_path.str);
    }
    argparse_parser_free(&parser);
    return result;
}

static void cleanup_string_slice_array(Aids_Array *array) {
    if (array == NULL) {
        return;
    }

    for (size_t i = 0; i < array->count; ++i) {
        Aids_String_Slice *slice = NULL;
        if (aids_array_get(array, i, (void **)&slice) == AIDS_OK && slice->str != NULL) {
            AIDS_FREE(slice->str);
        }
    }
    aids_array_free(array);
}

static int main_rm(const Tatr_Context *ctx) {
    int result = 0;
    Argparse_Parser parser = {0};
    Aids_String_Slice tasks_dir = {0};
    Aids_String_Slice task_file_path = {0};
    Aids_String_Slice task_dir = {0};
    Aids_Array entries = {0};
    boolean entries_initialized = false;

    argparse_parser_init(&parser, "tatr rm", "Remove a task by ID", TATR_VERSION);

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'I',
        .long_name = "id",
        .description = "Task ID (format YYYYMMDD-HHMMSS)",
        .type = ARGUMENT_TYPE_POSITIONAL,
        .required = 1
    });

    if (argparse_parse(&parser, ctx->argc, ctx->argv) != ARG_OK) {
        return_defer(1);
    }

    char *id_str = argparse_get_value(&parser, "id");
    if (id_str == NULL) {
        aids_log(AIDS_ERROR, "Missing task ID");
        return_defer(1);
    }
    Aids_String_Slice id = aids_string_slice_from_cstr(id_str);

    if (task_resolve(&ctx->cwd, &id, &tasks_dir, &task_file_path) != AIDS_OK) {
        return_defer(1);
    }

    if (task_dir_path_build(&tasks_dir, &id, &task_dir) != AIDS_OK) {
        return_defer(1);
    }

    // Remove every regular entry inside the (validated) task directory, then the
    // directory itself. task_resolve has already confirmed the HUID is
    // well-formed and the directory exists, so we only ever touch tasks/<id>/.
    aids_array_init(&entries, sizeof(Aids_String_Slice));
    entries_initialized = true;
    if (aids_io_listdir(&task_dir, &entries) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to list task directory: %s", aids_failure_reason());
        return_defer(1);
    }

    for (size_t i = 0; i < entries.count; ++i) {
        Aids_String_Slice *name = NULL;
        if (aids_array_get(&entries, i, (void **)&name) != AIDS_OK) {
            continue;
        }

        // Skip the "." and ".." directory entries.
        if ((name->len == 1 && name->str[0] == '.') ||
            (name->len == 2 && name->str[0] == '.' && name->str[1] == '.')) {
            continue;
        }

        char entry_path[PATH_MAX];
        // Truncation matters here: this path is passed to unlink, so a
        // silently shortened one would name a different file.
        int entry_written = snprintf(entry_path, sizeof(entry_path), SS_Fmt "/" SS_Fmt, SS_Arg(task_dir), SS_Arg(*name));
        if (entry_written < 0 || (size_t)entry_written >= sizeof(entry_path)) {
            aids_log(AIDS_ERROR, "Failed to build entry path for '" SS_Fmt "': path too long", SS_Arg(*name));
            return_defer(1);
        }

        if (unlink(entry_path) != 0) {
            aids_log(AIDS_ERROR, "Failed to remove '%s': %s", entry_path, strerror(errno));
            return_defer(1);
        }
    }

    char dir_path[PATH_MAX];
    if (snprintf(dir_path, sizeof(dir_path), SS_Fmt, SS_Arg(task_dir)) < 0) {
        aids_log(AIDS_ERROR, "Failed to build directory path: %s", strerror(errno));
        return_defer(1);
    }

    if (rmdir(dir_path) != 0) {
        aids_log(AIDS_ERROR, "Failed to remove task directory '%s': %s", dir_path, strerror(errno));
        return_defer(1);
    }

    printf("Task removed successfully: " SS_Fmt "\n", SS_Arg(id));

defer:
    if (entries_initialized) {
        cleanup_string_slice_array(&entries);
    }
    if (tasks_dir.str != NULL) {
        AIDS_FREE(tasks_dir.str);
    }
    if (task_file_path.str != NULL) {
        AIDS_FREE(task_file_path.str);
    }
    if (task_dir.str != NULL) {
        AIDS_FREE(task_dir.str);
    }
    argparse_parser_free(&parser);
    return result;
}

typedef struct {
    Aids_String_Slice huid;
    Task task;
} Task_Entry;

typedef enum {
    Sort_By_CREATED,
    Sort_By_PRIORITY,
    Sort_By_TITLE
} Sort_By;

static Aids_String_Slice Sort_By_Strings[] = {
    [Sort_By_CREATED] = (Aids_String_Slice) { .str = (unsigned char *)"created", .len = 7 },
    [Sort_By_PRIORITY] = (Aids_String_Slice) { .str = (unsigned char *)"priority", .len = 8 },
    [Sort_By_TITLE] = (Aids_String_Slice) { .str = (unsigned char *)"title", .len = 5 }
};

static Sort_By sort_by_from_string(const Aids_String_Slice *slice) {
    for (size_t i = 0; i < sizeof(Sort_By_Strings) / sizeof(Sort_By_Strings[0]); ++i) {
        if (aids_string_slice_compare(slice, &Sort_By_Strings[i]) == 0) {
            return (Sort_By)i;
        }
    }

    return Sort_By_CREATED; // Default to CREATED if not found
}

static int tasks_compare_created(const void *a, const void *b) {
    const Task_Entry *entry_a = (const Task_Entry *)a;
    const Task_Entry *entry_b = (const Task_Entry *)b;
    return aids_string_slice_compare(&entry_a->huid, &entry_b->huid);
}

static int tasks_compare_priority(const void *a, const void *b) {
    const Task_Entry *entry_a = (const Task_Entry *)a;
    const Task_Entry *entry_b = (const Task_Entry *)b;

    // Descending order (higher priority first)
    if (entry_a->task.meta.priority < entry_b->task.meta.priority) {
        return 1;
    } else if (entry_a->task.meta.priority > entry_b->task.meta.priority) {
        return -1;
    } else {
        return 0;
    }
}

static int tasks_compare_title(const void *a, const void *b) {
    const Task_Entry *entry_a = (const Task_Entry *)a;
    const Task_Entry *entry_b = (const Task_Entry *)b;
    return aids_string_slice_compare(&entry_a->task.title, &entry_b->task.title);
}

typedef int (*Task_Compare_Fn)(const void *, const void *);

static Task_Compare_Fn get_task_compare_fn(Sort_By sort_by) {
    switch (sort_by) {
        case Sort_By_CREATED:
            return tasks_compare_created;
        case Sort_By_PRIORITY:
            return tasks_compare_priority;
        case Sort_By_TITLE:
            return tasks_compare_title;
        default:
            return tasks_compare_created; // Default to CREATED
    }
}

typedef struct {
    Aids_String_Slice project_dir;
    Aids_Array tasks; /* Task_Entry */
} Project_Tasks;

static void project_tasks_cleanup(Project_Tasks *project_tasks) {
    if (project_tasks == NULL) {
        return;
    }

    // NOTE: project_dir is NOT freed here because it's owned by the project_dirs array

    for (size_t i = 0; i < project_tasks->tasks.count; ++i) {
        Task_Entry *entry = NULL;
        if (aids_array_get(&project_tasks->tasks, i, (void **)&entry) == AIDS_OK) {
            task_cleanup(&entry->task);
            if (entry->huid.str != NULL) {
                AIDS_FREE(entry->huid.str);
            }
        }
    }
    aids_array_free(&project_tasks->tasks);
}

static Aids_Result find_tasks_dirs_recursive(const Aids_String_Slice *root_dir, Aids_Array *tasks_dirs) {
    Aids_Result result = AIDS_OK;
    Aids_Array entries = {0};
    Aids_String_Builder path_sb = {0};
    Aids_String_Slice candidate_path = {0};

    aids_string_builder_init(&path_sb);
    if (aids_string_builder_append(&path_sb, SS_Fmt "/" SS_Fmt,
                                   SS_Arg(*root_dir), SS_Arg(TASKS_PATH)) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to build candidate tasks directory path: %s", aids_failure_reason());
        aids_string_builder_free(&path_sb);
        return_defer(AIDS_ERR);
    }
    aids_string_builder_to_slice(&path_sb, &candidate_path);

    if (aids_io_isdir(&candidate_path)) {
        Aids_String_Slice root_copy = {0};
        Aids_String_Builder root_sb = {0};
        aids_string_builder_init(&root_sb);
        if (aids_string_builder_append(&root_sb, SS_Fmt, SS_Arg(*root_dir)) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to copy root directory path: %s", aids_failure_reason());
            aids_string_builder_free(&root_sb);
            AIDS_FREE(candidate_path.str);
            return_defer(AIDS_ERR);
        }
        aids_string_builder_to_slice(&root_sb, &root_copy);

        if (aids_array_append(tasks_dirs, &root_copy) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to append tasks directory: %s", aids_failure_reason());
            AIDS_FREE(root_copy.str);
            AIDS_FREE(candidate_path.str);
            return_defer(AIDS_ERR);
        }
    }
    AIDS_FREE(candidate_path.str);

    aids_array_init(&entries, sizeof(Aids_String_Slice));
    if (aids_io_listdir(root_dir, &entries) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to list directory '" SS_Fmt "': %s",
                SS_Arg(*root_dir), aids_failure_reason());
        cleanup_string_slice_array(&entries);
        return_defer(AIDS_ERR);
    }

    for (size_t i = 0; i < entries.count; ++i) {
        Aids_String_Slice *entry = NULL;
        if (aids_array_get(&entries, i, (void **)&entry) != AIDS_OK) {
            continue;
        }

        if (entry->len > 0 && entry->str[0] == '.') {
            continue;
        }

        static const char *skip_dirs[] = {"node_modules", "target", "build", "dist", ".git"};
        boolean should_skip = false;
        for (size_t j = 0; j < sizeof(skip_dirs) / sizeof(skip_dirs[0]); ++j) {
            Aids_String_Slice skip_dir = aids_string_slice_from_cstr((char *)skip_dirs[j]);
            if (aids_string_slice_compare(entry, &skip_dir) == 0) {
                should_skip = true;
                break;
            }
        }
        if (should_skip) {
            continue;
        }

        Aids_String_Builder subdir_sb = {0};
        Aids_String_Slice subdir_path = {0};
        aids_string_builder_init(&subdir_sb);
        if (aids_string_builder_append(&subdir_sb, SS_Fmt "/" SS_Fmt,
                                       SS_Arg(*root_dir), SS_Arg(*entry)) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to build subdirectory path: %s", aids_failure_reason());
            aids_string_builder_free(&subdir_sb);
            continue;
        }
        aids_string_builder_to_slice(&subdir_sb, &subdir_path);

        if (aids_io_isdir(&subdir_path)) {
            if (find_tasks_dirs_recursive(&subdir_path, tasks_dirs) != AIDS_OK) {
                aids_log(AIDS_WARNING, "Failed to search directory '" SS_Fmt "': %s",
                        SS_Arg(subdir_path), aids_failure_reason());
            }
        }
        AIDS_FREE(subdir_path.str);
    }

    cleanup_string_slice_array(&entries);

defer:
    return result;
}

// Loads every well-formed task in a directory. A record that does not parse is
// SKIPPED, not fatal: one bad file must not hide the rest of the backlog, and
// listing is how a user finds the bad file in the first place. *skipped counts
// them so the caller can still exit non-zero.
static Aids_Result load_tasks_from_dir(const Aids_String_Slice *tasks_dir,
                                       Aids_Array *tasks,
                                       Sort_By sort_by,
                                       size_t *skipped) {
    Aids_Result result = AIDS_OK;
    Aids_Array tasks_files = {0};

    aids_array_init(&tasks_files, sizeof(Aids_String_Slice));
    if (aids_io_listdir(tasks_dir, &tasks_files) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to list tasks directory: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }

    for (size_t i = 0; i < tasks_files.count; ++i) {
        Aids_String_Slice *huid = NULL;
        if (aids_array_get(&tasks_files, i, (void **)&huid) != AIDS_OK) {
            continue;
        }

        if (!ishuid(huid)) {
            continue;
        }

        Aids_String_Slice task_file_path = {0};
        if (task_file_path_build(tasks_dir, huid, &task_file_path) != AIDS_OK) {
            cleanup_string_slice_array(&tasks_files);
            return_defer(AIDS_ERR);
        }

        Task task = {0};
        if (task_load(&task_file_path, &task) != AIDS_OK) {
            aids_log(AIDS_WARNING, "Skipping unreadable task '" SS_Fmt "'", SS_Arg(*huid));
            AIDS_FREE(task_file_path.str);
            if (skipped != NULL) {
                (*skipped)++;
            }
            continue;
        }
        AIDS_FREE(task_file_path.str);

        // Make a copy of the HUID since tasks_files will be cleaned up
        Aids_String_Slice huid_copy = {0};
        Aids_String_Builder huid_sb = {0};
        aids_string_builder_init(&huid_sb);
        if (aids_string_builder_append(&huid_sb, SS_Fmt, SS_Arg(*huid)) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to copy HUID: %s", aids_failure_reason());
            task_cleanup(&task);
            aids_string_builder_free(&huid_sb);
            cleanup_string_slice_array(&tasks_files);
            return_defer(AIDS_ERR);
        }
        aids_string_builder_to_slice(&huid_sb, &huid_copy);

        Task_Entry entry = {
            .huid = huid_copy,
            .task = task
        };

        if (aids_array_append(tasks, &entry) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to append task data: %s", aids_failure_reason());
            task_cleanup(&task);
            AIDS_FREE(huid_copy.str);
            cleanup_string_slice_array(&tasks_files);
            return_defer(AIDS_ERR);
        }
    }

    aids_array_sort(tasks, get_task_compare_fn(sort_by));

    cleanup_string_slice_array(&tasks_files);

defer:
    return result;
}

static Aids_Result find_current_tasks_dir(const Aids_String_Slice *cwd, Aids_Array *tasks_dirs) {
    Aids_Result result = AIDS_OK;
    Aids_String_Slice project_dir = {0};
    Aids_String_Builder project_dir_sb = {0};
    Aids_String_Slice found_tasks_dir = {0};
    boolean project_dir_allocated = false;

    if (tasks_dir_path_build(cwd, &found_tasks_dir) != AIDS_OK) {
        return_defer(AIDS_ERR);
    }

    Aids_String_Slice project_dir_slice = found_tasks_dir;
    if (project_dir_slice.len >= TASKS_PATH.len + 1) {
        project_dir_slice.len -= (TASKS_PATH.len + 1);
    }

    aids_string_builder_init(&project_dir_sb);
    if (aids_string_builder_append(&project_dir_sb, SS_Fmt, SS_Arg(project_dir_slice)) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to copy project directory: %s", aids_failure_reason());
        aids_string_builder_free(&project_dir_sb);
        return_defer(AIDS_ERR);
    }
    aids_string_builder_to_slice(&project_dir_sb, &project_dir);
    project_dir_allocated = true;

    if (aids_array_append(tasks_dirs, &project_dir) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to append project directory: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }
    project_dir_allocated = false; // Ownership transferred to array

defer:
    if (found_tasks_dir.str != NULL) {
        AIDS_FREE(found_tasks_dir.str);
    }
    if (project_dir_allocated && project_dir.str != NULL) {
        AIDS_FREE(project_dir.str);
    }
    return result;
}

static Aids_String_Slice FILTER_KEYWORD_EQ = (Aids_String_Slice) { .str = (unsigned char *)"eq", .len = 2 };
static Aids_String_Slice FILTER_KEYWORD_IN = (Aids_String_Slice) { .str = (unsigned char *)"in", .len = 2 };
static Aids_String_Slice FILTER_KEYWORD_CONTAINS = (Aids_String_Slice) { .str = (unsigned char *)"contains", .len = 8 };
static Aids_String_Slice FILTER_KEYWORD_AND = (Aids_String_Slice) { .str = (unsigned char *)"and", .len = 3 };
static Aids_String_Slice FILTER_KEYWORD_OR = (Aids_String_Slice) { .str = (unsigned char *)"or", .len = 2 };
static Aids_String_Slice FILTER_KEYWORD_NOT = (Aids_String_Slice) { .str = (unsigned char *)"not", .len = 3 };

typedef struct {
    unsigned long index;
} Tatr_Token_Info;

typedef enum {
    TATR_FILTER_TOKEN_KIND_EOF,
    TATR_FILTER_TOKEN_KIND_EQ,
    TATR_FILTER_TOKEN_KIND_IN,
    TATR_FILTER_TOKEN_KIND_CONTAINS,
    TATR_FILTER_TOKEN_KIND_AND,
    TATR_FILTER_TOKEN_KIND_OR,
    TATR_FILTER_TOKEN_KIND_NOT,
    TATR_FILTER_TOKEN_KIND_LPAREN,
    TATR_FILTER_TOKEN_KIND_RPAREN,
    TATR_FILTER_TOKEN_KIND_LBRACKET,
    TATR_FILTER_TOKEN_KIND_RBRACKET,
    TATR_FILTER_TOKEN_KIND_COMMA,
    TATR_FILTER_TOKEN_KIND_FIELD,
    TATR_FILTER_TOKEN_KIND_IDENTIFIER,
    TATR_FILTER_TOKEN_KIND_INVALID
} Tatr_Filter_Token_Kind;

typedef struct {
    Tatr_Filter_Token_Kind kind;
    Aids_String_Slice text;
    Tatr_Token_Info info;
} Tatr_Filter_Token;

typedef struct {
    Aids_String_Slice input;
    unsigned long pos;
    unsigned long read_pos;
    char ch;
} Tatr_Filter_Lexer;

// Convert index position to line and column
static void tatr_filter_lexer_position_info(Tatr_Filter_Lexer *lexer, unsigned long index, unsigned long *line, unsigned long *column) {
    *line = 1;
    *column = 1;

    for (unsigned long i = 0; i < index && i < lexer->input.len; ++i) {
        if (lexer->input.str[i] == '\n') {
            (*line)++;
            *column = 1;
        } else {
            (*column)++;
        }
    }
}

static char tatr_filter_lexer_peek(Tatr_Filter_Lexer *lexer) {
    if (lexer->read_pos >= lexer->input.len) {
        return EOF;
    }
    return lexer->input.str[lexer->read_pos];
}

static char tatr_filter_lexer_read(Tatr_Filter_Lexer *lexer) {
    lexer->ch = tatr_filter_lexer_peek(lexer);

    lexer->pos = lexer->read_pos;
    lexer->read_pos++;

    return lexer->ch;
}

static void tatr_filter_lexer_skip_whitespace(Tatr_Filter_Lexer *lexer) {
    while (isspace(lexer->ch)) {
        tatr_filter_lexer_read(lexer);
    }
}

// Characters allowed inside a bare literal after it has started with an
// alnum or '_'. Version-style tags need '.' and '-' (e.g. v0.1.0,
// release-candidate). These do not make '.' or '-' valid standalone tokens;
// they only continue a literal that is already in progress.
static int tatr_filter_is_literal_char(char ch) {
    return isalnum((unsigned char)ch) || ch == '_' || ch == '.' || ch == '-';
}

static void tatr_filter_lexer_init(Tatr_Filter_Lexer *lexer, Aids_String_Slice input) {
    lexer->input = input;
    lexer->pos = 0;
    lexer->read_pos = 0;
    lexer->ch = 0;

    tatr_filter_lexer_read(lexer);
}

static Aids_Result tatr_filter_lexer_next(Tatr_Filter_Lexer *lexer, Tatr_Filter_Token *token) {
    Aids_Result result = AIDS_OK;
    tatr_filter_lexer_skip_whitespace(lexer);

    if (lexer->ch == EOF || lexer->ch == '\0') {
        token->kind = TATR_FILTER_TOKEN_KIND_EOF;
        token->text = (Aids_String_Slice){0};
        token->info.index = lexer->pos;
        return_defer(AIDS_OK);
    }

    unsigned long start = lexer->pos;

    switch (lexer->ch) {
        case '(':
            token->kind = TATR_FILTER_TOKEN_KIND_LPAREN;
            token->text = (Aids_String_Slice){0};
            token->info.index = start;
            tatr_filter_lexer_read(lexer);
            return_defer(AIDS_OK);
        case ')':
            token->kind = TATR_FILTER_TOKEN_KIND_RPAREN;
            token->text = (Aids_String_Slice){0};
            token->info.index = start;
            tatr_filter_lexer_read(lexer);
            return_defer(AIDS_OK);
        case '[':
            token->kind = TATR_FILTER_TOKEN_KIND_LBRACKET;
            token->text = (Aids_String_Slice){0};
            token->info.index = start;
            tatr_filter_lexer_read(lexer);
            return_defer(AIDS_OK);
        case ']':
            token->kind = TATR_FILTER_TOKEN_KIND_RBRACKET;
            token->text = (Aids_String_Slice){0};
            token->info.index = start;
            tatr_filter_lexer_read(lexer);
            return_defer(AIDS_OK);
        case ',':
            token->kind = TATR_FILTER_TOKEN_KIND_COMMA;
            token->text = (Aids_String_Slice){0};
            token->info.index = start;
            tatr_filter_lexer_read(lexer);
            return_defer(AIDS_OK);
    }

    if (lexer->ch == ':') {
        unsigned long field_start = lexer->pos;
        tatr_filter_lexer_read(lexer); // consume ':'
        start = lexer->pos;

        if (!isalpha(lexer->ch) && lexer->ch != '_') {
            token->kind = TATR_FILTER_TOKEN_KIND_INVALID;
            token->text = aids_string_slice_from_parts(lexer->input.str + field_start, 1);
            token->info.index = field_start;
            return_defer(AIDS_ERR);
        }

        while (isalnum(lexer->ch) || lexer->ch == '_') {
            tatr_filter_lexer_read(lexer);
        }

        token->kind = TATR_FILTER_TOKEN_KIND_FIELD;
        token->text = aids_string_slice_from_parts(lexer->input.str + start, lexer->pos - start);
        token->info.index = field_start;
        return_defer(AIDS_OK);
    }

    if (isalpha(lexer->ch) || lexer->ch == '_' || isdigit(lexer->ch)) {
        while (tatr_filter_is_literal_char(lexer->ch)) {
            tatr_filter_lexer_read(lexer);
        }

        Aids_String_Slice text = aids_string_slice_from_parts(lexer->input.str + start, lexer->pos - start);

        if (aids_string_slice_compare(&text, &FILTER_KEYWORD_EQ) == 0) {
            token->kind = TATR_FILTER_TOKEN_KIND_EQ;
            token->text = (Aids_String_Slice){0};
        } else if (aids_string_slice_compare(&text, &FILTER_KEYWORD_IN) == 0) {
            token->kind = TATR_FILTER_TOKEN_KIND_IN;
            token->text = (Aids_String_Slice){0};
        } else if (aids_string_slice_compare(&text, &FILTER_KEYWORD_CONTAINS) == 0) {
            token->kind = TATR_FILTER_TOKEN_KIND_CONTAINS;
            token->text = (Aids_String_Slice){0};
        } else if (aids_string_slice_compare(&text, &FILTER_KEYWORD_AND) == 0) {
            token->kind = TATR_FILTER_TOKEN_KIND_AND;
            token->text = (Aids_String_Slice){0};
        } else if (aids_string_slice_compare(&text, &FILTER_KEYWORD_OR) == 0) {
            token->kind = TATR_FILTER_TOKEN_KIND_OR;
            token->text = (Aids_String_Slice){0};
        } else if (aids_string_slice_compare(&text, &FILTER_KEYWORD_NOT) == 0) {
            token->kind = TATR_FILTER_TOKEN_KIND_NOT;
            token->text = (Aids_String_Slice){0};
        } else {
            token->kind = TATR_FILTER_TOKEN_KIND_IDENTIFIER;
            token->text = text;
        }

        token->info.index = start;
        return_defer(AIDS_OK);
    }

    token->kind = TATR_FILTER_TOKEN_KIND_INVALID;
    token->text = aids_string_slice_from_parts(lexer->input.str + start, 1);
    token->info.index = start;
    tatr_filter_lexer_read(lexer);
    return_defer(AIDS_ERR);

defer:
    return result;
}

// AST Node Types
typedef enum {
    TATR_FILTER_AST_NODE_KIND_BINARY_OP,
    TATR_FILTER_AST_NODE_KIND_UNARY_OP,
    TATR_FILTER_AST_NODE_KIND_COMPARISON,
    TATR_FILTER_AST_NODE_KIND_FIELD,
    TATR_FILTER_AST_NODE_KIND_IDENTIFIER,
    TATR_FILTER_AST_NODE_KIND_LIST,
} Tatr_Filter_Ast_Node_Kind;

typedef enum {
    TATR_FILTER_BINARY_OP_AND,
    TATR_FILTER_BINARY_OP_OR,
} Tatr_Filter_Binary_Op;

typedef enum {
    TATR_FILTER_UNARY_OP_NOT,
} Tatr_Filter_Unary_Op;

typedef enum {
    TATR_FILTER_COMPARISON_OP_EQ,
    TATR_FILTER_COMPARISON_OP_IN,
    TATR_FILTER_COMPARISON_OP_CONTAINS,
} Tatr_Filter_Comparison_Op;

typedef struct Tatr_Filter_Ast_Node Tatr_Filter_Ast_Node;

typedef struct {
    Tatr_Filter_Binary_Op op;
    Tatr_Filter_Ast_Node *left;
    Tatr_Filter_Ast_Node *right;
} Tatr_Filter_Binary_Op_Node;

typedef struct {
    Tatr_Filter_Unary_Op op;
    Tatr_Filter_Ast_Node *operand;
} Tatr_Filter_Unary_Op_Node;

typedef struct {
    Tatr_Filter_Comparison_Op op;
    Tatr_Filter_Ast_Node *left;
    Tatr_Filter_Ast_Node *right;
} Tatr_Filter_Comparison_Node;

typedef struct {
    Aids_String_Slice name;
} Tatr_Filter_Field_Node;

typedef struct {
    Aids_String_Slice value;
} Tatr_Filter_Identifier_Node;

typedef struct {
    Aids_Array items; /* Tatr_Filter_Ast_Node* */
} Tatr_Filter_List_Node;

struct Tatr_Filter_Ast_Node {
    Tatr_Filter_Ast_Node_Kind kind;
    Tatr_Token_Info info;
    union {
        Tatr_Filter_Binary_Op_Node binary_op;
        Tatr_Filter_Unary_Op_Node unary_op;
        Tatr_Filter_Comparison_Node comparison;
        Tatr_Filter_Field_Node field;
        Tatr_Filter_Identifier_Node identifier;
        Tatr_Filter_List_Node list;
    } data;
};

// Parser
typedef struct {
    Tatr_Filter_Lexer lexer;
    Tatr_Filter_Token current;
    Tatr_Filter_Token peek;
    boolean has_error;
    char error_msg[256];
} Tatr_Filter_Parser;

static void tatr_filter_parser_init(Tatr_Filter_Parser *parser, Aids_String_Slice input) {
    tatr_filter_lexer_init(&parser->lexer, input);
    parser->has_error = false;
    parser->error_msg[0] = '\0';

    // Prime the parser with the first two tokens
    tatr_filter_lexer_next(&parser->lexer, &parser->current);
    tatr_filter_lexer_next(&parser->lexer, &parser->peek);
}

static void tatr_filter_parser_advance(Tatr_Filter_Parser *parser) {
    parser->current = parser->peek;
    tatr_filter_lexer_next(&parser->lexer, &parser->peek);
}

static const char* tatr_filter_token_kind_name(Tatr_Filter_Token_Kind kind) {
    switch (kind) {
        case TATR_FILTER_TOKEN_KIND_EOF: return "end of input";
        case TATR_FILTER_TOKEN_KIND_EQ: return "'eq'";
        case TATR_FILTER_TOKEN_KIND_IN: return "'in'";
        case TATR_FILTER_TOKEN_KIND_CONTAINS: return "'contains'";
        case TATR_FILTER_TOKEN_KIND_AND: return "'and'";
        case TATR_FILTER_TOKEN_KIND_OR: return "'or'";
        case TATR_FILTER_TOKEN_KIND_NOT: return "'not'";
        case TATR_FILTER_TOKEN_KIND_LPAREN: return "'('";
        case TATR_FILTER_TOKEN_KIND_RPAREN: return "')'";
        case TATR_FILTER_TOKEN_KIND_LBRACKET: return "'['";
        case TATR_FILTER_TOKEN_KIND_RBRACKET: return "']'";
        case TATR_FILTER_TOKEN_KIND_COMMA: return "','";
        case TATR_FILTER_TOKEN_KIND_FIELD: return "field";
        case TATR_FILTER_TOKEN_KIND_IDENTIFIER: return "identifier";
        case TATR_FILTER_TOKEN_KIND_INVALID: return "invalid token";
        default: return "unknown token";
    }
}

static void tatr_filter_parser_error(Tatr_Filter_Parser *parser, const char *message) {
    parser->has_error = true;
    unsigned long line, column;
    tatr_filter_lexer_position_info(&parser->lexer, parser->current.info.index, &line, &column);
    snprintf(parser->error_msg, sizeof(parser->error_msg),
             "line %lu, col %lu: %s", line, column, message);
}

static void tatr_filter_parser_error_fmt(Tatr_Filter_Parser *parser, const char *fmt, ...) {
    parser->has_error = true;
    unsigned long line, column;
    tatr_filter_lexer_position_info(&parser->lexer, parser->current.info.index, &line, &column);

    char temp[200];
    va_list args;
    va_start(args, fmt);
    vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);

    snprintf(parser->error_msg, sizeof(parser->error_msg), "line %lu, col %lu: %s", line, column, temp);
}

static boolean tatr_filter_parser_expect(Tatr_Filter_Parser *parser, Tatr_Filter_Token_Kind kind) {
    if (parser->current.kind != kind) {
        tatr_filter_parser_error_fmt(parser, "expected %s, got %s",
                                     tatr_filter_token_kind_name(kind),
                                     tatr_filter_token_kind_name(parser->current.kind));
        return false;
    }
    tatr_filter_parser_advance(parser);
    return true;
}

static Tatr_Filter_Ast_Node* tatr_filter_parse_expression(Tatr_Filter_Parser *parser);
static Tatr_Filter_Ast_Node* tatr_filter_parse_or_expression(Tatr_Filter_Parser *parser);
static Tatr_Filter_Ast_Node* tatr_filter_parse_and_expression(Tatr_Filter_Parser *parser);
static Tatr_Filter_Ast_Node* tatr_filter_parse_unary_expression(Tatr_Filter_Parser *parser);
static Tatr_Filter_Ast_Node* tatr_filter_parse_comparison(Tatr_Filter_Parser *parser);
static Tatr_Filter_Ast_Node* tatr_filter_parse_primary(Tatr_Filter_Parser *parser);
static Tatr_Filter_Ast_Node* tatr_filter_parse_list(Tatr_Filter_Parser *parser);
static void tatr_filter_ast_free(Tatr_Filter_Ast_Node *node);

// Parse a list: [ item1, item2, ... ]
static Tatr_Filter_Ast_Node* tatr_filter_parse_list(Tatr_Filter_Parser *parser) {
    Tatr_Filter_Ast_Node *node = malloc(sizeof(Tatr_Filter_Ast_Node));
    node->kind = TATR_FILTER_AST_NODE_KIND_LIST;
    node->info = parser->current.info;
    aids_array_init(&node->data.list.items, sizeof(Tatr_Filter_Ast_Node*));

    if (!tatr_filter_parser_expect(parser, TATR_FILTER_TOKEN_KIND_LBRACKET)) {
        free(node);
        return NULL;
    }

    // Empty list
    if (parser->current.kind == TATR_FILTER_TOKEN_KIND_RBRACKET) {
        tatr_filter_parser_advance(parser);
        return node;
    }

    // Parse list items
    while (true) {
        if (parser->current.kind != TATR_FILTER_TOKEN_KIND_IDENTIFIER) {
            tatr_filter_parser_error_fmt(parser, "expected identifier in list, got %s",
                                        tatr_filter_token_kind_name(parser->current.kind));
            aids_array_free(&node->data.list.items);
            free(node);
            return NULL;
        }

        Tatr_Filter_Ast_Node *item = malloc(sizeof(Tatr_Filter_Ast_Node));
        item->kind = TATR_FILTER_AST_NODE_KIND_IDENTIFIER;
        item->info = parser->current.info;
        item->data.identifier.value = parser->current.text;

        aids_array_append(&node->data.list.items, &item);
        tatr_filter_parser_advance(parser);

        if (parser->current.kind == TATR_FILTER_TOKEN_KIND_RBRACKET) {
            tatr_filter_parser_advance(parser);
            break;
        }

        if (parser->current.kind != TATR_FILTER_TOKEN_KIND_COMMA) {
            tatr_filter_parser_error_fmt(parser, "expected ',' or ']' in list, got %s",
                                        tatr_filter_token_kind_name(parser->current.kind));
            aids_array_free(&node->data.list.items);
            free(node);
            return NULL;
        }

        tatr_filter_parser_advance(parser); // consume comma
    }

    return node;
}

// Parse primary expressions: field, identifier, list, or parenthesized expression
static Tatr_Filter_Ast_Node* tatr_filter_parse_primary(Tatr_Filter_Parser *parser) {
    Tatr_Filter_Ast_Node *node = NULL;

    switch (parser->current.kind) {
        case TATR_FILTER_TOKEN_KIND_FIELD: {
            node = malloc(sizeof(Tatr_Filter_Ast_Node));
            node->kind = TATR_FILTER_AST_NODE_KIND_FIELD;
            node->info = parser->current.info;
            node->data.field.name = parser->current.text;
            tatr_filter_parser_advance(parser);
            return node;
        }

        case TATR_FILTER_TOKEN_KIND_IDENTIFIER: {
            node = malloc(sizeof(Tatr_Filter_Ast_Node));
            node->kind = TATR_FILTER_AST_NODE_KIND_IDENTIFIER;
            node->info = parser->current.info;
            node->data.identifier.value = parser->current.text;
            tatr_filter_parser_advance(parser);
            return node;
        }

        case TATR_FILTER_TOKEN_KIND_LBRACKET: {
            return tatr_filter_parse_list(parser);
        }

        case TATR_FILTER_TOKEN_KIND_LPAREN: {
            tatr_filter_parser_advance(parser); // consume '('
            node = tatr_filter_parse_expression(parser);
            if (!tatr_filter_parser_expect(parser, TATR_FILTER_TOKEN_KIND_RPAREN)) {
                return NULL;
            }
            return node;
        }

        default:
            tatr_filter_parser_error_fmt(parser, "expected field, identifier, list, or '(', got %s",
                                        tatr_filter_token_kind_name(parser->current.kind));
            return NULL;
    }
}

// Parse comparison: primary op primary
static Tatr_Filter_Ast_Node* tatr_filter_parse_comparison(Tatr_Filter_Parser *parser) {
    Tatr_Filter_Ast_Node *left = tatr_filter_parse_primary(parser);
    if (!left || parser->has_error) {
        return left;
    }

    Tatr_Filter_Comparison_Op op;
    switch (parser->current.kind) {
        case TATR_FILTER_TOKEN_KIND_EQ:
            op = TATR_FILTER_COMPARISON_OP_EQ;
            break;
        case TATR_FILTER_TOKEN_KIND_IN:
            op = TATR_FILTER_COMPARISON_OP_IN;
            break;
        case TATR_FILTER_TOKEN_KIND_CONTAINS:
            op = TATR_FILTER_COMPARISON_OP_CONTAINS;
            break;
        default:
            // Not a comparison, just return the primary expression
            return left;
    }

    Tatr_Token_Info op_info = parser->current.info;
    Tatr_Filter_Token_Kind op_kind = parser->current.kind;
    tatr_filter_parser_advance(parser);

    Tatr_Filter_Ast_Node *right = tatr_filter_parse_primary(parser);
    if (!right) {
        if (!parser->has_error) {
            tatr_filter_parser_error_fmt(parser, "expected value after %s operator",
                                        tatr_filter_token_kind_name(op_kind));
        }
        free(left);
        return NULL;
    }
    if (parser->has_error) {
        free(left);
        return NULL;
    }

    Tatr_Filter_Ast_Node *node = malloc(sizeof(Tatr_Filter_Ast_Node));
    node->kind = TATR_FILTER_AST_NODE_KIND_COMPARISON;
    node->info = op_info;
    node->data.comparison.op = op;
    node->data.comparison.left = left;
    node->data.comparison.right = right;

    return node;
}

// Parse unary expression: not term | term
static Tatr_Filter_Ast_Node* tatr_filter_parse_unary_expression(Tatr_Filter_Parser *parser) {
    if (parser->current.kind == TATR_FILTER_TOKEN_KIND_NOT) {
        Tatr_Token_Info info = parser->current.info;
        tatr_filter_parser_advance(parser);

        Tatr_Filter_Ast_Node *operand = tatr_filter_parse_unary_expression(parser);
        if (!operand) {
            if (!parser->has_error) {
                tatr_filter_parser_error(parser, "expected expression after 'not' operator");
            }
            return NULL;
        }
        if (parser->has_error) {
            return NULL;
        }

        Tatr_Filter_Ast_Node *node = malloc(sizeof(Tatr_Filter_Ast_Node));
        node->kind = TATR_FILTER_AST_NODE_KIND_UNARY_OP;
        node->info = info;
        node->data.unary_op.op = TATR_FILTER_UNARY_OP_NOT;
        node->data.unary_op.operand = operand;

        return node;
    }

    return tatr_filter_parse_comparison(parser);
}

// Parse AND expression: unary (and unary)*
static Tatr_Filter_Ast_Node* tatr_filter_parse_and_expression(Tatr_Filter_Parser *parser) {
    Tatr_Filter_Ast_Node *left = tatr_filter_parse_unary_expression(parser);
    if (!left || parser->has_error) {
        return left;
    }

    while (parser->current.kind == TATR_FILTER_TOKEN_KIND_AND) {
        Tatr_Token_Info info = parser->current.info;
        tatr_filter_parser_advance(parser);

        Tatr_Filter_Ast_Node *right = tatr_filter_parse_unary_expression(parser);
        if (!right) {
            if (!parser->has_error) {
                tatr_filter_parser_error(parser, "expected expression after 'and' operator");
            }
            free(left);
            return NULL;
        }
        if (parser->has_error) {
            free(left);
            return NULL;
        }

        Tatr_Filter_Ast_Node *node = malloc(sizeof(Tatr_Filter_Ast_Node));
        node->kind = TATR_FILTER_AST_NODE_KIND_BINARY_OP;
        node->info = info;
        node->data.binary_op.op = TATR_FILTER_BINARY_OP_AND;
        node->data.binary_op.left = left;
        node->data.binary_op.right = right;

        left = node;
    }

    return left;
}

// Parse OR expression: and (or and)*
static Tatr_Filter_Ast_Node* tatr_filter_parse_or_expression(Tatr_Filter_Parser *parser) {
    Tatr_Filter_Ast_Node *left = tatr_filter_parse_and_expression(parser);
    if (!left || parser->has_error) {
        return left;
    }

    while (parser->current.kind == TATR_FILTER_TOKEN_KIND_OR) {
        Tatr_Token_Info info = parser->current.info;
        tatr_filter_parser_advance(parser);

        Tatr_Filter_Ast_Node *right = tatr_filter_parse_and_expression(parser);
        if (!right) {
            if (!parser->has_error) {
                tatr_filter_parser_error(parser, "expected expression after 'or' operator");
            }
            free(left);
            return NULL;
        }
        if (parser->has_error) {
            free(left);
            return NULL;
        }

        Tatr_Filter_Ast_Node *node = malloc(sizeof(Tatr_Filter_Ast_Node));
        node->kind = TATR_FILTER_AST_NODE_KIND_BINARY_OP;
        node->info = info;
        node->data.binary_op.op = TATR_FILTER_BINARY_OP_OR;
        node->data.binary_op.left = left;
        node->data.binary_op.right = right;

        left = node;
    }

    return left;
}

// Parse expression (entry point for recursive descent)
static Tatr_Filter_Ast_Node* tatr_filter_parse_expression(Tatr_Filter_Parser *parser) {
    return tatr_filter_parse_or_expression(parser);
}

// Main parse function
static Tatr_Filter_Ast_Node* tatr_filter_parse(Aids_String_Slice input, char *error_msg, size_t error_msg_size) {
    Tatr_Filter_Parser parser = {0};
    tatr_filter_parser_init(&parser, input);

    // Check for empty input
    if (parser.current.kind == TATR_FILTER_TOKEN_KIND_EOF) {
        if (error_msg && error_msg_size > 0) {
            snprintf(error_msg, error_msg_size, "empty filter expression");
        }
        return NULL;
    }

    Tatr_Filter_Ast_Node *root = tatr_filter_parse_expression(&parser);

    if (parser.has_error) {
        if (error_msg && error_msg_size > 0) {
            snprintf(error_msg, error_msg_size, "%s", parser.error_msg);
        }
        return NULL;
    }

    if (parser.current.kind != TATR_FILTER_TOKEN_KIND_EOF) {
        if (error_msg && error_msg_size > 0) {
            unsigned long line, column;
            tatr_filter_lexer_position_info(&parser.lexer, parser.current.info.index, &line, &column);
            snprintf(error_msg, error_msg_size,
                     "line %lu, col %lu: unexpected %s after expression",
                     line, column, tatr_filter_token_kind_name(parser.current.kind));
        }
        tatr_filter_ast_free(root);
        return NULL;
    }

    return root;
}

// Type checker and interpreter

// Field type definitions
typedef enum {
    TATR_FILTER_FIELD_TYPE_STATUS,
    TATR_FILTER_FIELD_TYPE_TAGS,
    TATR_FILTER_FIELD_TYPE_PRIORITY,
    TATR_FILTER_FIELD_TYPE_TITLE,
    TATR_FILTER_FIELD_TYPE_KIND,
    TATR_FILTER_FIELD_TYPE_FLOW_STEP,
    TATR_FILTER_FIELD_TYPE_PLAN_STATUS,
    TATR_FILTER_FIELD_TYPE_PARENT,
    TATR_FILTER_FIELD_TYPE_DEPENDS,
    TATR_FILTER_FIELD_TYPE_UNKNOWN
} Tatr_Filter_Field_Type;

// Value type definitions
typedef enum {
    TATR_FILTER_VALUE_TYPE_STATUS,
    TATR_FILTER_VALUE_TYPE_STRING,
    TATR_FILTER_VALUE_TYPE_NUMBER,
    TATR_FILTER_VALUE_TYPE_LIST,
    TATR_FILTER_VALUE_TYPE_BOOLEAN,
    TATR_FILTER_VALUE_TYPE_UNKNOWN
} Tatr_Filter_Value_Type;

static Aids_String_Slice FIELD_NAME_STATUS = (Aids_String_Slice) { .str = (unsigned char *)"status", .len = 6 };
static Aids_String_Slice FIELD_NAME_TAGS = (Aids_String_Slice) { .str = (unsigned char *)"tags", .len = 4 };
static Aids_String_Slice FIELD_NAME_PRIORITY = (Aids_String_Slice) { .str = (unsigned char *)"priority", .len = 8 };
static Aids_String_Slice FIELD_NAME_TITLE = (Aids_String_Slice) { .str = (unsigned char *)"title", .len = 5 };
static Aids_String_Slice FIELD_NAME_KIND = (Aids_String_Slice) { .str = (unsigned char *)"kind", .len = 4 };
static Aids_String_Slice FIELD_NAME_FLOW_STEP = (Aids_String_Slice) { .str = (unsigned char *)"flow_step", .len = 9 };
static Aids_String_Slice FIELD_NAME_PLAN_STATUS = (Aids_String_Slice) { .str = (unsigned char *)"plan_status", .len = 11 };
static Aids_String_Slice FIELD_NAME_PARENT = (Aids_String_Slice) { .str = (unsigned char *)"parent", .len = 6 };
static Aids_String_Slice FIELD_NAME_DEPENDS = (Aids_String_Slice) { .str = (unsigned char *)"depends", .len = 7 };

// Get field type from field name
static Tatr_Filter_Field_Type tatr_filter_get_field_type(Aids_String_Slice field_name) {
    if (aids_string_slice_compare(&field_name, &FIELD_NAME_STATUS) == 0) {
        return TATR_FILTER_FIELD_TYPE_STATUS;
    }
    if (aids_string_slice_compare(&field_name, &FIELD_NAME_TAGS) == 0) {
        return TATR_FILTER_FIELD_TYPE_TAGS;
    }
    if (aids_string_slice_compare(&field_name, &FIELD_NAME_PRIORITY) == 0) {
        return TATR_FILTER_FIELD_TYPE_PRIORITY;
    }
    if (aids_string_slice_compare(&field_name, &FIELD_NAME_TITLE) == 0) {
        return TATR_FILTER_FIELD_TYPE_TITLE;
    }
    if (aids_string_slice_compare(&field_name, &FIELD_NAME_KIND) == 0) {
        return TATR_FILTER_FIELD_TYPE_KIND;
    }
    if (aids_string_slice_compare(&field_name, &FIELD_NAME_FLOW_STEP) == 0) {
        return TATR_FILTER_FIELD_TYPE_FLOW_STEP;
    }
    if (aids_string_slice_compare(&field_name, &FIELD_NAME_PLAN_STATUS) == 0) {
        return TATR_FILTER_FIELD_TYPE_PLAN_STATUS;
    }
    if (aids_string_slice_compare(&field_name, &FIELD_NAME_PARENT) == 0) {
        return TATR_FILTER_FIELD_TYPE_PARENT;
    }
    if (aids_string_slice_compare(&field_name, &FIELD_NAME_DEPENDS) == 0) {
        return TATR_FILTER_FIELD_TYPE_DEPENDS;
    }
    return TATR_FILTER_FIELD_TYPE_UNKNOWN;
}

// The enum-valued filter fields all behave identically: an identifier operand
// (or a list of them) that must name a value of the field's enum. Returns the
// value table for such a field, or NULL for the rest.
static const Aids_String_Slice *tatr_filter_enum_table(Tatr_Filter_Field_Type field_type,
                                                       size_t *count,
                                                       const char **field_label,
                                                       const char **values_hint) {
    switch (field_type) {
        case TATR_FILTER_FIELD_TYPE_STATUS:
            *count = ENUM_COUNT(Task_Status_Strings);
            *field_label = "status";
            *values_hint = STATUS_VALUES_CSTR;
            return Task_Status_Strings;
        case TATR_FILTER_FIELD_TYPE_KIND:
            *count = ENUM_COUNT(Task_Kind_Strings);
            *field_label = "kind";
            *values_hint = KIND_VALUES_CSTR;
            return Task_Kind_Strings;
        case TATR_FILTER_FIELD_TYPE_FLOW_STEP:
            *count = ENUM_COUNT(Flow_Step_Strings);
            *field_label = "flow step";
            *values_hint = FLOW_STEP_VALUES_CSTR;
            return Flow_Step_Strings;
        case TATR_FILTER_FIELD_TYPE_PLAN_STATUS:
            *count = ENUM_COUNT(Plan_Status_Strings);
            *field_label = "plan status";
            *values_hint = PLAN_STATUS_VALUES_CSTR;
            return Plan_Status_Strings;
        default:
            return NULL;
    }
}

// The enum a task actually carries for such a field.
static int tatr_filter_enum_value(Tatr_Filter_Field_Type field_type, const Task *task) {
    switch (field_type) {
        case TATR_FILTER_FIELD_TYPE_STATUS: return (int)task->meta.status;
        case TATR_FILTER_FIELD_TYPE_KIND: return (int)task->meta.kind;
        case TATR_FILTER_FIELD_TYPE_FLOW_STEP: return (int)task->meta.flow_step;
        case TATR_FILTER_FIELD_TYPE_PLAN_STATUS: return (int)task->meta.plan_status;
        default: return -1;
    }
}

// Type check the AST
static boolean tatr_filter_typecheck_node(Tatr_Filter_Ast_Node *node, Tatr_Filter_Lexer *lexer, char *error_msg, size_t error_msg_size);

static boolean tatr_filter_typecheck_comparison(Tatr_Filter_Ast_Node *node, Tatr_Filter_Lexer *lexer, char *error_msg, size_t error_msg_size) {
    Tatr_Filter_Ast_Node *left = node->data.comparison.left;
    Tatr_Filter_Ast_Node *right = node->data.comparison.right;

    // Left side must be a field
    if (left->kind != TATR_FILTER_AST_NODE_KIND_FIELD) {
        unsigned long line, column;
        tatr_filter_lexer_position_info(lexer, left->info.index, &line, &column);
        snprintf(error_msg, error_msg_size, "line %lu, col %lu: left side of comparison must be a field", line, column);
        return false;
    }

    Tatr_Filter_Field_Type field_type = tatr_filter_get_field_type(left->data.field.name);
    if (field_type == TATR_FILTER_FIELD_TYPE_UNKNOWN) {
        unsigned long line, column;
        tatr_filter_lexer_position_info(lexer, left->info.index, &line, &column);
        snprintf(error_msg, error_msg_size, "line %lu, col %lu: unknown field '" SS_Fmt "'",
                 line, column, SS_Arg(left->data.field.name));
        return false;
    }

    // Type check based on operator and field type
    Tatr_Filter_Comparison_Op op = node->data.comparison.op;

    size_t enum_count = 0;
    const char *enum_label = NULL;
    const char *enum_hint = NULL;
    const Aids_String_Slice *enum_table =
        tatr_filter_enum_table(field_type, &enum_count, &enum_label, &enum_hint);

    if (op == TATR_FILTER_COMPARISON_OP_EQ) {
        // eq: field eq value
        if (enum_table != NULL) {
            // Right side must be an identifier naming a value of the enum.
            if (right->kind != TATR_FILTER_AST_NODE_KIND_IDENTIFIER) {
                unsigned long line, column;
                tatr_filter_lexer_position_info(lexer, right->info.index, &line, &column);
                snprintf(error_msg, error_msg_size, "line %lu, col %lu: %s comparison requires an identifier (%s)",
                         line, column, enum_label, enum_hint);
                return false;
            }
            int value = 0;
            if (!enum_from_string(&right->data.identifier.value, enum_table, enum_count, &value)) {
                unsigned long line, column;
                tatr_filter_lexer_position_info(lexer, right->info.index, &line, &column);
                snprintf(error_msg, error_msg_size, "line %lu, col %lu: invalid %s value '" SS_Fmt "' (must be %s)",
                         line, column, enum_label, SS_Arg(right->data.identifier.value), enum_hint);
                return false;
            }
        } else if (field_type == TATR_FILTER_FIELD_TYPE_PARENT) {
            // Right side must be an identifier (a task ID).
            if (right->kind != TATR_FILTER_AST_NODE_KIND_IDENTIFIER) {
                unsigned long line, column;
                tatr_filter_lexer_position_info(lexer, right->info.index, &line, &column);
                snprintf(error_msg, error_msg_size, "line %lu, col %lu: parent comparison requires a task ID", line, column);
                return false;
            }
        } else if (field_type == TATR_FILTER_FIELD_TYPE_PRIORITY) {
            // Right side must be an identifier representing a number
            if (right->kind != TATR_FILTER_AST_NODE_KIND_IDENTIFIER) {
                unsigned long line, column;
                tatr_filter_lexer_position_info(lexer, right->info.index, &line, &column);
                snprintf(error_msg, error_msg_size, "line %lu, col %lu: priority comparison requires a number", line, column);
                return false;
            }
        } else if (field_type == TATR_FILTER_FIELD_TYPE_TITLE) {
            // Right side must be an identifier (string value)
            if (right->kind != TATR_FILTER_AST_NODE_KIND_IDENTIFIER) {
                unsigned long line, column;
                tatr_filter_lexer_position_info(lexer, right->info.index, &line, &column);
                snprintf(error_msg, error_msg_size, "line %lu, col %lu: title comparison requires a string value", line, column);
                return false;
            }
        } else {
            unsigned long line, column;
            tatr_filter_lexer_position_info(lexer, node->info.index, &line, &column);
            snprintf(error_msg, error_msg_size, "line %lu, col %lu: 'eq' operator not supported for field '" SS_Fmt "'",
                     line, column, SS_Arg(left->data.field.name));
            return false;
        }
    } else if (op == TATR_FILTER_COMPARISON_OP_IN) {
        // in: field in [list]
        if (enum_table != NULL) {
            // Right side must be a list of identifiers naming enum values.
            if (right->kind != TATR_FILTER_AST_NODE_KIND_LIST) {
                unsigned long line, column;
                tatr_filter_lexer_position_info(lexer, right->info.index, &line, &column);
                snprintf(error_msg, error_msg_size, "line %lu, col %lu: 'in' operator requires a list", line, column);
                return false;
            }
            for (unsigned long i = 0; i < right->data.list.items.count; i++) {
                Tatr_Filter_Ast_Node **item_ptr;
                aids_array_get(&right->data.list.items, i, (void**)&item_ptr);
                Tatr_Filter_Ast_Node *item = *item_ptr;

                int value = 0;
                if (!enum_from_string(&item->data.identifier.value, enum_table, enum_count, &value)) {
                    unsigned long line, column;
                    tatr_filter_lexer_position_info(lexer, item->info.index, &line, &column);
                    snprintf(error_msg, error_msg_size, "line %lu, col %lu: invalid %s value '" SS_Fmt "' in list",
                             line, column, enum_label, SS_Arg(item->data.identifier.value));
                    return false;
                }
            }
        } else if (field_type == TATR_FILTER_FIELD_TYPE_TAGS) {
            unsigned long line, column;
            tatr_filter_lexer_position_info(lexer, node->info.index, &line, &column);
            snprintf(error_msg, error_msg_size, "line %lu, col %lu: use 'contains' operator for tags, not 'in'", line, column);
            return false;
        } else {
            unsigned long line, column;
            tatr_filter_lexer_position_info(lexer, node->info.index, &line, &column);
            snprintf(error_msg, error_msg_size, "line %lu, col %lu: 'in' operator not supported for field '" SS_Fmt "'",
                     line, column, SS_Arg(left->data.field.name));
            return false;
        }
    } else if (op == TATR_FILTER_COMPARISON_OP_CONTAINS) {
        // contains: tags/depends contains value, or title contains substring
        if (field_type == TATR_FILTER_FIELD_TYPE_TAGS ||
            field_type == TATR_FILTER_FIELD_TYPE_DEPENDS) {
            // Right side must be an identifier (a tag name or a task ID)
            if (right->kind != TATR_FILTER_AST_NODE_KIND_IDENTIFIER) {
                unsigned long line, column;
                tatr_filter_lexer_position_info(lexer, right->info.index, &line, &column);
                snprintf(error_msg, error_msg_size, "line %lu, col %lu: " SS_Fmt " 'contains' requires an identifier",
                         line, column, SS_Arg(left->data.field.name));
                return false;
            }
        } else if (field_type == TATR_FILTER_FIELD_TYPE_TITLE) {
            // Right side must be an identifier (substring)
            if (right->kind != TATR_FILTER_AST_NODE_KIND_IDENTIFIER) {
                unsigned long line, column;
                tatr_filter_lexer_position_info(lexer, right->info.index, &line, &column);
                snprintf(error_msg, error_msg_size, "line %lu, col %lu: title 'contains' requires a string value", line, column);
                return false;
            }
        } else {
            unsigned long line, column;
            tatr_filter_lexer_position_info(lexer, node->info.index, &line, &column);
            snprintf(error_msg, error_msg_size, "line %lu, col %lu: 'contains' operator not supported for field '" SS_Fmt "'",
                     line, column, SS_Arg(left->data.field.name));
            return false;
        }
    }

    return true;
}

static boolean tatr_filter_typecheck_node(Tatr_Filter_Ast_Node *node, Tatr_Filter_Lexer *lexer, char *error_msg, size_t error_msg_size) {
    if (!node) return true;

    switch (node->kind) {
        case TATR_FILTER_AST_NODE_KIND_BINARY_OP:
            if (!tatr_filter_typecheck_node(node->data.binary_op.left, lexer, error_msg, error_msg_size)) {
                return false;
            }
            if (!tatr_filter_typecheck_node(node->data.binary_op.right, lexer, error_msg, error_msg_size)) {
                return false;
            }
            return true;

        case TATR_FILTER_AST_NODE_KIND_UNARY_OP:
            return tatr_filter_typecheck_node(node->data.unary_op.operand, lexer, error_msg, error_msg_size);

        case TATR_FILTER_AST_NODE_KIND_COMPARISON:
            return tatr_filter_typecheck_comparison(node, lexer, error_msg, error_msg_size);

        case TATR_FILTER_AST_NODE_KIND_FIELD:
        case TATR_FILTER_AST_NODE_KIND_IDENTIFIER:
        case TATR_FILTER_AST_NODE_KIND_LIST:
            return true;

        default:
            return true;
    }
}

// Interpreter - evaluates AST to boolean
static boolean tatr_filter_eval_node(Tatr_Filter_Ast_Node *node, const Task *task);

static boolean tatr_filter_eval_comparison(Tatr_Filter_Ast_Node *node, const Task *task) {
    Tatr_Filter_Ast_Node *left = node->data.comparison.left;
    Tatr_Filter_Ast_Node *right = node->data.comparison.right;
    Tatr_Filter_Comparison_Op op = node->data.comparison.op;

    // Get field type
    Tatr_Filter_Field_Type field_type = tatr_filter_get_field_type(left->data.field.name);

    size_t enum_count = 0;
    const char *enum_label = NULL;
    const char *enum_hint = NULL;
    const Aids_String_Slice *enum_table =
        tatr_filter_enum_table(field_type, &enum_count, &enum_label, &enum_hint);

    if (op == TATR_FILTER_COMPARISON_OP_EQ) {
        if (enum_table != NULL) {
            int expected = 0;
            if (!enum_from_string(&right->data.identifier.value, enum_table, enum_count, &expected)) {
                return false;
            }
            return tatr_filter_enum_value(field_type, task) == expected;
        } else if (field_type == TATR_FILTER_FIELD_TYPE_PARENT) {
            return task->meta.parent.len > 0 &&
                   aids_string_slice_compare(&task->meta.parent, &right->data.identifier.value) == 0;
        } else if (field_type == TATR_FILTER_FIELD_TYPE_PRIORITY) {
            // Parse number from identifier
            unsigned int expected_priority = 0;
            for (unsigned long i = 0; i < right->data.identifier.value.len; i++) {
                char c = right->data.identifier.value.str[i];
                if (c >= '0' && c <= '9') {
                    expected_priority = expected_priority * 10 + (c - '0');
                } else {
                    return false; // Invalid number
                }
            }
            return task->meta.priority == expected_priority;
        } else if (field_type == TATR_FILTER_FIELD_TYPE_TITLE) {
            return aids_string_slice_compare(&task->title, &right->data.identifier.value) == 0;
        }
    } else if (op == TATR_FILTER_COMPARISON_OP_IN) {
        if (enum_table != NULL) {
            int actual = tatr_filter_enum_value(field_type, task);
            for (unsigned long i = 0; i < right->data.list.items.count; i++) {
                Tatr_Filter_Ast_Node **item_ptr;
                aids_array_get(&right->data.list.items, i, (void**)&item_ptr);
                Tatr_Filter_Ast_Node *item = *item_ptr;

                int expected = 0;
                if (enum_from_string(&item->data.identifier.value, enum_table, enum_count, &expected) &&
                    actual == expected) {
                    return true;
                }
            }
            return false;
        }
    } else if (op == TATR_FILTER_COMPARISON_OP_CONTAINS) {
        if (field_type == TATR_FILTER_FIELD_TYPE_TAGS ||
            field_type == TATR_FILTER_FIELD_TYPE_DEPENDS) {
            // Check if the task carries the tag / dependency
            const Aids_Array *items = field_type == TATR_FILTER_FIELD_TYPE_TAGS
                                          ? &task->meta.tags
                                          : &task->meta.depends_on;
            for (unsigned long i = 0; i < items->count; i++) {
                Aids_String_Slice *item;
                aids_array_get((Aids_Array *)items, i, (void**)&item);
                if (aids_string_slice_compare(item, &right->data.identifier.value) == 0) {
                    return true;
                }
            }
            return false;
        } else if (field_type == TATR_FILTER_FIELD_TYPE_TITLE) {
            // Check if title contains substring
            if (right->data.identifier.value.len > task->title.len) {
                return false;
            }

            // Simple substring search
            for (unsigned long i = 0; i <= task->title.len - right->data.identifier.value.len; i++) {
                boolean match = true;
                for (unsigned long j = 0; j < right->data.identifier.value.len; j++) {
                    if (task->title.str[i + j] != right->data.identifier.value.str[j]) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    return true;
                }
            }
            return false;
        }
    }

    return false;
}

static boolean tatr_filter_eval_node(Tatr_Filter_Ast_Node *node, const Task *task) {
    if (!node) return true;

    switch (node->kind) {
        case TATR_FILTER_AST_NODE_KIND_BINARY_OP:
            if (node->data.binary_op.op == TATR_FILTER_BINARY_OP_AND) {
                return tatr_filter_eval_node(node->data.binary_op.left, task) &&
                       tatr_filter_eval_node(node->data.binary_op.right, task);
            } else if (node->data.binary_op.op == TATR_FILTER_BINARY_OP_OR) {
                return tatr_filter_eval_node(node->data.binary_op.left, task) ||
                       tatr_filter_eval_node(node->data.binary_op.right, task);
            }
            return false;

        case TATR_FILTER_AST_NODE_KIND_UNARY_OP:
            if (node->data.unary_op.op == TATR_FILTER_UNARY_OP_NOT) {
                return !tatr_filter_eval_node(node->data.unary_op.operand, task);
            }
            return false;

        case TATR_FILTER_AST_NODE_KIND_COMPARISON:
            return tatr_filter_eval_comparison(node, task);

        default:
            return false;
    }
}

// Free AST nodes recursively
static void tatr_filter_ast_free(Tatr_Filter_Ast_Node *node) {
    if (!node) return;

    switch (node->kind) {
        case TATR_FILTER_AST_NODE_KIND_BINARY_OP:
            tatr_filter_ast_free(node->data.binary_op.left);
            tatr_filter_ast_free(node->data.binary_op.right);
            break;

        case TATR_FILTER_AST_NODE_KIND_UNARY_OP:
            tatr_filter_ast_free(node->data.unary_op.operand);
            break;

        case TATR_FILTER_AST_NODE_KIND_COMPARISON:
            tatr_filter_ast_free(node->data.comparison.left);
            tatr_filter_ast_free(node->data.comparison.right);
            break;

        case TATR_FILTER_AST_NODE_KIND_LIST:
            // Free all list items
            for (unsigned long i = 0; i < node->data.list.items.count; i++) {
                Tatr_Filter_Ast_Node **item_ptr;
                if (aids_array_get(&node->data.list.items, i, (void**)&item_ptr) == AIDS_OK) {
                    tatr_filter_ast_free(*item_ptr);
                }
            }
            aids_array_free(&node->data.list.items);
            break;

        case TATR_FILTER_AST_NODE_KIND_FIELD:
        case TATR_FILTER_AST_NODE_KIND_IDENTIFIER:
            // No child nodes to free
            break;
    }

    free(node);
}

// Main entry point: parse, typecheck, and create evaluator
static Tatr_Filter_Ast_Node* tatr_filter_compile(Aids_String_Slice input, char *error_msg, size_t error_msg_size) {
    // Parse
    Tatr_Filter_Ast_Node *ast = tatr_filter_parse(input, error_msg, error_msg_size);
    if (!ast) {
        return NULL;
    }

    // Typecheck
    Tatr_Filter_Lexer lexer;
    tatr_filter_lexer_init(&lexer, input);
    if (!tatr_filter_typecheck_node(ast, &lexer, error_msg, error_msg_size)) {
        tatr_filter_ast_free(ast);
        return NULL;
    }

    return ast;
}

// Evaluate filter against a task
static boolean tatr_filter_eval(Tatr_Filter_Ast_Node *ast, const Task *task) {
    return tatr_filter_eval_node(ast, task);
}

static int main_ls(const Tatr_Context *ctx) {
    int result = 0;
    size_t skipped_tasks = 0;
    Argparse_Parser parser = {0};
    Sort_By sort_by = Sort_By_CREATED;
    boolean recursive = false;
    Tatr_Filter_Ast_Node *filter_ast = NULL;

    argparse_parser_init(&parser, "tatr ls", "List tasks", TATR_VERSION);

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 's',
        .long_name = "sort",
        .description = "Sort by (created, priority, title; default: created)",
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'R',
        .long_name = "recursive",
        .description = "Recursively search for tasks directories in all subdirectories",
        .type = ARGUMENT_TYPE_FLAG,
        .required = 0
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'f',
        .long_name = "filter",
        .description = "Filter tasks using a query expression (e.g., '(:status eq OPEN) and (:tags contains feature)')",
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    if (argparse_parse(&parser, ctx->argc, ctx->argv) != ARG_OK) {
        return_defer(1);
    }

    char *sort_by_str = argparse_get_value(&parser, "sort");
    if (sort_by_str != NULL) {
        Aids_String_Slice sort_by_slice = aids_string_slice_from_cstr(sort_by_str);
        sort_by = sort_by_from_string(&sort_by_slice);
    }

    recursive = argparse_get_flag(&parser, "recursive");

    Aids_Array project_dirs = {0};  /* Aids_String_Slice */
    Aids_Array all_projects = {0};  /* Project_Tasks */

    aids_array_init(&project_dirs, sizeof(Aids_String_Slice));
    aids_array_init(&all_projects, sizeof(Project_Tasks));

    // Parse and compile filter if provided
    char *filter_str = argparse_get_value(&parser, "filter");
    if (filter_str != NULL) {
        char error_msg[256];
        Aids_String_Slice filter_input = aids_string_slice_from_cstr(filter_str);
        filter_ast = tatr_filter_compile(filter_input, error_msg, sizeof(error_msg));
        if (!filter_ast) {
            aids_log(AIDS_ERROR, "Filter error: %s", error_msg);
            return_defer(1);
        }
    }

    if (recursive) {
        if (find_tasks_dirs_recursive(&ctx->cwd, &project_dirs) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to find tasks directories recursively");
            return_defer(1);
        }
    } else {
        if (find_current_tasks_dir(&ctx->cwd, &project_dirs) != AIDS_OK) {
            return_defer(1);
        }
    }

    for (size_t i = 0; i < project_dirs.count; ++i) {
        Aids_String_Slice *project_dir = NULL;
        if (aids_array_get(&project_dirs, i, (void **)&project_dir) != AIDS_OK) {
            continue;
        }

        Aids_String_Slice full_tasks_dir = {0};
        Aids_String_Builder path_sb = {0};
        aids_string_builder_init(&path_sb);
        if (aids_string_builder_append(&path_sb, SS_Fmt "/" SS_Fmt,
                                       SS_Arg(*project_dir), SS_Arg(TASKS_PATH)) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to build tasks directory path: %s", aids_failure_reason());
            aids_string_builder_free(&path_sb);
            continue;
        }
        aids_string_builder_to_slice(&path_sb, &full_tasks_dir);

        Project_Tasks pt = {0};
        pt.project_dir = *project_dir;
        aids_array_init(&pt.tasks, sizeof(Task_Entry));

        if (load_tasks_from_dir(&full_tasks_dir, &pt.tasks, sort_by, &skipped_tasks) != AIDS_OK) {
            aids_log(AIDS_WARNING, "Failed to list tasks in '" SS_Fmt "'", SS_Arg(full_tasks_dir));
            AIDS_FREE(full_tasks_dir.str);
            project_tasks_cleanup(&pt);
            skipped_tasks++;
            continue;
        }
        AIDS_FREE(full_tasks_dir.str);

        if (aids_array_append(&all_projects, &pt) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to append project tasks: %s", aids_failure_reason());
            project_tasks_cleanup(&pt);
            return_defer(1);
        }
    }

    for (size_t i = 0; i < all_projects.count; ++i) {
        Project_Tasks *pt = NULL;
        if (aids_array_get(&all_projects, i, (void **)&pt) != AIDS_OK) {
            continue;
        }

        if (pt->tasks.count == 0) {
            continue;
        }

        Aids_String_Slice full_tasks_dir = {0};
        Aids_String_Builder path_sb = {0};
        aids_string_builder_init(&path_sb);
        if (aids_string_builder_append(&path_sb, SS_Fmt "/" SS_Fmt,
                                       SS_Arg(pt->project_dir), SS_Arg(TASKS_PATH)) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to build tasks directory path: %s", aids_failure_reason());
            aids_string_builder_free(&path_sb);
            continue;
        }
        aids_string_builder_to_slice(&path_sb, &full_tasks_dir);

        // In recursive mode with a filter, check if any tasks match before printing header
        boolean has_matching_tasks = false;
        if (recursive && filter_ast != NULL) {
            for (size_t j = 0; j < pt->tasks.count; ++j) {
                Task_Entry *entry = NULL;
                if (aids_array_get(&pt->tasks, j, (void **)&entry) == AIDS_OK) {
                    if (tatr_filter_eval(filter_ast, &entry->task)) {
                        has_matching_tasks = true;
                        break;
                    }
                }
            }
        } else if (recursive) {
            // No filter, so all tasks match
            has_matching_tasks = true;
        }

        // Print section header only if we have matching tasks (in recursive mode)
        if (recursive && has_matching_tasks) {
            printf(SS_Fmt "\n", SS_Arg(pt->project_dir));
        }

        for (size_t j = 0; j < pt->tasks.count; ++j) {
            Task_Entry *entry = NULL;
            if (aids_array_get(&pt->tasks, j, (void **)&entry) == AIDS_OK) {
                // Apply filter if provided
                if (filter_ast != NULL) {
                    if (!tatr_filter_eval(filter_ast, &entry->task)) {
                        continue; // Task doesn't match filter, skip it
                    }
                }
                task_print(full_tasks_dir, entry->huid, entry->task);
            }
        }

        AIDS_FREE(full_tasks_dir.str);
    }

    // The readable tasks were listed above; unreadable ones were named as they
    // were skipped. Still exit non-zero, so a broken record cannot hide behind
    // a successful-looking listing.
    if (skipped_tasks > 0) {
        aids_log(AIDS_ERROR, "%zu task(s) could not be read; run 'tatr check' for details", skipped_tasks);
        result = 1;
    }

defer:
    if (filter_ast != NULL) {
        tatr_filter_ast_free(filter_ast);
    }

    for (size_t i = 0; i < all_projects.count; ++i) {
        Project_Tasks *pt = NULL;
        if (aids_array_get(&all_projects, i, (void **)&pt) == AIDS_OK && pt != NULL) {
            project_tasks_cleanup(pt);
        }
    }
    aids_array_free(&all_projects);

    cleanup_string_slice_array(&project_dirs);

    argparse_parser_free(&parser);
    return result;
}

// ---------------------------------------------------------------------------
// Artifact scans: the shared readers for the sibling records a task carries.
// `check` lints with them and `flow` gates its transitions on them, so the
// lint and the lifecycle guards read the same bytes the same way. Keeping one
// implementation per artifact question is what makes the tying invariant hold:
// a transition can never produce a state the lint would then flag, because
// both sides ask the same function.
// ---------------------------------------------------------------------------

// Reads "<tasks_dir>/<huid>/<name>" if it exists. Returns true and the
// content (caller frees content->str) when the file was read; false when it
// does not exist. A file that exists but cannot be read is logged and
// treated as absent.
static boolean task_sibling_read(const Aids_String_Slice *tasks_dir,
                                 const Aids_String_Slice *huid,
                                 const char *name,
                                 Aids_String_Slice *content) {
    char path_buffer[PATH_MAX];
    if (snprintf(path_buffer, sizeof(path_buffer), SS_Fmt "/" SS_Fmt "/%s",
                 SS_Arg(*tasks_dir), SS_Arg(*huid), name) < 0) {
        return false;
    }
    if (access(path_buffer, F_OK) != 0) {
        return false;
    }
    Aids_String_Slice path = aids_string_slice_from_cstr(path_buffer);
    if (aids_io_read(&path, content, "r") != AIDS_OK) {
        aids_log(AIDS_WARNING, "'%s' exists but could not be read: %s",
                 path_buffer, aids_failure_reason());
        return false;
    }
    return true;
}

// True when "<tasks_dir>/<huid>/<name>" exists. Presence only: RETRO.md is
// required to exist, never parsed.
static boolean task_sibling_exists(const Aids_String_Slice *tasks_dir,
                                   const Aids_String_Slice *huid,
                                   const char *name) {
    char path_buffer[PATH_MAX];
    if (snprintf(path_buffer, sizeof(path_buffer), SS_Fmt "/" SS_Fmt "/%s",
                 SS_Arg(*tasks_dir), SS_Arg(*huid), name) < 0) {
        return false;
    }
    return access(path_buffer, F_OK) == 0;
}

// Counts "- [ ]" items under the task body's "## Steps" heading. The heading
// match is exact, so "## Steps taken later" is NOT the Steps section.
static size_t artifact_count_unchecked_steps(Aids_String_Slice task_raw) {
    size_t unchecked = 0;
    Aids_String_Slice scan = task_raw;
    Aids_String_Slice line = {0};
    boolean in_steps = false;
    Aids_String_Slice steps_heading = aids_string_slice_from_cstr("## Steps");
    Aids_String_Slice any_heading = aids_string_slice_from_cstr("## ");
    Aids_String_Slice unchecked_box = aids_string_slice_from_cstr("- [ ]");
    while (aids_string_slice_tokenize(&scan, '\n', &line)) {
        Aids_String_Slice trimmed = line;
        aids_string_slice_trim_right(&trimmed);
        if (aids_string_slice_starts_with(&trimmed, any_heading)) {
            in_steps = aids_string_slice_compare(&trimmed, &steps_heading) == 0;
            continue;
        }
        if (in_steps && aids_string_slice_starts_with(&trimmed, unchecked_box)) {
            unchecked++;
        }
    }
    return unchecked;
}

// The known review severities, as written inside the parens of a finding
// line: "- [ ] R1.2 (MAJOR) file:line - ...".
static boolean artifact_severity_is_known(Aids_String_Slice severity) {
    static const char *known[] = {"BLOCKER", "MAJOR", "MINOR", "NIT"};
    for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); ++i) {
        Aids_String_Slice k = aids_string_slice_from_cstr((char *)known[i]);
        if (aids_string_slice_compare(&severity, &k) == 0) {
            return true;
        }
    }
    return false;
}

// Parses one REVIEW.md line as a finding: "- [ ] R1.2 (MAJOR) file:line - ...".
// Tolerant scan - a finding line starts with a checkbox followed by an R-id
// (the R must be followed by a digit, so "- [ ] Rebase onto master" is prose),
// and its severity is whatever sits in the first parens. Returns false for any
// line that is not a finding; fills *severity with the parenthesised token and
// *resolved with whether the box is ticked.
static boolean artifact_parse_finding_line(Aids_String_Slice line,
                                           Aids_String_Slice *severity,
                                           boolean *resolved) {
    Aids_String_Slice l = line;
    aids_string_slice_trim(&l);
    Aids_String_Slice box_open = aids_string_slice_from_cstr("- [ ] R");
    Aids_String_Slice box_done = aids_string_slice_from_cstr("- [x] R");
    boolean open = aids_string_slice_starts_with(&l, box_open);
    boolean done = aids_string_slice_starts_with(&l, box_done);
    if (!open && !done) {
        return false;
    }
    if (l.len <= box_open.len || !isdigit(l.str[box_open.len])) {
        return false;
    }
    unsigned long paren_open = 0;
    while (paren_open < l.len && l.str[paren_open] != '(') {
        paren_open++;
    }
    if (paren_open == l.len) {
        return false;
    }
    unsigned long paren_close = paren_open + 1;
    while (paren_close < l.len && l.str[paren_close] != ')') {
        paren_close++;
    }
    if (paren_close == l.len) {
        return false;
    }
    *severity = aids_string_slice_from_parts(l.str + paren_open + 1,
                                             paren_close - paren_open - 1);
    *resolved = done;
    return true;
}

// Counts the unticked BLOCKER and MAJOR findings in a REVIEW.md. These are the
// findings a review cycle must resolve before the work moves on; MINOR and NIT
// do not block.
static size_t artifact_count_open_blocking_findings(Aids_String_Slice review) {
    size_t open = 0;
    Aids_String_Slice scan = review;
    Aids_String_Slice line = {0};
    Aids_String_Slice blocker = aids_string_slice_from_cstr("BLOCKER");
    Aids_String_Slice major = aids_string_slice_from_cstr("MAJOR");
    while (aids_string_slice_tokenize(&scan, '\n', &line)) {
        Aids_String_Slice severity = {0};
        boolean resolved = false;
        if (!artifact_parse_finding_line(line, &severity, &resolved) || resolved) {
            continue;
        }
        if (aids_string_slice_compare(&severity, &blocker) == 0 ||
            aids_string_slice_compare(&severity, &major) == 0) {
            open++;
        }
    }
    return open;
}

// The latest "- VERDICT: " value in a REVIEW.md - rounds are appended, never
// rewritten, so the last one wins. The value is the first whitespace-delimited
// token, so "APPROVE (1 round)" and a CRLF "APPROVE\r" both read as APPROVE.
// Returns false when the file carries no VERDICT line at all.
static boolean artifact_latest_verdict(Aids_String_Slice review, Aids_String_Slice *out) {
    Aids_String_Slice scan = review;
    Aids_String_Slice line = {0};
    Aids_String_Slice verdict_format = aids_string_slice_from_cstr("- VERDICT: ");
    boolean found = false;
    while (aids_string_slice_tokenize(&scan, '\n', &line)) {
        Aids_String_Slice l = line;
        if (!aids_string_slice_starts_with(&l, verdict_format)) {
            continue;
        }
        aids_string_slice_skip(&l, verdict_format.len);
        aids_string_slice_trim_left(&l);
        unsigned long tok = 0;
        while (tok < l.len && !isspace(l.str[tok])) {
            tok++;
        }
        l.len = tok;
        *out = l;
        found = true;
    }
    return found;
}

// Returns the slice up to (not including) an inline " #" comment, so a
// DECISION.md line like "- STATUS: ACCEPTED   # ACCEPTED | SUPERSEDED by ..."
// (the enum-hint comment style the spike/decision templates use) validates on
// its value alone. Leaves the slice untouched when there is no inline comment.
static Aids_String_Slice artifact_strip_inline_comment(Aids_String_Slice s) {
    for (size_t i = 0; i + 1 < s.len; ++i) {
        if (s.str[i] == ' ' && s.str[i + 1] == '#') {
            s.len = i;
            break;
        }
    }
    return s;
}

// The lifecycle a DECISION.md's "- STATUS: " line records.
typedef enum {
    Decision_Status_MISSING,    // no STATUS line at all
    Decision_Status_INVALID,    // a STATUS line that is neither valid form
    Decision_Status_ACCEPTED,
    Decision_Status_SUPERSEDED
} Decision_Status;

// Scans a DECISION.md for its first "- STATUS: " value (first STATUS wins).
// *value receives the raw value with any inline comment stripped, for use in
// a diagnostic; *ref receives the supersede reference when the status is
// SUPERSEDED. An empty "SUPERSEDED by" reference is INVALID, not SUPERSEDED:
// a supersede that names nothing records nothing.
static Decision_Status artifact_decision_status(Aids_String_Slice decision,
                                                Aids_String_Slice *value,
                                                Aids_String_Slice *ref) {
    Aids_String_Slice accepted = aids_string_slice_from_cstr("ACCEPTED");
    Aids_String_Slice superseded_by = aids_string_slice_from_cstr("SUPERSEDED by ");
    Aids_String_Slice scan = decision;
    Aids_String_Slice line = {0};

    while (aids_string_slice_tokenize(&scan, '\n', &line)) {
        Aids_String_Slice l = line;
        if (!aids_string_slice_starts_with(&l, STATUS_FORMAT)) {
            continue;
        }
        aids_string_slice_skip(&l, STATUS_FORMAT.len);
        Aids_String_Slice v = artifact_strip_inline_comment(l);
        aids_string_slice_trim(&v);
        *value = v;
        if (aids_string_slice_compare(&v, &accepted) == 0) {
            return Decision_Status_ACCEPTED;
        }
        if (aids_string_slice_starts_with(&v, superseded_by)) {
            Aids_String_Slice r = v;
            aids_string_slice_skip(&r, superseded_by.len);
            aids_string_slice_trim(&r);
            if (r.len > 0) {
                *ref = r;
                return Decision_Status_SUPERSEDED;
            }
        }
        return Decision_Status_INVALID;
    }
    return Decision_Status_MISSING;
}

// An EPIC container is exempt from the record-completeness rules: its
// aggregate record lives in its own TASK.md while child tasks carry the review
// and retro records, so demanding those files would force a fabricated one;
// and its frozen step boxes stay verbatim (superseded or dropped children are
// honest history) rather than being ticked to silence the lint. It may also
// sit IN_PROGRESS without a plan approval of its own - the plan gate applies
// to the work tasks underneath it. `check` and `flow` share the predicate so
// the lint and the lifecycle exempt the same records.
static boolean check_task_is_container(const Task *task) {
    return task->meta.kind == Task_Kind_EPIC;
}

// ---------------------------------------------------------------------------
// flow: the guarded lifecycle. `tatr flow <ID> [--to <STEP>]` is the only
// writer of STATUS, FLOW STEP and PLAN STATUS - `new` and `edit` cannot set
// them - so a record can only reach a state by walking edges whose
// preconditions were met. Every precondition is evaluated before anything is
// mutated and all unmet ones are reported, so a refused transition leaves
// TASK.md byte-identical. There is no --force and no repair verb: a record in
// the wrong state is corrected by hand in TASK.md, where a reviewer sees it in
// the diff.
// ---------------------------------------------------------------------------

// STATUS is derived from the flow step, never chosen. A work task therefore
// stays IN_PROGRESS through review and compound, and closes atomically at DONE
// in the same write that sets FLOW STEP: DONE.
static Task_Status flow_step_implied_status(Flow_Step step) {
    switch (step) {
    case Flow_Step_BACKLOG:
    case Flow_Step_UNDERSTANDING:
    case Flow_Step_PLANNING:
    case Flow_Step_PLANNED:
        return Task_Status_OPEN;
    case Flow_Step_WORKING:
    case Flow_Step_REVIEWING:
    case Flow_Step_COMPOUNDING:
        return Task_Status_IN_PROGRESS;
    case Flow_Step_DONE:
        return Task_Status_CLOSED;
    }
    return Task_Status_OPEN; // unreachable: the enum is closed
}

typedef struct {
    Flow_Step from;
    Flow_Step to;
} Flow_Transition;

// The eight legal edges, and nothing else:
//
//   BACKLOG -> UNDERSTANDING -> PLANNING -> PLANNED -> WORKING -> REVIEWING
//                                                        ^           |
//                                                        |           v
//                                                        +-- (fix) COMPOUNDING -> DONE
//
// REVIEWING is the only step with two successors. The first entry for a step
// is its default - the target a bare `tatr flow <ID>` picks - so the review
// loop back to WORKING must be asked for by name with `--to WORKING`.
static const Flow_Transition FLOW_TRANSITIONS[] = {
    {Flow_Step_BACKLOG,      Flow_Step_UNDERSTANDING},
    {Flow_Step_UNDERSTANDING, Flow_Step_PLANNING},
    {Flow_Step_PLANNING,     Flow_Step_PLANNED},
    {Flow_Step_PLANNED,      Flow_Step_WORKING},
    {Flow_Step_WORKING,      Flow_Step_REVIEWING},
    {Flow_Step_REVIEWING,    Flow_Step_COMPOUNDING},
    {Flow_Step_REVIEWING,    Flow_Step_WORKING},
    {Flow_Step_COMPOUNDING,  Flow_Step_DONE},
};

// The default successor of a step: the first edge leaving it. False when the
// step is terminal (DONE).
static boolean flow_step_successor(Flow_Step from, Flow_Step *out) {
    for (size_t i = 0; i < ENUM_COUNT(FLOW_TRANSITIONS); ++i) {
        if (FLOW_TRANSITIONS[i].from == from) {
            *out = FLOW_TRANSITIONS[i].to;
            return true;
        }
    }
    return false;
}

static boolean flow_transition_is_legal(Flow_Step from, Flow_Step to) {
    for (size_t i = 0; i < ENUM_COUNT(FLOW_TRANSITIONS); ++i) {
        if (FLOW_TRANSITIONS[i].from == from && FLOW_TRANSITIONS[i].to == to) {
            return true;
        }
    }
    return false;
}

// Names what a task at <from> may legally do next, so a refusal tells the
// caller the move to make instead of only the one it rejected.
static void flow_print_legal_moves(Flow_Step from) {
    Flow_Step targets[ENUM_COUNT(FLOW_TRANSITIONS)];
    size_t count = 0;
    for (size_t i = 0; i < ENUM_COUNT(FLOW_TRANSITIONS); ++i) {
        if (FLOW_TRANSITIONS[i].from == from) {
            targets[count++] = FLOW_TRANSITIONS[i].to;
        }
    }
    if (count == 0) {
        fprintf(stderr, "  " SS_Fmt " is terminal: nothing follows it\n",
                SS_Arg(Flow_Step_Strings[from]));
        return;
    }
    if (count == 1) {
        fprintf(stderr, "  the legal move from " SS_Fmt " is " SS_Fmt "\n",
                SS_Arg(Flow_Step_Strings[from]), SS_Arg(Flow_Step_Strings[targets[0]]));
        return;
    }
    fprintf(stderr, "  the legal moves from " SS_Fmt " are " SS_Fmt " (the default)",
            SS_Arg(Flow_Step_Strings[from]), SS_Arg(Flow_Step_Strings[targets[0]]));
    for (size_t i = 1; i < count; ++i) {
        fprintf(stderr, " or " SS_Fmt " (--to " SS_Fmt ")",
                SS_Arg(Flow_Step_Strings[targets[i]]), SS_Arg(Flow_Step_Strings[targets[i]]));
    }
    fprintf(stderr, "\n");
}

// Collected preconditions a transition did not satisfy. Every one is reported,
// not just the first: an agent that fixes one requirement only to be refused
// for the next one learns the gate one round trip at a time.
#define FLOW_UNMET_CAPACITY 32
#define FLOW_UNMET_MESSAGE_SIZE 512

typedef struct {
    char messages[FLOW_UNMET_CAPACITY][FLOW_UNMET_MESSAGE_SIZE];
    size_t count;
} Flow_Unmet;

static void flow_unmet_add(Flow_Unmet *unmet, const char *fmt, ...) {
    if (unmet->count >= FLOW_UNMET_CAPACITY) {
        return; // a task with 32 unmet preconditions has been told enough
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf(unmet->messages[unmet->count], FLOW_UNMET_MESSAGE_SIZE, fmt, args);
    va_end(args);
    unmet->count++;
}

// Every DEPENDS ON id must resolve to an existing task that is CLOSED. Reads
// each dependency through the same sibling reader and parser the rest of the
// tool uses, so an unparseable dependency is reported rather than assumed open.
static void flow_check_dependencies(const Aids_String_Slice *tasks_dir,
                                    const Task *task,
                                    Flow_Unmet *unmet) {
    for (size_t i = 0; i < task->meta.depends_on.count; ++i) {
        Aids_String_Slice *dep = NULL;
        if (aids_array_get((Aids_Array *)&task->meta.depends_on, i, (void **)&dep) != AIDS_OK ||
            dep == NULL) {
            continue;
        }
        Aids_String_Slice raw = {0};
        if (!task_sibling_read(tasks_dir, dep, TASK_FILE_NAME_CSTR, &raw)) {
            flow_unmet_add(unmet, "dependency " SS_Fmt " does not exist", SS_Arg(*dep));
            continue;
        }
        Task dep_task = {0};
        task_init_empty(&dep_task);
        if (task_deserialize(raw, &dep_task) != AIDS_OK) {
            flow_unmet_add(unmet, "dependency " SS_Fmt " does not parse", SS_Arg(*dep));
            AIDS_FREE(raw.str);
            task_cleanup(&dep_task);
            continue;
        }
        dep_task._buffer = raw.str;
        if (dep_task.meta.status != Task_Status_CLOSED) {
            flow_unmet_add(unmet, "dependency " SS_Fmt " is not CLOSED (STATUS: " SS_Fmt ")",
                           SS_Arg(*dep), SS_Arg(Task_Status_Strings[dep_task.meta.status]));
        }
        task_cleanup(&dep_task);
    }
}

// Evaluates every precondition the edge <from> -> <to> carries and appends one
// message per unmet one. Three gates exist, and an edge either carries a gate
// whole or not at all:
//
//   PLANNED    -> WORKING      the plan gate: an approved plan, CLOSED deps
//   REVIEWING  -> COMPOUNDING  the review gate: an approved REVIEW.md
//   COMPOUNDING-> DONE         both of the above, plus the close gate:
//                              no unchecked Steps, a RETRO.md, and a valid
//                              DECISION.md status when the task carries one
//
// REVIEWING -> WORKING and the three pre-plan edges carry no preconditions:
// going back to fix something must never be harder than going forward.
//
// KIND: EPIC is exempt from exactly the four requirements it is already exempt
// from in `tatr check` - plan approval, review, retro and unchecked Steps -
// because an Epic container's record lives in its children. Its dependencies
// and its DECISION.md are checked like anyone's.
static void flow_check_preconditions(const Aids_String_Slice *tasks_dir,
                                     const Aids_String_Slice *huid,
                                     const Task *task,
                                     Aids_String_Slice task_raw,
                                     Flow_Step from,
                                     Flow_Step to,
                                     Flow_Unmet *unmet) {
    boolean plan_gate = false;
    boolean review_gate = false;
    boolean close_gate = false;
    boolean exempt = check_task_is_container(task);

    if (from == Flow_Step_PLANNED && to == Flow_Step_WORKING) {
        plan_gate = true;
    } else if (from == Flow_Step_REVIEWING && to == Flow_Step_COMPOUNDING) {
        review_gate = true;
    } else if (from == Flow_Step_COMPOUNDING && to == Flow_Step_DONE) {
        plan_gate = true;
        review_gate = true;
        close_gate = true;
    }

    if (plan_gate) {
        if (!exempt && task->meta.plan_status != Plan_Status_APPROVED) {
            flow_unmet_add(unmet, "the plan is not approved (PLAN STATUS: " SS_Fmt "); "
                           "walk the plan gate with `tatr flow " SS_Fmt " --to PLANNED` "
                           "once the user accepts the plan",
                           SS_Arg(Plan_Status_Strings[task->meta.plan_status]), SS_Arg(*huid));
        }
        flow_check_dependencies(tasks_dir, task, unmet);
    }

    if (review_gate) {
        Aids_String_Slice review = {0};
        if (!task_sibling_read(tasks_dir, huid, "REVIEW.md", &review)) {
            // Only the PRESENCE of the record is exempt for a container -
            // exactly as `check` exempts it from closed-missing-review but
            // never from closed-not-approved. A REVIEW.md an Epic does carry
            // is held to the same verdict as anyone's, so no transition can
            // produce a state the lint would then flag.
            if (!exempt) {
                flow_unmet_add(unmet, "there is no REVIEW.md: the work has not been reviewed");
            }
        } else {
            Aids_String_Slice verdict = {0};
            Aids_String_Slice approve = aids_string_slice_from_cstr("APPROVE");
            if (!artifact_latest_verdict(review, &verdict)) {
                flow_unmet_add(unmet, "REVIEW.md has no VERDICT line");
            } else if (aids_string_slice_compare(&verdict, &approve) != 0) {
                flow_unmet_add(unmet, "the latest REVIEW.md verdict is '" SS_Fmt "', not APPROVE",
                               SS_Arg(verdict));
            }
            size_t open = artifact_count_open_blocking_findings(review);
            if (open > 0) {
                flow_unmet_add(unmet, "REVIEW.md has %zu open BLOCKER/MAJOR finding(s)", open);
            }
            AIDS_FREE(review.str);
        }
    }

    if (close_gate) {
        if (!exempt) {
            size_t unchecked = artifact_count_unchecked_steps(task_raw);
            if (unchecked > 0) {
                flow_unmet_add(unmet, "%zu unchecked Steps item(s) remain", unchecked);
            }
            if (!task_sibling_exists(tasks_dir, huid, "RETRO.md")) {
                flow_unmet_add(unmet, "there is no RETRO.md: the retro has not been written");
            }
        }
        Aids_String_Slice decision = {0};
        if (task_sibling_read(tasks_dir, huid, "DECISION.md", &decision)) {
            Aids_String_Slice value = {0};
            Aids_String_Slice ref = {0};
            switch (artifact_decision_status(decision, &value, &ref)) {
            case Decision_Status_ACCEPTED:
            case Decision_Status_SUPERSEDED:
                break;
            case Decision_Status_INVALID:
                flow_unmet_add(unmet, "DECISION.md has an invalid STATUS '" SS_Fmt
                               "' (use ACCEPTED or 'SUPERSEDED by <ref>')", SS_Arg(value));
                break;
            case Decision_Status_MISSING:
                flow_unmet_add(unmet, "DECISION.md has no STATUS line");
                break;
            }
            AIDS_FREE(decision.str);
        }
    }
}

static int main_flow(const Tatr_Context *ctx) {
    int result = 0;
    Argparse_Parser parser = {0};
    Task task = {0};
    boolean task_initialized = false;
    Aids_String_Slice tasks_dir = {0};
    Aids_String_Slice task_file_path = {0};
    Aids_String_Slice raw = {0};
    Flow_Unmet unmet = {0};

    argparse_parser_init(&parser, "tatr flow", "Move a task to its next flow step", TATR_VERSION);

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'I',
        .long_name = "id",
        .description = "Task ID (format YYYYMMDD-HHMMSS)",
        .type = ARGUMENT_TYPE_POSITIONAL,
        .required = 1
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'o',
        .long_name = "to",
        .description = "Target flow step (default: the next step; " FLOW_STEP_VALUES_CSTR ")",
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    if (argparse_parse(&parser, ctx->argc, ctx->argv) != ARG_OK) {
        return_defer(1);
    }

    char *id_str = argparse_get_value(&parser, "id");
    if (id_str == NULL) {
        aids_log(AIDS_ERROR, "Missing task ID");
        return_defer(1);
    }
    Aids_String_Slice id = aids_string_slice_from_cstr(id_str);

    if (task_resolve(&ctx->cwd, &id, &tasks_dir, &task_file_path) != AIDS_OK) {
        return_defer(1);
    }

    task_init_empty(&task);
    task_initialized = true;

    // The raw bytes come back with the parsed record: the close gate counts
    // the unchecked Steps in the very body this task was parsed from.
    if (task_load_raw(&task_file_path, &task, &raw) != AIDS_OK) {
        return_defer(1);
    }

    Flow_Step from = task.meta.flow_step;
    Flow_Step to = from;

    char *to_str = argparse_get_value(&parser, "to");
    if (to_str != NULL) {
        Aids_String_Slice to_slice = aids_string_slice_from_cstr(to_str);
        if (!flow_step_from_string(&to_slice, &to)) {
            aids_log(AIDS_ERROR, "Invalid flow step '%s': expected " FLOW_STEP_VALUES_CSTR, to_str);
            return_defer(1);
        }
    } else if (!flow_step_successor(from, &to)) {
        aids_log(AIDS_ERROR, "Task " SS_Fmt " is at " SS_Fmt " and has nowhere to go",
                 SS_Arg(id), SS_Arg(Flow_Step_Strings[from]));
        flow_print_legal_moves(from);
        return_defer(1);
    }

    if (!flow_transition_is_legal(from, to)) {
        aids_log(AIDS_ERROR, "Illegal transition for " SS_Fmt ": " SS_Fmt " -> " SS_Fmt,
                 SS_Arg(id), SS_Arg(Flow_Step_Strings[from]), SS_Arg(Flow_Step_Strings[to]));
        flow_print_legal_moves(from);
        return_defer(1);
    }

    flow_check_preconditions(&tasks_dir, &id, &task, raw, from, to, &unmet);
    if (unmet.count > 0) {
        aids_log(AIDS_ERROR, "Refusing to move " SS_Fmt " from " SS_Fmt " to " SS_Fmt
                 ": %zu precondition(s) not met",
                 SS_Arg(id), SS_Arg(Flow_Step_Strings[from]), SS_Arg(Flow_Step_Strings[to]),
                 unmet.count);
        for (size_t i = 0; i < unmet.count; ++i) {
            fprintf(stderr, "  - %s\n", unmet.messages[i]);
        }
        return_defer(1);
    }

    // Every precondition held, so the whole transition lands in one write:
    // the step, the status it implies, and - at the plan gate only - the plan
    // approval it records.
    task.meta.flow_step = to;
    task.meta.status = flow_step_implied_status(to);
    if (to == Flow_Step_PLANNED) {
        task.meta.plan_status = Plan_Status_APPROVED;
    }

    if (task_save(&task_file_path, &task) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to save task"); // task_save logged the cause
        return_defer(1);
    }

    printf("Task " SS_Fmt " moved " SS_Fmt " -> " SS_Fmt " (STATUS: " SS_Fmt ")\n",
           SS_Arg(id), SS_Arg(Flow_Step_Strings[from]), SS_Arg(Flow_Step_Strings[to]),
           SS_Arg(Task_Status_Strings[task.meta.status]));

defer:
    if (task_initialized) {
        task_cleanup(&task);
    }
    if (tasks_dir.str != NULL) {
        AIDS_FREE(tasks_dir.str);
    }
    if (task_file_path.str != NULL) {
        AIDS_FREE(task_file_path.str);
    }
    argparse_parser_free(&parser);
    return result;
}

// ---------------------------------------------------------------------------
// check: lint task artifacts for process drift. Reports findings one per
// line as "<id>: <rule>: <detail>" on stdout; exits 1 if anything was found,
// 0 when clean. Unlike ls, a malformed task is a FINDING here, not an abort:
// the whole point is to surface broken artifacts.
// ---------------------------------------------------------------------------

static int string_slice_compare_fn(const void *a, const void *b) {
    return aids_string_slice_compare((const Aids_String_Slice *)a,
                                     (const Aids_String_Slice *)b);
}

static boolean slice_contains_cstr(Aids_String_Slice haystack, const char *needle) {
    unsigned long needle_len = strlen(needle);
    if (needle_len == 0 || haystack.len < needle_len) {
        return false;
    }
    for (unsigned long i = 0; i + needle_len <= haystack.len; ++i) {
        if (memcmp(haystack.str + i, needle, needle_len) == 0) {
            return true;
        }
    }
    return false;
}

// Resolves a DECISION.md supersede reference to an existing DECISION.md. The
// canonical ref is "tasks/<id>/DECISION.md", but a bare "<id>" works too: every
// task is flat under one tasks dir, so we pull the first path segment that is a
// well-formed HUID and check "<tasks_dir>/<huid>/DECISION.md" on disk. A ref
// with no HUID segment (e.g. the literal "tasks/<id>/DECISION.md" placeholder)
// does not resolve.
static boolean check_supersede_ref_resolves(const Aids_String_Slice *tasks_dir,
                                            Aids_String_Slice ref) {
    Aids_String_Slice scan = ref;
    Aids_String_Slice segment = {0};
    Aids_String_Slice huid = {0};
    boolean found = false;
    while (aids_string_slice_tokenize(&scan, '/', &segment)) {
        Aids_String_Slice s = segment;
        aids_string_slice_trim(&s);
        if (ishuid(&s)) {
            huid = s;
            found = true;
            break;
        }
    }
    if (!found) {
        return false;
    }
    char path_buffer[PATH_MAX];
    if (snprintf(path_buffer, sizeof(path_buffer), SS_Fmt "/" SS_Fmt "/DECISION.md",
                 SS_Arg(*tasks_dir), SS_Arg(huid)) < 0) {
        return false;
    }
    return access(path_buffer, F_OK) == 0;
}

// Lints a task's DECISION.md (only called when the file exists): the STATUS
// value must be exactly ACCEPTED or "SUPERSEDED by <ref>" (bad-decision-status),
// and every supersede reference - the one in a SUPERSEDED-by STATUS and any
// "- Supersedes: <ref>" header line - must resolve to an existing DECISION.md
// (dangling-supersede). Fires by presence only, so tasks without a DECISION.md
// are never touched. Returns findings printed.
static size_t check_decision(const Aids_String_Slice *tasks_dir,
                             const Aids_String_Slice *huid,
                             const Aids_String_Slice *decision) {
    size_t findings = 0;
    Aids_String_Slice supersedes_format = aids_string_slice_from_cstr("- Supersedes: ");
    Aids_String_Slice line = {0};

    // bad-decision-status: validate the first "- STATUS: " value. A missing
    // STATUS line is itself a finding (a decision record needs a lifecycle).
    Aids_String_Slice value = {0};
    Aids_String_Slice ref = {0};
    switch (artifact_decision_status(*decision, &value, &ref)) {
    case Decision_Status_ACCEPTED:
        break;
    case Decision_Status_SUPERSEDED:
        if (!check_supersede_ref_resolves(tasks_dir, ref)) {
            printf(SS_Fmt ": dangling-supersede: STATUS supersedes '" SS_Fmt "' which has no DECISION.md\n",
                   SS_Arg(*huid), SS_Arg(ref));
            findings++;
        }
        break;
    case Decision_Status_INVALID:
        printf(SS_Fmt ": bad-decision-status: invalid STATUS '" SS_Fmt "' in DECISION.md (use ACCEPTED or 'SUPERSEDED by <ref>')\n",
               SS_Arg(*huid), SS_Arg(value));
        findings++;
        break;
    case Decision_Status_MISSING:
        printf(SS_Fmt ": bad-decision-status: DECISION.md has no STATUS line\n", SS_Arg(*huid));
        findings++;
        break;
    }

    // dangling-supersede: every "- Supersedes: <ref>" header must resolve.
    Aids_String_Slice scan = *decision;
    while (aids_string_slice_tokenize(&scan, '\n', &line)) {
        Aids_String_Slice l = line;
        if (!aids_string_slice_starts_with(&l, supersedes_format)) {
            continue;
        }
        aids_string_slice_skip(&l, supersedes_format.len);
        Aids_String_Slice header_ref = artifact_strip_inline_comment(l);
        aids_string_slice_trim(&header_ref);
        if (header_ref.len == 0) {
            continue; // an empty header is treated as absent, not dangling
        }
        if (!check_supersede_ref_resolves(tasks_dir, header_ref)) {
            printf(SS_Fmt ": dangling-supersede: Supersedes '" SS_Fmt "' which has no DECISION.md\n",
                   SS_Arg(*huid), SS_Arg(header_ref));
            findings++;
        }
    }
    return findings;
}

// Lints one task directory. Returns the number of findings printed.
static size_t check_task(const Aids_String_Slice *tasks_dir,
                         const Aids_String_Slice *huid) {
    size_t findings = 0;
    Aids_String_Slice raw = {0};
    Aids_String_Slice review = {0};
    boolean has_review = false;
    Task task = {0};
    boolean task_loaded = false;

    if (!task_sibling_read(tasks_dir, huid, TASK_FILE_NAME_CSTR, &raw)) {
        printf(SS_Fmt ": malformed-header: TASK.md missing or unreadable\n", SS_Arg(*huid));
        findings++;
        goto review_checks;
    }

    task_init_empty(&task);
    if (task_deserialize(raw, &task) != AIDS_OK) {
        printf(SS_Fmt ": malformed-header: TASK.md failed to parse (title and v2 metadata block)\n", SS_Arg(*huid));
        findings++;
        AIDS_FREE(raw.str);
        raw = (Aids_String_Slice){0};
        goto review_checks;
    }
    task._buffer = raw.str; // task now owns the buffer
    task_loaded = true;

    // Metadata values no longer need a re-scan here: the v2 parser validates
    // the exact token it consumes (trailing spaces and CRLF tails included),
    // so an invalid STATUS, KIND, FLOW STEP or PLAN STATUS has already been
    // reported above as a parse failure.
    if (task.meta.status == Task_Status_IN_PROGRESS &&
        task.meta.plan_status != Plan_Status_APPROVED &&
        !check_task_is_container(&task)) {
        printf(SS_Fmt ": unplanned-in-progress: IN_PROGRESS task lacks PLAN STATUS: APPROVED\n",
               SS_Arg(*huid));
        findings++;
    }

    if (task.meta.status == Task_Status_CLOSED && !check_task_is_container(&task)) {
        size_t unchecked = artifact_count_unchecked_steps(raw);
        if (unchecked > 0) {
            printf(SS_Fmt ": closed-unchecked: %zu unchecked Steps item(s) on a CLOSED task\n",
                   SS_Arg(*huid), unchecked);
            findings++;
        }
    }

review_checks:
    has_review = task_sibling_read(tasks_dir, huid, "REVIEW.md", &review);

    if (has_review) {
        Aids_String_Slice approve = aids_string_slice_from_cstr("APPROVE");
        Aids_String_Slice last_verdict = {0};
        boolean has_verdict = artifact_latest_verdict(review, &last_verdict);
        if (task_loaded && task.meta.status == Task_Status_CLOSED) {
            if (!has_verdict) {
                printf(SS_Fmt ": closed-not-approved: REVIEW.md has no VERDICT line\n", SS_Arg(*huid));
                findings++;
            } else if (aids_string_slice_compare(&last_verdict, &approve) != 0) {
                printf(SS_Fmt ": closed-not-approved: latest REVIEW.md verdict is '" SS_Fmt "'\n",
                       SS_Arg(*huid), SS_Arg(last_verdict));
                findings++;
            }
        }

        // bad-severity: "- [ ] R1.2 (LOW) ..." with a severity outside the
        // vocabulary.
        Aids_String_Slice scan = review;
        Aids_String_Slice line = {0};
        while (aids_string_slice_tokenize(&scan, '\n', &line)) {
            Aids_String_Slice severity = {0};
            boolean resolved = false;
            if (!artifact_parse_finding_line(line, &severity, &resolved)) {
                continue;
            }
            if (!artifact_severity_is_known(severity)) {
                printf(SS_Fmt ": bad-severity: unknown severity '" SS_Fmt "' in REVIEW.md (use BLOCKER|MAJOR|MINOR|NIT)\n",
                       SS_Arg(*huid), SS_Arg(severity));
                findings++;
            }
        }
    }

    if (task_loaded && task.meta.status == Task_Status_CLOSED &&
        !check_task_is_container(&task)) {
        if (!has_review) {
            printf(SS_Fmt ": closed-missing-review: CLOSED task has no REVIEW.md\n", SS_Arg(*huid));
            findings++;
        }
        if (!task_sibling_exists(tasks_dir, huid, "RETRO.md")) {
            printf(SS_Fmt ": closed-missing-retro: CLOSED task has no RETRO.md\n", SS_Arg(*huid));
            findings++;
        }
    }

    // DECISION.md checks (bad-decision-status / dangling-supersede) are
    // presence-gated and independent of TASK.md validity: they only fire when
    // the sibling exists, so a task without one is never touched.
    {
        Aids_String_Slice decision = {0};
        if (task_sibling_read(tasks_dir, huid, "DECISION.md", &decision)) {
            findings += check_decision(tasks_dir, huid, &decision);
            AIDS_FREE(decision.str);
        }
    }

    if (task_loaded) {
        task_cleanup(&task); // frees raw via task->_buffer
    } else if (raw.str != NULL) {
        AIDS_FREE(raw.str);
    }
    if (has_review && review.str != NULL) {
        AIDS_FREE(review.str);
    }
    return findings;
}

// Lints the lessons ledger: a lesson at three or more occurrences must sit
// in the "## Pending promotions" section. Returns findings printed.
static size_t check_ledger(const Aids_String_Slice *cwd, const char *ledger_arg) {
    size_t findings = 0;
    Aids_String_Slice content = {0};
    char path_buffer[PATH_MAX];

    if (ledger_arg[0] == '/') {
        if (snprintf(path_buffer, sizeof(path_buffer), "%s", ledger_arg) < 0) {
            return 0;
        }
    } else if (snprintf(path_buffer, sizeof(path_buffer), SS_Fmt "/%s",
                        SS_Arg(*cwd), ledger_arg) < 0) {
        return 0;
    }

    Aids_String_Slice path = aids_string_slice_from_cstr(path_buffer);
    // fopen on a directory succeeds on Linux and reads as empty; a
    // directory must be a finding, not a silently clean ledger.
    if (aids_io_isdir(&path) || aids_io_read(&path, &content, "r") != AIDS_OK) {
        printf("ledger: unreadable: '%s'\n", ledger_arg);
        return 1;
    }

    Aids_String_Slice scan = content;
    Aids_String_Slice line = {0};
    Aids_String_Slice any_heading = aids_string_slice_from_cstr("## ");
    boolean in_pending = false;
    while (aids_string_slice_tokenize(&scan, '\n', &line)) {
        Aids_String_Slice l = line;
        aids_string_slice_trim(&l);
        if (aids_string_slice_starts_with(&l, any_heading)) {
            in_pending = slice_contains_cstr(l, "Pending promotions");
            continue;
        }
        if (in_pending) {
            continue;
        }
        // Find "(xN)" with N >= 3.
        for (unsigned long i = 0; i + 3 < l.len; ++i) {
            if (l.str[i] != '(' || l.str[i + 1] != 'x') {
                continue;
            }
            unsigned long j = i + 2;
            unsigned long count = 0;
            while (j < l.len && l.str[j] >= '0' && l.str[j] <= '9') {
                count = count * 10 + (unsigned long)(l.str[j] - '0');
                j++;
            }
            if (j >= l.len || l.str[j] != ')' || j <= i + 2) {
                // "(x-axis)", "(x2, enforced)": not a counter here - keep
                // scanning; a real one may follow later on the line.
                continue;
            }
            if (count >= 3) {
                // Slug is the first `backticked` token on the line, if any.
                Aids_String_Slice slug = aids_string_slice_from_cstr("(unnamed)");
                for (unsigned long a = 0; a < l.len; ++a) {
                    if (l.str[a] != '`') {
                        continue;
                    }
                    unsigned long b = a + 1;
                    while (b < l.len && l.str[b] != '`') {
                        b++;
                    }
                    if (b < l.len) {
                        slug = aids_string_slice_from_parts(l.str + a + 1, b - a - 1);
                    }
                    break;
                }
                printf("ledger: promotion-stalled: " SS_Fmt " (x%lu) is outside Pending promotions\n",
                       SS_Arg(slug), count);
                findings++;
            }
            break; // one count per line
        }
    }

    AIDS_FREE(content.str);
    return findings;
}

static int main_check(const Tatr_Context *ctx) {
    int result = 0;
    Argparse_Parser parser = {0};
    Aids_String_Slice tasks_dir = {0};
    Aids_String_Slice task_file_path = {0};
    Aids_Array project_dirs = {0}; /* Aids_String_Slice */
    boolean project_dirs_initialized = false;
    size_t findings = 0;

    argparse_parser_init(&parser, "tatr check", "Lint task artifacts for process drift", TATR_VERSION);

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'I',
        .long_name = "id",
        .description = "Task ID to check (default: every task)",
        .type = ARGUMENT_TYPE_POSITIONAL,
        .required = 0
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'L',
        .long_name = "ledger",
        .description = "Also lint a lessons ledger file (path relative to the root)",
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    if (argparse_parse(&parser, ctx->argc, ctx->argv) != ARG_OK) {
        return_defer(1);
    }

    char *ledger_arg = argparse_get_value(&parser, "ledger");
    char *id_str = argparse_get_value(&parser, "id");

    if (id_str != NULL) {
        Aids_String_Slice id = aids_string_slice_from_cstr(id_str);
        if (task_resolve(&ctx->cwd, &id, &tasks_dir, &task_file_path) != AIDS_OK) {
            return_defer(1);
        }
        findings += check_task(&tasks_dir, &id);
    } else {
        aids_array_init(&project_dirs, sizeof(Aids_String_Slice));
        project_dirs_initialized = true;
        if (find_current_tasks_dir(&ctx->cwd, &project_dirs) != AIDS_OK) {
            return_defer(1);
        }
        for (size_t i = 0; i < project_dirs.count; ++i) {
            Aids_String_Slice *dir = NULL;
            if (aids_array_get(&project_dirs, i, (void **)&dir) != AIDS_OK) {
                continue;
            }
            // find_current_tasks_dir returns PROJECT dirs (the "/tasks"
            // suffix stripped); re-append it for listing and path building.
            char tasks_path_buffer[PATH_MAX];
            if (snprintf(tasks_path_buffer, sizeof(tasks_path_buffer),
                         SS_Fmt "/%s", SS_Arg(*dir), TASKS_PATH_CSTR) < 0) {
                continue;
            }
            Aids_String_Slice tasks_path = aids_string_slice_from_cstr(tasks_path_buffer);
            Aids_Array entries = {0};
            aids_array_init(&entries, sizeof(Aids_String_Slice));
            if (aids_io_listdir(&tasks_path, &entries) != AIDS_OK) {
                aids_log(AIDS_ERROR, "Failed to list tasks directory: %s", aids_failure_reason());
                cleanup_string_slice_array(&entries);
                return_defer(1);
            }
            aids_array_sort(&entries, string_slice_compare_fn);
            for (size_t j = 0; j < entries.count; ++j) {
                Aids_String_Slice *huid_entry = NULL;
                if (aids_array_get(&entries, j, (void **)&huid_entry) != AIDS_OK) {
                    continue;
                }
                if (!ishuid(huid_entry)) {
                    continue;
                }
                findings += check_task(&tasks_path, huid_entry);
            }
            cleanup_string_slice_array(&entries);
        }
    }

    if (ledger_arg != NULL) {
        findings += check_ledger(&ctx->cwd, ledger_arg);
    }

    if (findings > 0) {
        result = 1;
    }

defer:
    if (tasks_dir.str != NULL) {
        AIDS_FREE(tasks_dir.str);
    }
    if (task_file_path.str != NULL) {
        AIDS_FREE(task_file_path.str);
    }
    if (project_dirs_initialized) {
        cleanup_string_slice_array(&project_dirs);
    }
    argparse_parser_free(&parser);
    return result;
}

static void tatr_print_help(Argparse_Parser *parser) {
    fprintf(stderr, "Usage: %s [-r ROOT] <subcommand> [options]\n", parser->name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Global options:\n");
    fprintf(stderr, "  -r, --root     Change working directory before running command\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Subcommands:\n");
    fprintf(stderr, "  help         Show this help message\n");
    fprintf(stderr, "  version      Show version information\n");
    fprintf(stderr, "  new          Create a new task\n");
    fprintf(stderr, "  ls           List tasks\n");
    fprintf(stderr, "  show         Show a single task by ID\n");
    fprintf(stderr, "  edit         Update fields of an existing task\n");
    fprintf(stderr, "  flow         Move a task to its next flow step\n");
    fprintf(stderr, "  rm           Remove a task by ID\n");
    fprintf(stderr, "  check        Lint task artifacts for process drift\n");
    fprintf(stderr, "\n");
}

static void tatr_print_version(Argparse_Parser *parser) {
    printf("%s %s\n", parser->name, TATR_VERSION);
}

int main(int argc, char **argv) {
    int result = 0;
    Argparse_Parser parser = {0};
    Tatr_Context ctx = {0};
    Aids_String_Slice cwd_allocated = {0};

    argparse_parser_init(&parser, "tatr", "Task tracker", TATR_VERSION);
    parser.help_fn = tatr_print_help;
    parser.version_fn = tatr_print_version;

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'r',
        .long_name = "root",
        .description = "Change working directory",
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'c',
        .long_name = "command",
        .description = "Subcommand to execute",
        .type = ARGUMENT_TYPE_SUBCOMMAND,
        .required = 1
    });

    if (argparse_parse(&parser, argc, argv) != ARG_OK) {
        tatr_print_help(&parser);
        return_defer(1);
    }

    char *root_str = argparse_get_value(&parser, "root");
    if (root_str != NULL) {
        ctx.cwd = aids_string_slice_from_cstr(root_str);
        aids_string_slice_trim_char_right(&ctx.cwd, '/');
    } else {
        if (aids_io_getcwd(&cwd_allocated) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to get current working directory: %s", aids_failure_reason());
            return_defer(1);
        }
        ctx.cwd = cwd_allocated;
    }

    unsigned long subcommand_offset = 0;
    char *subcommand = argparse_get_subcommand(&parser, "command", &subcommand_offset);
    if (subcommand == NULL) {
        tatr_print_help(&parser);
        return_defer(1);
    }

    char **new_argv = argv + subcommand_offset;
    int new_argc = argc - subcommand_offset;
    ctx.argv = new_argv;
    ctx.argc = new_argc;

    if (strcmp(subcommand, "help") == 0) {
        tatr_print_help(&parser);
        return_defer(0);
    } else if (strcmp(subcommand, "version") == 0) {
        tatr_print_version(&parser);
        return_defer(0);
    } else if (strcmp(subcommand, "new") == 0) {
        result = main_new(&ctx);
        return_defer(result);
    } else if (strcmp(subcommand, "ls") == 0) {
        result = main_ls(&ctx);
        return_defer(result);
    } else if (strcmp(subcommand, "show") == 0) {
        result = main_show(&ctx);
        return_defer(result);
    } else if (strcmp(subcommand, "edit") == 0) {
        result = main_edit(&ctx);
        return_defer(result);
    } else if (strcmp(subcommand, "flow") == 0) {
        result = main_flow(&ctx);
        return_defer(result);
    } else if (strcmp(subcommand, "rm") == 0) {
        result = main_rm(&ctx);
        return_defer(result);
    } else if (strcmp(subcommand, "check") == 0) {
        result = main_check(&ctx);
        return_defer(result);
    } else {
        fprintf(stderr, "Unknown subcommand: %s\n", subcommand);
        tatr_print_help(&parser);
        return_defer(1);
    }

defer:
    if (cwd_allocated.str != NULL) {
        AIDS_FREE(cwd_allocated.str);
    }
    argparse_parser_free(&parser);
    return result;
}

#define ARGPARSE_IMPLEMENTATION
#include "argparse.h"
#define AIDS_IMPLEMENTATION
#include "aids.h"
