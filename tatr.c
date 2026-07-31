#include "aids.h"
#include "argparse.h"
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#define TATR_VERSION "0.1.0"

// Format-checks a vararg reporter, so a message built with SS_Fmt but missing
// its SS_Arg is a compile error rather than a garbage finding. MinGW's default
// printf archetype is msvcrt's, which does not know %zu; the gnu archetype
// matches the ANSI stdio the Windows build actually links, and without it
// every %zu in a checked reporter becomes a warning under `make windows`.
#if defined(__MINGW_PRINTF_FORMAT)
#define TATR_PRINTF_FORMAT(fmt_index, first_arg) \
    __attribute__((format(__MINGW_PRINTF_FORMAT, fmt_index, first_arg)))
#elif defined(__GNUC__)
#define TATR_PRINTF_FORMAT(fmt_index, first_arg) \
    __attribute__((format(printf, fmt_index, first_arg)))
#else
#define TATR_PRINTF_FORMAT(fmt_index, first_arg)
#endif

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

// Defined with the artifact scans below. `new` needs them to check that a
// PARENT or DEPENDS ON reference resolves before it creates a record whose
// relationships the lint would reject on sight.
static boolean task_sibling_read(const Aids_String_Slice *tasks_dir,
                                 const Aids_String_Slice *huid,
                                 const char *name,
                                 Aids_String_Slice *content);
static boolean task_sibling_exists(const Aids_String_Slice *tasks_dir,
                                   const Aids_String_Slice *huid,
                                   const char *name);

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

// Checks that a new task's PARENT and DEPENDS ON name records that exist, and
// that a Story is given the Epic it belongs to. Returns false after logging.
// This is the same question graph_node_problems asks of an existing record; it
// is asked here as well because `new` is the one producer that could otherwise
// create a task the lint rejects on sight, and there is no gate between the
// two.
static boolean new_relationships_resolve(const Aids_String_Slice *cwd, const Task *task) {
    Aids_String_Slice tasks_dir = {0};
    boolean ok = true;

    if (task->meta.parent.len == 0 && task->meta.depends_on.count == 0) {
        if (task->meta.kind == Task_Kind_STORY) {
            aids_log(AIDS_ERROR, "A KIND: STORY belongs to an Epic: pass -P/--parent <epic id>");
            return false;
        }
        return true; // nothing to resolve, so no tasks dir needed
    }
    if (tasks_dir_path_build(cwd, &tasks_dir) != AIDS_OK) {
        return false;
    }

    if (task->meta.parent.len > 0) {
        Aids_String_Slice raw = {0};
        if (!task_sibling_read(&tasks_dir, &task->meta.parent, TASK_FILE_NAME_CSTR, &raw)) {
            aids_log(AIDS_ERROR, "Parent '" SS_Fmt "' does not exist", SS_Arg(task->meta.parent));
            ok = false;
        } else {
            Task parent = {0};
            task_init_empty(&parent);
            if (task_deserialize(raw, &parent) != AIDS_OK) {
                aids_log(AIDS_ERROR, "Parent '" SS_Fmt "' does not parse", SS_Arg(task->meta.parent));
                AIDS_FREE(raw.str);
                ok = false;
            } else {
                parent._buffer = raw.str;
                if (parent.meta.kind != Task_Kind_EPIC) {
                    aids_log(AIDS_ERROR, "Parent '" SS_Fmt "' is KIND: " SS_Fmt ", not EPIC: only a container has children",
                             SS_Arg(task->meta.parent), SS_Arg(Task_Kind_Strings[parent.meta.kind]));
                    ok = false;
                }
            }
            task_cleanup(&parent);
        }
    } else if (task->meta.kind == Task_Kind_STORY) {
        aids_log(AIDS_ERROR, "A KIND: STORY belongs to an Epic: pass -P/--parent <epic id>");
        ok = false;
    }

    for (size_t i = 0; i < task->meta.depends_on.count; ++i) {
        Aids_String_Slice *dep = NULL;
        if (aids_array_get((Aids_Array *)&task->meta.depends_on, i, (void **)&dep) != AIDS_OK) {
            continue;
        }
        if (!task_sibling_exists(&tasks_dir, dep, TASK_FILE_NAME_CSTR)) {
            aids_log(AIDS_ERROR, "Dependency '" SS_Fmt "' does not exist", SS_Arg(*dep));
            ok = false;
        }
    }

    AIDS_FREE(tasks_dir.str);
    return ok;
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

    // A relationship the graph rules would reject is refused HERE, before the
    // record exists: `tatr new` must not mint a task that `tatr check` flags
    // the moment it is created, for the same reason the plan gate owns the
    // plan sections rather than `new` doing. Checked after the body is read so
    // that a bad path and a bad reference fail the same way - creating nothing.
    if (!new_relationships_resolve(&ctx->cwd, &task)) {
        return_defer(1);
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

    // Only when this edit SETS a relationship or a kind: an edit that touches
    // the title must not be blocked by a dangling reference it did not create,
    // but an edit that writes one is the same producer `new` is.
    if ((argparse_get_value(&parser, "parent") != NULL ||
         argparse_get_value(&parser, "kind") != NULL ||
         argparse_get_values(&parser, "depends-on", (char *[ARGPARSE_CAPACITY]){0}) > 0) &&
        !new_relationships_resolve(&ctx->cwd, &task)) {
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
// Record schemas: the one in-code source for what each sibling record looks
// like. `tatr scaffold` writes from this table and `tatr check` validates
// against it, so a format change is one edit and the scaffolder can never emit
// a record the linter rejects. Skill prose points at the CLI instead of
// carrying its own copy of the template.
// ---------------------------------------------------------------------------

#define RECORD_MAX_FIELDS 6
#define RECORD_MAX_SECTIONS 8

typedef enum {
    Record_Kind_TASK,
    Record_Kind_SPIKE,
    Record_Kind_DECISION,
    Record_Kind_REVIEW,
    Record_Kind_RETRO
} Record_Kind;

typedef struct {
    const char *name;         // "REVIEW", as written on the command line
    const char *file_name;    // "REVIEW.md"
    const char *title_prefix; // the exact "# " line prefix the record opens with
    // Required "- KEY: " header lines, NULL-terminated. Each must be present
    // with a non-empty value.
    const char *fields[RECORD_MAX_FIELDS];
    // Required "## " headings, NULL-terminated. Each must be present with at
    // least one non-blank line under it.
    const char *sections[RECORD_MAX_SECTIONS];
    // Everything after the header block in a scaffolded record, or NULL when
    // the sections list generates it. TASK.md alone is not scaffoldable here:
    // `tatr new` creates it, and it is typed metadata rather than prose.
    const char *body_template;
} Record_Schema;

// REVIEW.md's body is round-structured rather than section-structured, so its
// shape is validated by review_round_problems and scaffolded from this literal.
#define REVIEW_BODY_TEMPLATE \
    "## Round 1\n" \
    "\n" \
    "- REVIEWER: TODO\n" \
    "- VERDICT: REQUEST_CHANGES\n" \
    "\n" \
    "- [ ] R1.1 (MAJOR) file:line - TODO\n"

static const Record_Schema RECORD_SCHEMAS[] = {
    [Record_Kind_TASK] = {
        .name = "TASK",
        .file_name = TASK_FILE_NAME_CSTR,
        .title_prefix = "# ",
        .fields = {NULL},
        // TASK sections are kind-specific; see task_required_sections.
        .sections = {NULL},
        .body_template = NULL,
    },
    [Record_Kind_SPIKE] = {
        .name = "SPIKE",
        .file_name = "SPIKE.md",
        .title_prefix = "# Spike: ",
        .fields = {"- DATE: ", "- STATUS: ", "- TAGS: ", NULL},
        .sections = {"## Question", "## Context", "## Options considered",
                     "## Recommendation", "## Open questions", "## Next steps", NULL},
        .body_template = NULL,
    },
    [Record_Kind_DECISION] = {
        .name = "DECISION",
        .file_name = "DECISION.md",
        .title_prefix = "# Decision: ",
        .fields = {"- DATE: ", "- STATUS: ", "- TASK: ", "- TAGS: ", NULL},
        .sections = {"## Context", "## Decision", "## Alternatives considered",
                     "## Consequences", NULL},
        .body_template = NULL,
    },
    [Record_Kind_REVIEW] = {
        .name = "REVIEW",
        .file_name = "REVIEW.md",
        .title_prefix = "# Review: ",
        .fields = {"- TASK: ", "- BRANCH: ", NULL},
        .sections = {NULL},
        .body_template = REVIEW_BODY_TEMPLATE,
    },
    [Record_Kind_RETRO] = {
        .name = "RETRO",
        .file_name = "RETRO.md",
        .title_prefix = "# Retro: ",
        .fields = {"- TASK: ", "- BRANCH: ", "- REVIEW ROUNDS: ", NULL},
        .sections = {"## What went well", "## What went wrong",
                     "## What to improve next time", "## Action items", NULL},
        .body_template = NULL,
    },
};

#define RECORD_KIND_COUNT ENUM_COUNT(RECORD_SCHEMAS)
#define RECORD_VALUES_CSTR "TASK, SPIKE, DECISION, REVIEW or RETRO"

static boolean record_kind_from_string(const Aids_String_Slice *slice, Record_Kind *out) {
    for (size_t i = 0; i < RECORD_KIND_COUNT; ++i) {
        Aids_String_Slice name = aids_string_slice_from_cstr((char *)RECORD_SCHEMAS[i].name);
        if (aids_string_slice_compare(slice, &name) == 0) {
            *out = (Record_Kind)i;
            return true;
        }
    }
    return false;
}

// The allowed values of a SPIKE.md's "- STATUS: " line, as the spike skill
// defines them: the exploration either landed on a direction, could not answer
// its question, or ruled the idea out. There is no fourth answer.
static Aids_String_Slice Spike_Status_Strings[] = {
    (Aids_String_Slice) { .str = (unsigned char *)"RECOMMENDED", .len = 11 },
    (Aids_String_Slice) { .str = (unsigned char *)"INCONCLUSIVE", .len = 12 },
    (Aids_String_Slice) { .str = (unsigned char *)"DROPPED", .len = 7 }
};

#define SPIKE_STATUS_VALUES_CSTR "RECOMMENDED, INCONCLUSIVE or DROPPED"

// The allowed values of a REVIEW.md round's "- VERDICT: " line.
static Aids_String_Slice Verdict_Strings[] = {
    (Aids_String_Slice) { .str = (unsigned char *)"APPROVE", .len = 7 },
    (Aids_String_Slice) { .str = (unsigned char *)"REQUEST_CHANGES", .len = 15 }
};

#define VERDICT_VALUES_CSTR "APPROVE or REQUEST_CHANGES"

// The "## " sections a TASK.md must carry, by kind. An Epic container's record
// IS its own TASK.md - the done definition and the child queue live there -
// while a work task carries the Steps and the proofs. A SPIKE task's research
// lives in its SPIKE.md sibling, so its TASK.md is only asked for a Question.
static const char *const *task_required_sections(Task_Kind kind) {
    static const char *work[] = {"## Steps", "## Definition of Done", NULL};
    static const char *epic[] = {"## Done Means", "## Child Tasks", NULL};
    static const char *spike[] = {"## Question", NULL};
    switch (kind) {
    case Task_Kind_EPIC:  return epic;
    case Task_Kind_SPIKE: return spike;
    case Task_Kind_TASK:
    case Task_Kind_STORY: return work;
    }
    return work; // unreachable: the enum is closed
}

// The value of a "- KEY: " line, with any inline comment stripped and trimmed.
// First occurrence wins, matching artifact_decision_status. Returns false when
// the record carries no such line at all; an empty value returns true with a
// zero-length slice, so the caller can tell "absent" from "blank".
static boolean artifact_field(Aids_String_Slice doc, const char *key,
                              Aids_String_Slice *out) {
    Aids_String_Slice format = aids_string_slice_from_cstr((char *)key);
    Aids_String_Slice key_only = format;
    key_only.len -= 1; // the trailing space is optional when the value is empty
    Aids_String_Slice scan = doc;
    Aids_String_Slice line = {0};
    while (aids_string_slice_tokenize(&scan, '\n', &line)) {
        Aids_String_Slice l = line;
        if (!aids_string_slice_starts_with(&l, key_only)) {
            continue;
        }
        aids_string_slice_skip(&l, key_only.len);
        Aids_String_Slice value = artifact_strip_inline_comment(l);
        aids_string_slice_trim(&value);
        *out = value;
        return true;
    }
    return false;
}

// Whether a "## <heading>" section is present, and whether it carries at least
// one non-blank line before the next "## " heading. The heading match is exact
// (after trimming the right edge), so "## Steps taken later" is a different
// section - the same rule artifact_count_unchecked_steps already follows.
static void artifact_section_state(Aids_String_Slice doc, const char *heading,
                                   boolean *present, boolean *nonempty) {
    Aids_String_Slice want = aids_string_slice_from_cstr((char *)heading);
    Aids_String_Slice any_heading = aids_string_slice_from_cstr("## ");
    Aids_String_Slice scan = doc;
    Aids_String_Slice line = {0};
    boolean inside = false;
    *present = false;
    *nonempty = false;
    while (aids_string_slice_tokenize(&scan, '\n', &line)) {
        Aids_String_Slice l = line;
        aids_string_slice_trim_right(&l);
        if (aids_string_slice_starts_with(&l, any_heading)) {
            if (inside) {
                return; // the section ended; whatever we saw is the answer
            }
            inside = aids_string_slice_compare(&l, &want) == 0;
            if (inside) {
                *present = true;
            }
            continue;
        }
        if (inside && l.len > 0) {
            *nonempty = true;
        }
    }
}

// A REVIEW.md finding line, parsed strictly: "- [ ] R<round>.<index> (SEV) ...".
// The tolerant artifact_parse_finding_line above decides whether a line IS a
// finding; this one decides whether its ID is well-formed, so a malformed ID is
// reported rather than silently skipped.
typedef struct {
    unsigned long round;
    unsigned long index;
} Artifact_Finding_Id;

static boolean artifact_parse_finding_id(Aids_String_Slice line,
                                         Artifact_Finding_Id *out) {
    Aids_String_Slice l = line;
    aids_string_slice_trim(&l);
    Aids_String_Slice box = aids_string_slice_from_cstr("- [ ] R");
    if (l.len < box.len) {
        return false;
    }
    aids_string_slice_skip(&l, box.len); // "- [x] R" has the same length
    unsigned long i = 0;
    unsigned long round = 0;
    while (i < l.len && isdigit(l.str[i])) {
        round = round * 10 + (unsigned long)(l.str[i] - '0');
        i++;
    }
    if (i == 0 || i >= l.len || l.str[i] != '.') {
        return false;
    }
    i++;
    unsigned long digits = 0;
    unsigned long index = 0;
    while (i < l.len && isdigit(l.str[i])) {
        index = index * 10 + (unsigned long)(l.str[i] - '0');
        i++;
        digits++;
    }
    if (digits == 0) {
        return false;
    }
    out->round = round;
    out->index = index;
    return true;
}

// The proof kinds a "## Definition of Done" item may carry. tatr parses them
// and prints them; it never runs them. A `cmd:` proof is shell text that
// round-trips verbatim, and the decision to execute it belongs to the caller's
// shell, where the user can see the command.
typedef enum {
    Proof_Kind_TEST,
    Proof_Kind_CMD,
    Proof_Kind_MANUAL
} Proof_Kind;

static const char *PROOF_KIND_NAMES[] = {
    [Proof_Kind_TEST] = "test",
    [Proof_Kind_CMD] = "cmd",
    [Proof_Kind_MANUAL] = "manual"
};

#define PROOF_MARKERS_CSTR "test:, cmd: or manual:"

// Finds the next "(<kind>: <text>)" proof at or after *cursor inside one
// Definition of Done item. Returns false when the item holds no further proof.
// The text may span lines - a wrapped bullet is contiguous in the buffer - so
// the scan runs to the matching ')' wherever it lands.
static boolean artifact_next_proof(Aids_String_Slice item, size_t *cursor,
                                   Proof_Kind *kind, Aids_String_Slice *text) {
    while (*cursor < item.len) {
        if (item.str[*cursor] != '(') {
            (*cursor)++;
            continue;
        }
        size_t open = *cursor;
        Aids_String_Slice rest = aids_string_slice_from_parts(item.str + open + 1,
                                                              item.len - open - 1);
        boolean matched = false;
        for (size_t k = 0; k < ENUM_COUNT(PROOF_KIND_NAMES); ++k) {
            char marker_buffer[16];
            snprintf(marker_buffer, sizeof(marker_buffer), "%s:", PROOF_KIND_NAMES[k]);
            Aids_String_Slice marker = aids_string_slice_from_cstr(marker_buffer);
            if (!aids_string_slice_starts_with(&rest, marker)) {
                continue;
            }
            size_t close = open + 1 + marker.len;
            int depth = 1;
            while (close < item.len) {
                if (item.str[close] == '(') {
                    depth++;
                } else if (item.str[close] == ')') {
                    depth--;
                    if (depth == 0) {
                        break;
                    }
                }
                close++;
            }
            if (close >= item.len) {
                break; // an unclosed proof group is not a proof
            }
            Aids_String_Slice value = aids_string_slice_from_parts(
                item.str + open + 1 + marker.len,
                close - (open + 1 + marker.len));
            aids_string_slice_trim(&value);
            *kind = (Proof_Kind)k;
            *text = value;
            *cursor = close + 1;
            matched = true;
            break;
        }
        if (matched) {
            return true;
        }
        (*cursor)++;
    }
    return false;
}

// Walks the "- " items of the "## Definition of Done" section. An item runs
// from its bullet line through every continuation line that follows, so a
// wrapped proof is still one contiguous slice. Returns false when the section
// holds no further item; *cursor is opaque state seeded to 0.
static boolean artifact_next_dod_item(Aids_String_Slice task_raw, size_t *cursor,
                                      Aids_String_Slice *item) {
    Aids_String_Slice heading = aids_string_slice_from_cstr("## Definition of Done");
    Aids_String_Slice any_heading = aids_string_slice_from_cstr("## ");
    Aids_String_Slice bullet = aids_string_slice_from_cstr("- ");

    // Locate the section body once per call; the cursor is a byte offset into
    // task_raw, so restarting the scan is cheap and keeps the state a plain
    // integer the caller can seed with 0.
    size_t pos = 0;
    size_t section_start = 0;
    size_t section_end = task_raw.len;
    boolean found = false;
    while (pos < task_raw.len) {
        size_t line_end = pos;
        while (line_end < task_raw.len && task_raw.str[line_end] != '\n') {
            line_end++;
        }
        Aids_String_Slice line = aids_string_slice_from_parts(task_raw.str + pos,
                                                              line_end - pos);
        aids_string_slice_trim_right(&line);
        if (aids_string_slice_starts_with(&line, any_heading)) {
            if (found) {
                section_end = pos;
                break;
            }
            if (aids_string_slice_compare(&line, &heading) == 0) {
                found = true;
                section_start = line_end < task_raw.len ? line_end + 1 : task_raw.len;
            }
        }
        pos = line_end < task_raw.len ? line_end + 1 : task_raw.len;
    }
    if (!found) {
        return false;
    }

    size_t at = *cursor > section_start ? *cursor : section_start;
    while (at < section_end) {
        size_t line_end = at;
        while (line_end < section_end && task_raw.str[line_end] != '\n') {
            line_end++;
        }
        Aids_String_Slice line = aids_string_slice_from_parts(task_raw.str + at,
                                                              line_end - at);
        Aids_String_Slice trimmed = line;
        aids_string_slice_trim(&trimmed);
        if (!aids_string_slice_starts_with(&trimmed, bullet)) {
            at = line_end < section_end ? line_end + 1 : section_end;
            continue;
        }
        // Absorb the continuation lines: anything non-blank that does not open
        // a new bullet belongs to this item.
        size_t item_end = line_end;
        size_t next = line_end < section_end ? line_end + 1 : section_end;
        while (next < section_end) {
            size_t next_end = next;
            while (next_end < section_end && task_raw.str[next_end] != '\n') {
                next_end++;
            }
            Aids_String_Slice cont = aids_string_slice_from_parts(task_raw.str + next,
                                                                  next_end - next);
            Aids_String_Slice cont_trimmed = cont;
            aids_string_slice_trim(&cont_trimmed);
            if (cont_trimmed.len == 0 || aids_string_slice_starts_with(&cont_trimmed, bullet)) {
                break;
            }
            item_end = next_end;
            next = next_end < section_end ? next_end + 1 : section_end;
        }
        *item = aids_string_slice_from_parts(task_raw.str + at, item_end - at);
        *cursor = next;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Record validation, as a pure collector. Both `check` and `flow` ask these
// functions the same questions and get the same answers back as data: `check`
// prints each problem as a finding, `flow` collects them as unmet
// preconditions. That is what makes the tying invariant hold - a transition
// cannot produce a state the lint would flag, because neither side owns its
// own copy of a rule. Nothing here prints.
// ---------------------------------------------------------------------------

#define RECORD_PROBLEM_CAPACITY 32
#define RECORD_PROBLEM_MESSAGE_SIZE 256

typedef struct {
    const char *rule; // the `check` rule slug this problem would be reported as
    char message[RECORD_PROBLEM_MESSAGE_SIZE];
} Record_Problem;

typedef struct {
    Record_Problem items[RECORD_PROBLEM_CAPACITY];
    size_t count;
} Record_Problems;

static void record_problems_add(Record_Problems *problems, const char *rule,
                                const char *fmt, ...) TATR_PRINTF_FORMAT(3, 4);

static void record_problems_add(Record_Problems *problems, const char *rule,
                                const char *fmt, ...) {
    if (problems->count >= RECORD_PROBLEM_CAPACITY) {
        return; // a record with 32 problems has been told enough
    }
    problems->items[problems->count].rule = rule;
    va_list args;
    va_start(args, fmt);
    vsnprintf(problems->items[problems->count].message,
              RECORD_PROBLEM_MESSAGE_SIZE, fmt, args);
    va_end(args);
    problems->count++;
}

// bad-record-schema: the title prefix, the required "- KEY:" header fields and
// the required "## " sections a record kind declares in RECORD_SCHEMAS.
static void record_schema_problems(Record_Kind kind,
                                   Aids_String_Slice doc,
                                   const char *const *sections_override,
                                   Record_Problems *problems) {
    const Record_Schema *schema = &RECORD_SCHEMAS[kind];

    Aids_String_Slice prefix = aids_string_slice_from_cstr((char *)schema->title_prefix);
    Aids_String_Slice first = {0};
    Aids_String_Slice scan = doc;
    if (!aids_string_slice_tokenize(&scan, '\n', &first)) {
        first = doc;
    }
    aids_string_slice_trim_right(&first);
    if (!aids_string_slice_starts_with(&first, prefix) || first.len <= prefix.len) {
        record_problems_add(problems, "bad-record-schema",
                            "%s does not open with '%s<title>'",
                            schema->file_name, schema->title_prefix);
    }

    for (size_t i = 0; i < RECORD_MAX_FIELDS && schema->fields[i] != NULL; ++i) {
        Aids_String_Slice value = {0};
        if (!artifact_field(doc, schema->fields[i], &value)) {
            record_problems_add(problems, "bad-record-schema", "%s has no '%s' line",
                                schema->file_name, schema->fields[i]);
        } else if (value.len == 0) {
            record_problems_add(problems, "bad-record-schema", "%s has an empty '%s' value",
                                schema->file_name, schema->fields[i]);
        }
    }

    const char *const *sections = sections_override != NULL ? sections_override : schema->sections;
    for (size_t i = 0; sections[i] != NULL; ++i) {
        boolean present = false;
        boolean nonempty = false;
        artifact_section_state(doc, sections[i], &present, &nonempty);
        if (!present) {
            record_problems_add(problems, "bad-record-schema", "%s has no '%s' section",
                                schema->file_name, sections[i]);
        } else if (!nonempty) {
            record_problems_add(problems, "bad-record-schema", "%s section '%s' is empty",
                                schema->file_name, sections[i]);
        }
    }
}

// Checks that an enum-valued field carries one of its allowed values.
static void record_enum_problem(Record_Problems *problems, const char *rule,
                                const char *what, Aids_String_Slice value,
                                const Aids_String_Slice *table, size_t count,
                                const char *values_hint) {
    int index = 0;
    if (enum_from_string(&value, table, count, &index)) {
        return;
    }
    record_problems_add(problems, rule, "invalid %s '" SS_Fmt "' (use %s)",
                        what, SS_Arg(value), values_hint);
}

// bad-review-round / bad-verdict / missing-reviewer / bad-finding-id /
// bad-severity / approve-with-open-findings: everything a REVIEW.md's round
// structure is held to. Rounds are appended and numbered from 1; each carries a
// reviewer and one verdict from the vocabulary; each finding ID is
// R<this round>.<next index> and its severity is one of the four.
static void review_round_problems(Aids_String_Slice review, Record_Problems *problems) {
    Aids_String_Slice round_heading = aids_string_slice_from_cstr("## Round ");
    Aids_String_Slice reviewer_format = aids_string_slice_from_cstr("- REVIEWER: ");
    Aids_String_Slice verdict_format = aids_string_slice_from_cstr("- VERDICT: ");
    Aids_String_Slice approve = aids_string_slice_from_cstr("APPROVE");

    Aids_String_Slice scan = review;
    Aids_String_Slice line = {0};
    unsigned long expected_round = 1;
    unsigned long current_round = 0;
    unsigned long expected_finding = 1;
    boolean saw_reviewer = false;
    boolean saw_verdict = false;
    Aids_String_Slice last_verdict = {0};

    while (aids_string_slice_tokenize(&scan, '\n', &line)) {
        Aids_String_Slice l = line;
        aids_string_slice_trim_right(&l);

        if (aids_string_slice_starts_with(&l, round_heading)) {
            if (current_round > 0 && !saw_reviewer) {
                record_problems_add(problems, "missing-reviewer",
                                    "REVIEW.md round %lu has no '- REVIEWER: ' line",
                                    current_round);
            }
            if (current_round > 0 && !saw_verdict) {
                record_problems_add(problems, "bad-verdict",
                                    "REVIEW.md round %lu has no '- VERDICT: ' line",
                                    current_round);
            }
            Aids_String_Slice number = l;
            aids_string_slice_skip(&number, round_heading.len);
            unsigned long parsed = 0;
            unsigned long digits = 0;
            while (digits < number.len && isdigit(number.str[digits])) {
                parsed = parsed * 10 + (unsigned long)(number.str[digits] - '0');
                digits++;
            }
            if (digits == 0) {
                record_problems_add(problems, "bad-review-round",
                                    "REVIEW.md heading '" SS_Fmt "' has no round number",
                                    SS_Arg(l));
                parsed = expected_round;
            } else if (parsed != expected_round) {
                record_problems_add(problems, "bad-review-round",
                                    "REVIEW.md round %lu follows round %lu (rounds are numbered from 1 without gaps)",
                                    parsed, expected_round - 1);
            }
            current_round = parsed;
            expected_round = parsed + 1;
            expected_finding = 1;
            saw_reviewer = false;
            saw_verdict = false;
            continue;
        }

        if (aids_string_slice_starts_with(&l, reviewer_format)) {
            Aids_String_Slice value = l;
            aids_string_slice_skip(&value, reviewer_format.len);
            aids_string_slice_trim(&value);
            if (value.len == 0) {
                record_problems_add(problems, "missing-reviewer",
                                    "REVIEW.md round %lu has an empty REVIEWER", current_round);
            }
            saw_reviewer = true;
            continue;
        }

        if (aids_string_slice_starts_with(&l, verdict_format)) {
            Aids_String_Slice value = l;
            aids_string_slice_skip(&value, verdict_format.len);
            aids_string_slice_trim_left(&value);
            unsigned long tok = 0;
            while (tok < value.len && !isspace(value.str[tok])) {
                tok++;
            }
            value.len = tok;
            record_enum_problem(problems, "bad-verdict", "VERDICT", value,
                                Verdict_Strings, ENUM_COUNT(Verdict_Strings),
                                VERDICT_VALUES_CSTR);
            last_verdict = value;
            saw_verdict = true;
            continue;
        }

        Aids_String_Slice severity = {0};
        boolean resolved = false;
        if (!artifact_parse_finding_line(l, &severity, &resolved)) {
            continue;
        }
        if (!artifact_severity_is_known(severity)) {
            record_problems_add(problems, "bad-severity",
                                "unknown severity '" SS_Fmt "' in REVIEW.md (use BLOCKER|MAJOR|MINOR|NIT)",
                                SS_Arg(severity));
        }
        Artifact_Finding_Id id = {0};
        if (!artifact_parse_finding_id(l, &id)) {
            record_problems_add(problems, "bad-finding-id",
                                "REVIEW.md finding is not 'R<round>.<index>': " SS_Fmt,
                                SS_Arg(l));
            continue;
        }
        if (id.round != current_round) {
            record_problems_add(problems, "bad-finding-id",
                                "REVIEW.md finding R%lu.%lu sits in round %lu",
                                id.round, id.index, current_round);
        } else if (id.index != expected_finding) {
            record_problems_add(problems, "bad-finding-id",
                                "REVIEW.md finding R%lu.%lu follows R%lu.%lu (findings are numbered from 1 without gaps)",
                                id.round, id.index, current_round, expected_finding - 1);
            expected_finding = id.index;
        }
        expected_finding++;
    }

    if (current_round == 0) {
        record_problems_add(problems, "bad-review-round",
                            "REVIEW.md has no '## Round 1' heading");
        return;
    }
    if (!saw_reviewer) {
        record_problems_add(problems, "missing-reviewer",
                            "REVIEW.md round %lu has no '- REVIEWER: ' line", current_round);
    }
    if (!saw_verdict) {
        record_problems_add(problems, "bad-verdict",
                            "REVIEW.md round %lu has no '- VERDICT: ' line", current_round);
    }

    // A review that says APPROVE while a BLOCKER or MAJOR is still unticked
    // approves work it has itself declared unfinished.
    if (saw_verdict && aids_string_slice_compare(&last_verdict, &approve) == 0) {
        size_t open = artifact_count_open_blocking_findings(review);
        if (open > 0) {
            record_problems_add(problems, "approve-with-open-findings",
                                "REVIEW.md verdict is APPROVE with %lu open BLOCKER/MAJOR finding(s)",
                                (unsigned long)open);
        }
    }
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

// Extracts the first well-formed HUID path segment of a DECISION.md reference.
// The canonical form is "tasks/<id>/DECISION.md", but a bare "<id>" works too:
// every task is flat under one tasks dir.
static boolean check_ref_huid(Aids_String_Slice ref, Aids_String_Slice *out) {
    Aids_String_Slice scan = ref;
    Aids_String_Slice segment = {0};
    while (aids_string_slice_tokenize(&scan, '/', &segment)) {
        Aids_String_Slice s = segment;
        aids_string_slice_trim(&s);
        if (ishuid(&s)) {
            *out = s;
            return true;
        }
    }
    return false;
}

// Reads "<tasks_dir>/<ref huid>/DECISION.md" and reports whether it names
// <expect> on a "- Supersedes: " line. This is the back half of the reciprocal
// supersede rule: A saying it was superseded by B is only a record if B says it
// supersedes A. Returns false when the replacement has no DECISION.md at all,
// which the dangling-supersede rule reports separately.
static boolean check_supersede_is_reciprocal(const Aids_String_Slice *tasks_dir,
                                             Aids_String_Slice replacement,
                                             Aids_String_Slice expect) {
    char path_buffer[PATH_MAX];
    if (snprintf(path_buffer, sizeof(path_buffer), SS_Fmt "/" SS_Fmt "/DECISION.md",
                 SS_Arg(*tasks_dir), SS_Arg(replacement)) < 0) {
        return false;
    }
    if (access(path_buffer, F_OK) != 0) {
        return false;
    }
    Aids_String_Slice path = aids_string_slice_from_cstr(path_buffer);
    Aids_String_Slice content = {0};
    if (aids_io_read(&path, &content, "r") != AIDS_OK) {
        return false;
    }

    Aids_String_Slice supersedes_format = aids_string_slice_from_cstr("- Supersedes: ");
    Aids_String_Slice scan = content;
    Aids_String_Slice line = {0};
    boolean reciprocal = false;
    while (!reciprocal && aids_string_slice_tokenize(&scan, '\n', &line)) {
        Aids_String_Slice l = line;
        if (!aids_string_slice_starts_with(&l, supersedes_format)) {
            continue;
        }
        aids_string_slice_skip(&l, supersedes_format.len);
        Aids_String_Slice ref = artifact_strip_inline_comment(l);
        aids_string_slice_trim(&ref);
        Aids_String_Slice ref_huid = {0};
        if (check_ref_huid(ref, &ref_huid) &&
            aids_string_slice_compare(&ref_huid, &expect) == 0) {
            reciprocal = true;
        }
    }
    AIDS_FREE(content.str);
    return reciprocal;
}

// bad-spike-status / dangling-seeded-task, plus the SPIKE.md schema. Fires on
// PRESENCE: any SPIKE.md is validated, whatever the task's KIND, because
// `tatr scaffold <id> SPIKE` will write one for any task. Only the
// missing-spike-record rule is kind-gated, and that lives in check_task.
static void spike_record_problems(const Aids_String_Slice *tasks_dir,
                                  Aids_String_Slice spike,
                                  Record_Problems *problems) {
    record_schema_problems(Record_Kind_SPIKE, spike, NULL, problems);

    Aids_String_Slice status = {0};
    if (artifact_field(spike, "- STATUS: ", &status) && status.len > 0) {
        record_enum_problem(problems, "bad-spike-status", "SPIKE.md STATUS", status,
                            Spike_Status_Strings, ENUM_COUNT(Spike_Status_Strings),
                            SPIKE_STATUS_VALUES_CSTR);
    }

    // Every task ID named under "## Next steps" must resolve: a spike whose
    // seeded pointers dangle has recorded a direction nobody can pick up.
    Aids_String_Slice any_heading = aids_string_slice_from_cstr("## ");
    Aids_String_Slice want = aids_string_slice_from_cstr("## Next steps");
    Aids_String_Slice scan = spike;
    Aids_String_Slice line = {0};
    boolean inside = false;
    while (aids_string_slice_tokenize(&scan, '\n', &line)) {
        Aids_String_Slice l = line;
        aids_string_slice_trim_right(&l);
        if (aids_string_slice_starts_with(&l, any_heading)) {
            if (inside) {
                break;
            }
            inside = aids_string_slice_compare(&l, &want) == 0;
            continue;
        }
        if (!inside) {
            continue;
        }
        for (unsigned long i = 0; i + (HUID_LENGTH - 1) <= l.len; ++i) {
            Aids_String_Slice candidate = aids_string_slice_from_parts(l.str + i, HUID_LENGTH - 1);
            if (!ishuid(&candidate)) {
                continue;
            }
            char path_buffer[PATH_MAX];
            if (snprintf(path_buffer, sizeof(path_buffer), SS_Fmt "/" SS_Fmt "/%s",
                         SS_Arg(*tasks_dir), SS_Arg(candidate), TASK_FILE_NAME_CSTR) < 0) {
                continue;
            }
            if (access(path_buffer, F_OK) != 0) {
                record_problems_add(problems, "dangling-seeded-task",
                                    "SPIKE.md seeds '" SS_Fmt "' which has no TASK.md",
                                    SS_Arg(candidate));
            }
            i += HUID_LENGTH - 2; // consume the match
        }
    }
}

// bad-proof-syntax: every "## Definition of Done" item must carry at least one
// test:, cmd: or manual: proof. A criterion nothing can check is a wish.
static void dod_proof_problems(Aids_String_Slice task_raw, Record_Problems *problems) {
    size_t cursor = 0;
    Aids_String_Slice item = {0};
    while (artifact_next_dod_item(task_raw, &cursor, &item)) {
        size_t proof_cursor = 0;
        Proof_Kind kind = Proof_Kind_TEST;
        Aids_String_Slice text = {0};
        if (artifact_next_proof(item, &proof_cursor, &kind, &text)) {
            continue;
        }
        // Name the item by its first line, so the problem points at a place.
        Aids_String_Slice first = item;
        Aids_String_Slice head = {0};
        if (!aids_string_slice_tokenize(&first, '\n', &head)) {
            head = item;
        }
        aids_string_slice_trim(&head);
        record_problems_add(problems, "bad-proof-syntax",
                            "Definition of Done item has no %s proof: " SS_Fmt,
                            PROOF_MARKERS_CSTR, SS_Arg(head));
    }
}

// bad-record-schema / dangling-decision-task / bad-decision-status /
// dangling-supersede / nonreciprocal-supersede: everything a DECISION.md is
// held to. Fires by presence only, so a task without one is never touched.
static void decision_record_problems(const Aids_String_Slice *tasks_dir,
                                     const Aids_String_Slice *huid,
                                     Aids_String_Slice decision,
                                     Record_Problems *problems) {
    Aids_String_Slice supersedes_format = aids_string_slice_from_cstr("- Supersedes: ");
    Aids_String_Slice line = {0};

    record_schema_problems(Record_Kind_DECISION, decision, NULL, problems);

    // dangling-decision-task: the record must point at a task that exists.
    Aids_String_Slice task_ref = {0};
    if (artifact_field(decision, "- TASK: ", &task_ref) && task_ref.len > 0) {
        Aids_String_Slice task_huid = {0};
        char path_buffer[PATH_MAX];
        if (!check_ref_huid(task_ref, &task_huid)) {
            record_problems_add(problems, "dangling-decision-task",
                                "DECISION.md TASK '" SS_Fmt "' is not a task ID",
                                SS_Arg(task_ref));
        } else if (snprintf(path_buffer, sizeof(path_buffer), SS_Fmt "/" SS_Fmt "/%s",
                            SS_Arg(*tasks_dir), SS_Arg(task_huid), TASK_FILE_NAME_CSTR) >= 0 &&
                   access(path_buffer, F_OK) != 0) {
            record_problems_add(problems, "dangling-decision-task",
                                "DECISION.md TASK '" SS_Fmt "' has no TASK.md",
                                SS_Arg(task_huid));
        }
    }

    // bad-decision-status: validate the first "- STATUS: " value. A missing
    // STATUS line is itself a problem (a decision record needs a lifecycle).
    Aids_String_Slice value = {0};
    Aids_String_Slice ref = {0};
    switch (artifact_decision_status(decision, &value, &ref)) {
    case Decision_Status_ACCEPTED:
        break;
    case Decision_Status_SUPERSEDED: {
        if (!check_supersede_ref_resolves(tasks_dir, ref)) {
            record_problems_add(problems, "dangling-supersede",
                                "STATUS supersedes '" SS_Fmt "' which has no DECISION.md",
                                SS_Arg(ref));
            break;
        }
        Aids_String_Slice replacement = {0};
        if (check_ref_huid(ref, &replacement) &&
            !check_supersede_is_reciprocal(tasks_dir, replacement, *huid)) {
            record_problems_add(problems, "nonreciprocal-supersede",
                                "STATUS supersedes '" SS_Fmt "' but its DECISION.md has no '- Supersedes: " SS_Fmt "' line",
                                SS_Arg(replacement), SS_Arg(*huid));
        }
        break;
    }
    case Decision_Status_INVALID:
        record_problems_add(problems, "bad-decision-status",
                            "invalid STATUS '" SS_Fmt "' in DECISION.md (use ACCEPTED or 'SUPERSEDED by <ref>')",
                            SS_Arg(value));
        break;
    case Decision_Status_MISSING:
        record_problems_add(problems, "bad-decision-status",
                            "DECISION.md has no STATUS line");
        break;
    }

    // dangling-supersede / nonreciprocal-supersede: every "- Supersedes: <ref>"
    // header must resolve, and the record it names must say it was superseded
    // by this one.
    Aids_String_Slice scan = decision;
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
            record_problems_add(problems, "dangling-supersede",
                                "Supersedes '" SS_Fmt "' which has no DECISION.md",
                                SS_Arg(header_ref));
            continue;
        }
        Aids_String_Slice superseded = {0};
        if (!check_ref_huid(header_ref, &superseded)) {
            continue;
        }
        Aids_String_Slice old_content = {0};
        Aids_String_Slice old_value = {0};
        Aids_String_Slice old_ref = {0};
        Aids_String_Slice back = {0};
        boolean reciprocal = false;
        if (task_sibling_read(tasks_dir, &superseded, "DECISION.md", &old_content)) {
            if (artifact_decision_status(old_content, &old_value, &old_ref) == Decision_Status_SUPERSEDED &&
                check_ref_huid(old_ref, &back) &&
                aids_string_slice_compare(&back, huid) == 0) {
                reciprocal = true;
            }
            AIDS_FREE(old_content.str);
        }
        if (!reciprocal) {
            record_problems_add(problems, "nonreciprocal-supersede",
                                "Supersedes '" SS_Fmt "' but its DECISION.md STATUS does not say 'SUPERSEDED by " SS_Fmt "'",
                                SS_Arg(superseded), SS_Arg(*huid));
        }
    }
}

// The record problems of a TASK.md itself: the plan gate's sections (owed only
// from PLANNED on, since they ARE its output) and the DoD proof contracts.
//
// <step> is the step the record is being judged AT, which is not always the one
// it currently carries: `check` passes the task's own step, while `flow` passes
// the step it is about to move to. That is what lets the plan gate refuse to
// mint a PLANNED record the lint would immediately flag.
static void task_record_problems(Task_Kind kind, Flow_Step step,
                                 Aids_String_Slice task_raw,
                                 Record_Problems *problems) {
    if (step >= Flow_Step_PLANNED) {
        record_schema_problems(Record_Kind_TASK, task_raw,
                               task_required_sections(kind), problems);
    }
    dod_proof_problems(task_raw, problems);
}

static int string_slice_compare_fn(const void *a, const void *b) {
    return aids_string_slice_compare((const Aids_String_Slice *)a,
                                     (const Aids_String_Slice *)b);
}

// ---------------------------------------------------------------------------
// The task graph. PARENT and DEPENDS ON are typed references, but until now
// nothing asked whether they resolve: `tatr flow` would treat a dependency that
// does not exist as one more thing to wait for. The graph is a property of the
// whole record set, so it is loaded once and answered from, rather than
// re-walked per question.
//
// A graph is a flat array of nodes - every task is one directory under one
// tasks dir - so a reference resolves by linear scan over at most a few hundred
// entries. That is fast enough to be uninteresting and keeps the loader honest:
// there is no index to fall out of date.
// ---------------------------------------------------------------------------

typedef struct {
    Aids_String_Slice huid;    // borrowed from the node's own owned copy
    Aids_String_Slice title;
    Task_Kind kind;
    Task_Status status;
    Flow_Step flow_step;
    unsigned int priority;
    Aids_String_Slice parent;  // len 0 when unset
    Aids_Array depends_on;     /* Aids_String_Slice, borrowed from _buffer */
    boolean parsed;            // false when the record did not deserialize
    unsigned char *_buffer;    // owns huid, title, parent and the dependency slices
    Aids_String_Slice _huid_owned;
} Graph_Node;

typedef struct {
    Aids_Array nodes;          /* Graph_Node */
    boolean initialized;
} Task_Graph;

static void task_graph_init(Task_Graph *graph) {
    aids_array_init(&graph->nodes, sizeof(Graph_Node));
    graph->initialized = true;
}

static void task_graph_free(Task_Graph *graph) {
    if (!graph->initialized) {
        return;
    }
    for (size_t i = 0; i < graph->nodes.count; ++i) {
        Graph_Node *node = NULL;
        if (aids_array_get(&graph->nodes, i, (void **)&node) != AIDS_OK) {
            continue;
        }
        aids_array_free(&node->depends_on);
        if (node->_buffer != NULL) {
            AIDS_FREE(node->_buffer);
        }
        if (node->_huid_owned.str != NULL) {
            AIDS_FREE(node->_huid_owned.str);
        }
    }
    aids_array_free(&graph->nodes);
    graph->initialized = false;
}

// Loads every HUID-named directory under <tasks_dir> into the graph. A record
// that does not parse becomes a node with parsed=false rather than being
// dropped: the graph must still know the ID exists, or every reference to it
// would be reported as dangling on top of the parse failure that is the real
// problem.
static Aids_Result task_graph_load(const Aids_String_Slice *tasks_dir, Task_Graph *graph) {
    Aids_Result result = AIDS_OK;
    Aids_Array entries = {0};

    aids_array_init(&entries, sizeof(Aids_String_Slice));
    if (aids_io_listdir(tasks_dir, &entries) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to list tasks directory: %s", aids_failure_reason());
        cleanup_string_slice_array(&entries);
        return_defer(AIDS_ERR);
    }
    aids_array_sort(&entries, string_slice_compare_fn);

    for (size_t i = 0; i < entries.count; ++i) {
        Aids_String_Slice *entry = NULL;
        if (aids_array_get(&entries, i, (void **)&entry) != AIDS_OK || !ishuid(entry)) {
            continue;
        }

        Graph_Node node = {0};
        aids_array_init(&node.depends_on, sizeof(Aids_String_Slice));

        Aids_String_Builder huid_sb = {0};
        aids_string_builder_init(&huid_sb);
        if (aids_string_builder_append(&huid_sb, SS_Fmt, SS_Arg(*entry)) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to copy HUID: %s", aids_failure_reason());
            aids_string_builder_free(&huid_sb);
            aids_array_free(&node.depends_on);
            cleanup_string_slice_array(&entries);
            return_defer(AIDS_ERR);
        }
        aids_string_builder_to_slice(&huid_sb, &node._huid_owned);
        node.huid = node._huid_owned;

        Aids_String_Slice raw = {0};
        if (task_sibling_read(tasks_dir, entry, TASK_FILE_NAME_CSTR, &raw)) {
            Task task = {0};
            task_init_empty(&task);
            if (task_deserialize(raw, &task) == AIDS_OK) {
                node.parsed = true;
                node.title = task.title;
                node.kind = task.meta.kind;
                node.status = task.meta.status;
                node.flow_step = task.meta.flow_step;
                node.priority = task.meta.priority;
                node.parent = task.meta.parent;
                for (size_t d = 0; d < task.meta.depends_on.count; ++d) {
                    Aids_String_Slice *dep = NULL;
                    if (aids_array_get(&task.meta.depends_on, d, (void **)&dep) == AIDS_OK) {
                        aids_array_append(&node.depends_on, dep);
                    }
                }
                node._buffer = raw.str; // the node now owns the bytes its slices point into
                task._buffer = NULL;    // ... so the task must not free them
            } else {
                AIDS_FREE(raw.str);
            }
            task_cleanup(&task);
        }

        if (aids_array_append(&graph->nodes, &node) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to append graph node: %s", aids_failure_reason());
            aids_array_free(&node.depends_on);
            if (node._buffer != NULL) {
                AIDS_FREE(node._buffer);
            }
            AIDS_FREE(node._huid_owned.str);
            cleanup_string_slice_array(&entries);
            return_defer(AIDS_ERR);
        }
    }

    cleanup_string_slice_array(&entries);

defer:
    return result;
}

static Graph_Node *task_graph_find(const Task_Graph *graph, Aids_String_Slice huid) {
    for (size_t i = 0; i < graph->nodes.count; ++i) {
        Graph_Node *node = NULL;
        if (aids_array_get((Aids_Array *)&graph->nodes, i, (void **)&node) != AIDS_OK) {
            continue;
        }
        if (aids_string_slice_compare(&node->huid, &huid) == 0) {
            return node;
        }
    }
    return NULL;
}

// Walks a chain from <start> and reports whether it returns to <start>. <next>
// picks the successor of a node, so one walk serves both the PARENT chain (one
// successor) and each DEPENDS ON edge (walked once per edge). The step limit is
// the node count: a chain longer than that has necessarily revisited something.
typedef boolean (*Graph_Next_Fn)(const Graph_Node *node, Aids_String_Slice *out);

static boolean graph_parent_next(const Graph_Node *node, Aids_String_Slice *out) {
    if (node->parent.len == 0) {
        return false;
    }
    *out = node->parent;
    return true;
}

static boolean graph_chain_returns_to(const Task_Graph *graph,
                                      Aids_String_Slice start,
                                      Aids_String_Slice from,
                                      Graph_Next_Fn next) {
    Aids_String_Slice at = from;
    for (size_t steps = 0; steps <= graph->nodes.count; ++steps) {
        if (aids_string_slice_compare(&at, &start) == 0) {
            return true;
        }
        Graph_Node *node = task_graph_find(graph, at);
        if (node == NULL || !node->parsed) {
            return false;
        }
        Aids_String_Slice up = {0};
        if (!next(node, &up)) {
            return false;
        }
        at = up;
    }
    return false;
}

// True when <start> can reach itself by following DEPENDS ON edges.
//
// <seen> marks the nodes already expanded, so every node is expanded at most
// once and the walk is O(V+E). Without it a legal diamond-shaped graph - two
// tasks depending on a third, repeated - is explored exponentially: measured at
// 0.016s for 48 tasks, 3.2s for 72 and 12.4s for 78, on a walk that runs for
// every dependency edge of every task on every `tatr check` and every gated
// transition. A depth bound stops infinite recursion but not that blow-up.
static boolean graph_depends_reaches(const Task_Graph *graph,
                                     Aids_String_Slice start,
                                     Aids_String_Slice from,
                                     boolean *seen) {
    if (aids_string_slice_compare(&from, &start) == 0) {
        return true;
    }
    for (size_t i = 0; i < graph->nodes.count; ++i) {
        Graph_Node *candidate = NULL;
        if (aids_array_get((Aids_Array *)&graph->nodes, i, (void **)&candidate) != AIDS_OK) {
            continue;
        }
        if (aids_string_slice_compare(&candidate->huid, &from) != 0) {
            continue;
        }
        if (seen[i]) {
            return false; // this subgraph has already been explored
        }
        seen[i] = true;
        if (!candidate->parsed) {
            return false;
        }
        for (size_t d = 0; d < candidate->depends_on.count; ++d) {
            Aids_String_Slice *dep = NULL;
            if (aids_array_get(&candidate->depends_on, d, (void **)&dep) != AIDS_OK) {
                continue;
            }
            if (graph_depends_reaches(graph, start, *dep, seen)) {
                return true;
            }
        }
        return false;
    }
    return false; // the reference does not resolve; missing-dependency says so
}

// missing-parent / self-parent / parent-cycle / bad-epic-relationship /
// missing-dependency / duplicate-dependency / self-dependency /
// dependency-cycle: everything one task's place in the graph is held to.
// Collected as data like every other record rule, so `check` prints them and
// `flow` refuses transitions on them through the same call.
static void graph_node_problems(const Task_Graph *graph,
                                const Graph_Node *node,
                                Record_Problems *problems) {
    if (!node->parsed) {
        return; // malformed-header is the finding; its fields are not readable
    }

    if (node->parent.len > 0) {
        if (aids_string_slice_compare(&node->parent, &node->huid) == 0) {
            record_problems_add(problems, "self-parent", "PARENT names the task itself");
        } else {
            Graph_Node *parent = task_graph_find(graph, node->parent);
            if (parent == NULL) {
                record_problems_add(problems, "missing-parent",
                                    "PARENT '" SS_Fmt "' does not exist", SS_Arg(node->parent));
            } else {
                if (parent->parsed && parent->kind != Task_Kind_EPIC) {
                    record_problems_add(problems, "bad-epic-relationship",
                                        "PARENT '" SS_Fmt "' is KIND: " SS_Fmt ", not EPIC",
                                        SS_Arg(node->parent),
                                        SS_Arg(Task_Kind_Strings[parent->kind]));
                }
                if (graph_chain_returns_to(graph, node->huid, node->parent, graph_parent_next)) {
                    record_problems_add(problems, "parent-cycle",
                                        "the PARENT chain from '" SS_Fmt "' returns to this task",
                                        SS_Arg(node->parent));
                }
            }
        }
    } else if (node->kind == Task_Kind_STORY) {
        record_problems_add(problems, "bad-epic-relationship",
                            "KIND: STORY has no PARENT (a Story belongs to an Epic)");
    }

    for (size_t i = 0; i < node->depends_on.count; ++i) {
        Aids_String_Slice *dep = NULL;
        if (aids_array_get((Aids_Array *)&node->depends_on, i, (void **)&dep) != AIDS_OK) {
            continue;
        }
        // Duplicates are reported once, on the second occurrence.
        boolean duplicate = false;
        for (size_t j = 0; j < i; ++j) {
            Aids_String_Slice *earlier = NULL;
            if (aids_array_get((Aids_Array *)&node->depends_on, j, (void **)&earlier) == AIDS_OK &&
                aids_string_slice_compare(earlier, dep) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            record_problems_add(problems, "duplicate-dependency",
                                "DEPENDS ON lists '" SS_Fmt "' more than once", SS_Arg(*dep));
            continue;
        }
        if (aids_string_slice_compare(dep, &node->huid) == 0) {
            record_problems_add(problems, "self-dependency",
                                "DEPENDS ON lists the task itself");
            continue;
        }
        if (task_graph_find(graph, *dep) == NULL) {
            record_problems_add(problems, "missing-dependency",
                                "DEPENDS ON '" SS_Fmt "' does not exist", SS_Arg(*dep));
            continue;
        }
        // One visited set per edge walked: a node already expanded for THIS
        // edge cannot yield a new answer, but it must still be expandable when
        // the next edge is walked.
        boolean *seen = calloc(graph->nodes.count > 0 ? graph->nodes.count : 1, sizeof(boolean));
        if (seen == NULL) {
            aids_log(AIDS_ERROR, "Failed to allocate the dependency walk's visited set");
            continue;
        }
        boolean cycles = graph_depends_reaches(graph, node->huid, *dep, seen);
        free(seen);
        if (cycles) {
            record_problems_add(problems, "dependency-cycle",
                                "the DEPENDS ON chain from '" SS_Fmt "' returns to this task",
                                SS_Arg(*dep));
        }
    }
}

// ---------------------------------------------------------------------------
// Claims: the coordination primitive for parallel sessions. A claim is a file
// created with O_CREAT|O_EXCL under "<tasks_dir>/.claims/<id>", which the
// kernel makes atomic: of any number of racing sessions exactly one open()
// succeeds and the rest get EEXIST. The winner writes its owner payload in the
// same call that won the race, so a claim is never anonymous.
//
// Two environment variables decide WHERE claims live and WHO holds one, and
// both exist because a claim has to outlive the process that took it and reach
// across working trees:
//
//   TATR_CLAIMS_DIR   the claims directory, default "<tasks_dir>/.claims".
//                     A worktree session points this at the shared checkout's
//                     claims dir so that `claim` and the `flow` guard read the
//                     SAME directory even though they resolve different tasks
//                     trees. Without it the guard could never fire in the
//                     topology the feature exists for.
//   TATR_SESSION      the session identity, default the process's working
//                     directory. tatr is a one-shot CLI: the process that ran
//                     `tatr claim` is gone by the time anything else runs, so
//                     ownership CANNOT be a pid. It is whatever names the
//                     session across invocations - the worktree path by
//                     default, or an explicit id when the session moves around.
//
// There is no timeout and no automatic steal - "the owner is slow" and "the
// owner is dead" are indistinguishable to tatr, so recovering a stale claim is
// a deliberate `tatr release <id> --force`.
//
// The default directory is dotted so that every HUID scan skips it for free and
// one .gitignore line covers the lot; a claim is machine state, and the task's
// own folder holds only versioned history.
// ---------------------------------------------------------------------------

#define CLAIMS_DIR_NAME_CSTR ".claims"

// The claims directory: TATR_CLAIMS_DIR verbatim when set, else
// "<tasks_dir>/.claims".
static Aids_Result claims_dir_path_build(const Aids_String_Slice *tasks_dir,
                                         Aids_String_Slice *out) {
    const char *override = getenv("TATR_CLAIMS_DIR");
    Aids_String_Builder sb = {0};
    Aids_Result appended;

    aids_string_builder_init(&sb);
    if (override != NULL && override[0] != '\0') {
        appended = aids_string_builder_append(&sb, "%s", override);
    } else {
        appended = aids_string_builder_append(&sb, SS_Fmt "/%s", SS_Arg(*tasks_dir),
                                              CLAIMS_DIR_NAME_CSTR);
    }
    if (appended != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to build claims directory path: %s", aids_failure_reason());
        aids_string_builder_free(&sb);
        return AIDS_ERR;
    }
    aids_string_builder_to_slice(&sb, out);
    return AIDS_OK;
}

// Fills <buffer> with "<claims_dir>/<huid>". Returns false when the path would
// be truncated, which must never silently name a different file.
static boolean claim_path_build(const Aids_String_Slice *tasks_dir,
                                Aids_String_Slice huid,
                                char *buffer, size_t size) {
    Aids_String_Slice claims_dir = {0};
    if (claims_dir_path_build(tasks_dir, &claims_dir) != AIDS_OK) {
        return false;
    }
    int written = snprintf(buffer, size, SS_Fmt "/" SS_Fmt,
                           SS_Arg(claims_dir), SS_Arg(huid));
    AIDS_FREE(claims_dir.str);
    return written > 0 && (size_t)written < size;
}

// The identity of the session taking a claim. Not the pid: tatr is a one-shot
// CLI, so a pid recorded by `tatr claim` names a process that has already
// exited and can never match again.
//
// TATR_SESSION when set, else the tasks directory found by walking up from the
// process's REAL working directory. Not the cwd itself: running tatr from a
// subdirectory of a checkout is ordinary, and it must not change who you are.
// Not `-r` either: pointing at a shared tree is how sessions cooperate, so it
// must not make them the same session. The tasks tree a session works in is
// stable across its invocations and different between parallel worktrees,
// which is exactly the identity wanted.
//
// The value is written into a line-oriented record and compared after trimming,
// so a value carrying a newline could forge or orphan a claim: it is rejected
// rather than sanitised, because a session id that is not what the caller
// asked for is its own kind of wrong.
static Aids_Result claim_session_id(Aids_String_Slice *out) {
    const char *override = getenv("TATR_SESSION");
    if (override != NULL && override[0] != '\0') {
        for (const char *c = override; *c != '\0'; ++c) {
            if (iscntrl((unsigned char)*c)) {
                aids_log(AIDS_ERROR, "TATR_SESSION contains a control character; "
                         "a session id is one line of text");
                return AIDS_ERR;
            }
        }
        // Trimmed BEFORE the copy, exactly as artifact_field trims on read, or
        // a value with a trailing space would never match itself. Trimming the
        // slice afterwards would move its str pointer into the middle of the
        // allocation, and freeing that is undefined.
        const char *start = override;
        const char *end = override + strlen(override);
        while (start < end && isspace((unsigned char)*start)) {
            start++;
        }
        while (end > start && isspace((unsigned char)*(end - 1))) {
            end--;
        }
        if (start == end) {
            aids_log(AIDS_ERROR, "TATR_SESSION is only whitespace; unset it to use the default");
            return AIDS_ERR;
        }

        Aids_String_Builder sb = {0};
        aids_string_builder_init(&sb);
        if (aids_string_builder_append(&sb, "%.*s", (int)(end - start), start) != AIDS_OK) {
            aids_string_builder_free(&sb);
            return AIDS_ERR;
        }
        aids_string_builder_to_slice(&sb, out);
        return AIDS_OK;
    }

    Aids_String_Slice cwd = {0};
    if (aids_io_getcwd(&cwd) != AIDS_OK) {
        return AIDS_ERR;
    }
    Aids_String_Slice tasks_dir = {0};
    if (tasks_dir_path_build(&cwd, &tasks_dir) == AIDS_OK) {
        AIDS_FREE(cwd.str);
        *out = tasks_dir;
        return AIDS_OK;
    }
    *out = cwd; // no tasks tree above us; the directory itself is the identity
    return AIDS_OK;
}

typedef struct {
    Aids_String_Slice session; // the identity ownership is decided on
    Aids_String_Slice owner;
    Aids_String_Slice host;
    Aids_String_Slice pid;
    Aids_String_Slice since;
    Aids_String_Slice _buffer; // owns the five slices above
} Claim;

static void claim_free(Claim *claim) {
    if (claim->_buffer.str != NULL) {
        AIDS_FREE(claim->_buffer.str);
        claim->_buffer = (Aids_String_Slice){0};
    }
}

// Reads the claim on <huid>, if there is one. Returns false when the task is
// unclaimed. A claim file that exists but cannot be read or parsed still counts
// as a claim: the safe reading of "something is there but I cannot tell who" is
// that the task is taken.
static boolean claim_read(const Aids_String_Slice *tasks_dir,
                          Aids_String_Slice huid,
                          Claim *claim) {
    char path_buffer[PATH_MAX];
    if (!claim_path_build(tasks_dir, huid, path_buffer, sizeof(path_buffer))) {
        return false;
    }
    if (access(path_buffer, F_OK) != 0) {
        return false;
    }
    Aids_String_Slice path = aids_string_slice_from_cstr(path_buffer);
    Aids_String_Slice content = {0};
    if (aids_io_read(&path, &content, "r") != AIDS_OK) {
        return true; // claimed by someone we cannot name
    }
    claim->_buffer = content;
    artifact_field(content, "- SESSION: ", &claim->session);
    artifact_field(content, "- OWNER: ", &claim->owner);
    artifact_field(content, "- HOST: ", &claim->host);
    artifact_field(content, "- PID: ", &claim->pid);
    artifact_field(content, "- SINCE: ", &claim->since);
    return true;
}

static boolean task_is_claimed(const Aids_String_Slice *tasks_dir, Aids_String_Slice huid) {
    char path_buffer[PATH_MAX];
    if (!claim_path_build(tasks_dir, huid, path_buffer, sizeof(path_buffer))) {
        return false;
    }
    return access(path_buffer, F_OK) == 0;
}

// The machine this claim was taken on, for the diagnostic a contended claim
// prints. Best-effort by design: a claim whose host is "unknown" is still a
// claim, and no caller should fail because a hostname was unavailable.
static boolean tatr_hostname(char *buffer, size_t size) {
#if defined(_WIN32)
    const char *name = getenv("COMPUTERNAME");
    if (name == NULL || name[0] == '\0') {
        return false;
    }
    int written = snprintf(buffer, size, "%s", name);
    return written > 0 && (size_t)written < size;
#else
    return gethostname(buffer, size - 1) == 0;
#endif
}

// Renders the claim payload of the calling process.
static Aids_Result claim_payload_build(Aids_String_Slice *out) {
    Aids_String_Builder sb = {0};
    Aids_Result result = AIDS_OK;
    char host[256] = {0};
    char stamp[HUID_LENGTH] = {0};
    const char *user = getenv("USER");

    // gethostname lives in <winsock2.h> under MinGW and needs a linked socket
    // library there, which is far too much machinery for one diagnostic field.
    // Windows publishes the same answer as an environment variable.
    if (!tatr_hostname(host, sizeof(host))) {
        snprintf(host, sizeof(host), "unknown");
    }
    if (huid(stamp) != AIDS_OK) {
        return AIDS_ERR;
    }

    Aids_String_Slice session = {0};
    if (claim_session_id(&session) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to determine the session identity: %s", aids_failure_reason());
        return AIDS_ERR;
    }

    aids_string_builder_init(&sb);
    // SESSION first: it is the field ownership is decided on. OWNER, HOST, PID
    // and SINCE are for the human reading a contended claim, not for the
    // comparison - a pid cannot be compared across one-shot invocations.
    if (aids_string_builder_append(&sb,
            "# Claim\n\n- SESSION: " SS_Fmt "\n- OWNER: %s\n- HOST: %s\n- PID: %ld\n- SINCE: %s\n",
            SS_Arg(session),
            user != NULL && user[0] != '\0' ? user : "unknown",
            host, (long)getpid(), stamp) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to build claim payload: %s", aids_failure_reason());
        aids_string_builder_free(&sb);
        return_defer(AIDS_ERR);
    }
    aids_string_builder_to_slice(&sb, out);

defer:
    if (session.str != NULL) {
        AIDS_FREE(session.str);
    }
    return result;
}

// True when <claim> belongs to a session other than this one. A claim with no
// SESSION field at all counts as someone else's: an unattributable claim is
// exactly the case `--force` exists for, and guessing it is ours would let a
// corrupt file silently unblock the guard.
static boolean claim_held_by_other_session(const Claim *claim) {
    Aids_String_Slice mine = {0};
    if (claim->session.len == 0) {
        return true;
    }
    if (claim_session_id(&mine) != AIDS_OK) {
        return true;
    }
    boolean other = aids_string_slice_compare(&claim->session, &mine) != 0;
    AIDS_FREE(mine.str);
    return other;
}

typedef enum {
    Claim_Result_TAKEN,      // this process now owns the claim
    Claim_Result_CONTENDED,  // someone else got there first
    Claim_Result_ERROR
} Claim_Result;

// Takes the claim on <huid> with one atomic O_CREAT|O_EXCL open. Everything
// that could fail before the race - building the payload, creating the claims
// directory - happens first, so the only thing between the open and the write
// is the write itself.
static Claim_Result claim_take(const Aids_String_Slice *tasks_dir, Aids_String_Slice huid) {
    Aids_String_Slice payload = {0};
    Aids_String_Slice claims_dir = {0};
    Claim_Result result = Claim_Result_ERROR;
    int fd = -1;

    if (claim_payload_build(&payload) != AIDS_OK) {
        return Claim_Result_ERROR;
    }
    if (claims_dir_path_build(tasks_dir, &claims_dir) != AIDS_OK) {
        goto cleanup;
    }
    if (!aids_io_isdir(&claims_dir) && aids_io_mkdir(&claims_dir, true) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to create claims directory '" SS_Fmt "': %s",
                 SS_Arg(claims_dir), aids_failure_reason());
        goto cleanup;
    }

    char path_buffer[PATH_MAX];
    if (!claim_path_build(tasks_dir, huid, path_buffer, sizeof(path_buffer))) {
        aids_log(AIDS_ERROR, "Failed to build claim path: path too long");
        goto cleanup;
    }

    // The atomic step. O_EXCL makes this the whole race: exactly one caller
    // gets a descriptor, every other gets EEXIST.
    fd = open(path_buffer, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd < 0) {
        result = (errno == EEXIST) ? Claim_Result_CONTENDED : Claim_Result_ERROR;
        if (result == Claim_Result_ERROR) {
            aids_log(AIDS_ERROR, "Failed to create claim '%s': %s", path_buffer, strerror(errno));
        }
        goto cleanup;
    }

    if (write(fd, payload.str, payload.len) != (ssize_t)payload.len) {
        aids_log(AIDS_ERROR, "Failed to write claim '%s': %s", path_buffer, strerror(errno));
        close(fd);
        fd = -1;
        unlink(path_buffer); // an unattributable claim is worse than none
        goto cleanup;
    }
    result = Claim_Result_TAKEN;

cleanup:
    if (fd >= 0) {
        close(fd);
    }
    if (payload.str != NULL) {
        AIDS_FREE(payload.str);
    }
    if (claims_dir.str != NULL) {
        AIDS_FREE(claims_dir.str);
    }
    return result;
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

// Appends one message per record problem the shared collectors found, so a
// refusal names the same thing `tatr check` would have named. The rule slug is
// carried through: an agent that reads "bad-record-schema: RETRO.md has no
// '## Action items' section" from a refused transition can find the same rule
// in the lint documentation.
static void flow_unmet_add_problems(Flow_Unmet *unmet, const Record_Problems *problems) {
    for (size_t i = 0; i < problems->count; ++i) {
        flow_unmet_add(unmet, "%s: %s", problems->items[i].rule, problems->items[i].message);
    }
}

// Evaluates every precondition the edge <from> -> <to> carries and appends one
// message per unmet one. Four gates exist, and an edge either carries a gate
// whole or not at all:
//
//   PLANNING   -> PLANNED      the record gate: the sections the plan produces
//   PLANNED    -> WORKING      the plan gate: an approved plan, CLOSED deps
//   REVIEWING  -> COMPOUNDING  the review gate: an approved, well-formed
//                              REVIEW.md
//   COMPOUNDING-> DONE         all of the above, plus the close gate:
//                              no unchecked Steps, a schema-clean RETRO.md,
//                              and a valid DECISION.md when the task has one
//
// REVIEWING -> WORKING and the two remaining pre-plan edges carry no
// preconditions: going back to fix something must never be harder than going
// forward.
//
// The record gate is what keeps AGENTS.md's tying invariant true: `flow` and
// `check` ask the SAME collector functions the same questions, so no
// transition can mint a record the lint would immediately flag. The plan-gate
// sections are judged at the step being moved TO, because PLANNING -> PLANNED
// is the transition that makes the record owe them.
//
// KIND: EPIC is exempt from exactly the four requirements it is already exempt
// from in `tatr check` - plan approval, review presence, retro presence and
// unchecked Steps - because an Epic container's record lives in its children.
// Its own sections, its dependencies and its DECISION.md are checked like
// anyone's.
static void flow_check_preconditions(const Aids_String_Slice *tasks_dir,
                                     const Task_Graph *graph,
                                     const Aids_String_Slice *huid,
                                     const Task *task,
                                     Aids_String_Slice task_raw,
                                     Flow_Step from,
                                     Flow_Step to,
                                     Flow_Unmet *unmet) {
    boolean record_gate = false;
    boolean plan_gate = false;
    boolean review_gate = false;
    boolean close_gate = false;
    boolean exempt = check_task_is_container(task);

    if (from == Flow_Step_PLANNING && to == Flow_Step_PLANNED) {
        record_gate = true;
    } else if (from == Flow_Step_PLANNED && to == Flow_Step_WORKING) {
        record_gate = true;
        plan_gate = true;
    } else if (from == Flow_Step_REVIEWING && to == Flow_Step_COMPOUNDING) {
        review_gate = true;
    } else if (from == Flow_Step_COMPOUNDING && to == Flow_Step_DONE) {
        record_gate = true;
        plan_gate = true;
        review_gate = true;
        close_gate = true;
    }

    if (record_gate) {
        Record_Problems problems = {0};
        task_record_problems(task->meta.kind, to, task_raw, &problems);
        // The task's place in the graph, read from the same loader `check`
        // reads: a dependency that does not exist must be a refusal, not one
        // more thing to wait for.
        const Graph_Node *node = graph != NULL ? task_graph_find(graph, *huid) : NULL;
        if (node != NULL) {
            graph_node_problems(graph, node, &problems);
        }
        flow_unmet_add_problems(unmet, &problems);

        // Any SPIKE.md the task carries is held to its schema; only its
        // ABSENCE is a question for a SPIKE-kind task, matching `check`.
        Aids_String_Slice spike = {0};
        if (!task_sibling_read(tasks_dir, huid, "SPIKE.md", &spike)) {
            if (task->meta.kind == Task_Kind_SPIKE) {
                flow_unmet_add(unmet, "missing-spike-record: KIND: SPIKE task has no SPIKE.md");
            }
        } else {
            Record_Problems spike_problems = {0};
            spike_record_problems(tasks_dir, spike, &spike_problems);
            flow_unmet_add_problems(unmet, &spike_problems);
            AIDS_FREE(spike.str);
        }
    }

    if (plan_gate) {
        // A claim is how parallel sessions divide work, so starting a task
        // another session holds is the exact collision claims exist to
        // prevent. A claim this process took is not in its own way.
        if (to == Flow_Step_WORKING) {
            Claim holder = {0};
            if (claim_read(tasks_dir, *huid, &holder) && claim_held_by_other_session(&holder)) {
                flow_unmet_add(unmet, "the task is claimed by session '" SS_Fmt "' ("
                               SS_Fmt "@" SS_Fmt ", since " SS_Fmt "); set TATR_SESSION to that "
                               "id if it is yours, or release it with `tatr release " SS_Fmt
                               " --force` if that session is gone",
                               SS_Arg(holder.session), SS_Arg(holder.owner),
                               SS_Arg(holder.host), SS_Arg(holder.since), SS_Arg(*huid));
            }
            claim_free(&holder);
        }
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
            // The record's SHAPE is never exempt: a container that carries a
            // REVIEW.md is held to the same schema as anyone's, or the
            // transition would mint a state the lint flags. The open
            // BLOCKER/MAJOR count lives in review_round_problems as
            // approve-with-open-findings and is not repeated here: the verdict
            // check above already refuses anything but APPROVE, so the two
            // conditions coincide and one message is enough.
            Record_Problems problems = {0};
            record_schema_problems(Record_Kind_REVIEW, review, NULL, &problems);
            review_round_problems(review, &problems);
            flow_unmet_add_problems(unmet, &problems);
            AIDS_FREE(review.str);
        }
    }

    if (close_gate) {
        // An Epic's work is its children's: it cannot be done while any of them
        // is not. A child that was dropped is CLOSED with the reason recorded -
        // there is no optional-child marker, because leaving one OPEN to mean
        // "not required" would make this guard unfalsifiable.
        if (task->meta.kind == Task_Kind_EPIC && graph != NULL) {
            for (size_t i = 0; i < graph->nodes.count; ++i) {
                Graph_Node *child = NULL;
                if (aids_array_get((Aids_Array *)&graph->nodes, i, (void **)&child) != AIDS_OK ||
                    !child->parsed) {
                    continue;
                }
                if (child->parent.len == 0 ||
                    aids_string_slice_compare(&child->parent, huid) != 0) {
                    continue;
                }
                if (child->status != Task_Status_CLOSED) {
                    flow_unmet_add(unmet, "child " SS_Fmt " is not CLOSED (STATUS: " SS_Fmt ")",
                                   SS_Arg(child->huid), SS_Arg(Task_Status_Strings[child->status]));
                }
            }
        }
        if (!exempt) {
            size_t unchecked = artifact_count_unchecked_steps(task_raw);
            if (unchecked > 0) {
                flow_unmet_add(unmet, "%zu unchecked Steps item(s) remain", unchecked);
            }
        }
        // Presence is the container-exempt part; a RETRO.md that IS there is
        // held to its schema whatever the kind.
        Aids_String_Slice retro = {0};
        if (!task_sibling_read(tasks_dir, huid, "RETRO.md", &retro)) {
            if (!exempt) {
                flow_unmet_add(unmet, "there is no RETRO.md: the retro has not been written");
            }
        } else {
            Record_Problems problems = {0};
            record_schema_problems(Record_Kind_RETRO, retro, NULL, &problems);
            flow_unmet_add_problems(unmet, &problems);
            AIDS_FREE(retro.str);
        }
        Aids_String_Slice decision = {0};
        if (task_sibling_read(tasks_dir, huid, "DECISION.md", &decision)) {
            // The whole DECISION.md rule set, not a subset of it: a supersede
            // link that dangles or resolves only one way is exactly the kind of
            // state the lint would flag on a record this gate had just closed.
            Record_Problems problems = {0};
            decision_record_problems(tasks_dir, huid, decision, &problems);
            flow_unmet_add_problems(unmet, &problems);
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
    Task_Graph graph = {0};
    boolean graph_loaded = false;

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

    task_graph_init(&graph);
    graph_loaded = true;
    if (task_graph_load(&tasks_dir, &graph) != AIDS_OK) {
        return_defer(1);
    }

    flow_check_preconditions(&tasks_dir, &graph, &id, &task, raw, from, to, &unmet);
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
    if (graph_loaded) {
        task_graph_free(&graph);
    }
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

// ---------------------------------------------------------------------------
// Historical exemptions. Sibling records that predate a schema rule are
// classified rather than rewritten: the flow trail is append-only history, so a
// task record is not edited to satisfy a rule invented after it was written.
// `tasks/EXEMPTIONS.md` classifies each such record explicitly, one
// "- <task-id> <rule>: <reason>" line per suppressed finding. Every finding
// routes through check_finding, so any rule can be exempted the same way, and
// an exemption that never fires is itself a finding - the list cannot rot.
// Adding an entry is visible in the diff, which is the same argument that
// keeps `tatr flow` free of a --force flag.
// ---------------------------------------------------------------------------

#define EXEMPTIONS_FILE_NAME_CSTR "EXEMPTIONS.md"

typedef struct {
    Aids_String_Slice huid;
    Aids_String_Slice rule;
    boolean used;
} Check_Exemption;

typedef struct {
    Aids_Array entries;        /* Check_Exemption */
    Aids_String_Slice content; // owns the bytes the slices above point into
    boolean initialized;
} Check_Exemptions;

static void check_exemptions_init(Check_Exemptions *ex) {
    aids_array_init(&ex->entries, sizeof(Check_Exemption));
    ex->content = (Aids_String_Slice){0};
    ex->initialized = true;
}

static void check_exemptions_free(Check_Exemptions *ex) {
    if (!ex->initialized) {
        return;
    }
    aids_array_free(&ex->entries);
    if (ex->content.str != NULL) {
        AIDS_FREE(ex->content.str);
        ex->content = (Aids_String_Slice){0};
    }
    ex->initialized = false;
}

// Reads "<tasks_dir>/EXEMPTIONS.md" if it exists. A missing file is not an
// error: a repository with no history to classify carries no such file.
static void check_exemptions_load(Check_Exemptions *ex,
                                  const Aids_String_Slice *tasks_dir) {
    if (ex->content.str != NULL) {
        return; // already loaded; the slices below borrow those bytes
    }
    char path_buffer[PATH_MAX];
    if (snprintf(path_buffer, sizeof(path_buffer), SS_Fmt "/%s",
                 SS_Arg(*tasks_dir), EXEMPTIONS_FILE_NAME_CSTR) < 0) {
        return;
    }
    if (access(path_buffer, F_OK) != 0) {
        return;
    }
    Aids_String_Slice path = aids_string_slice_from_cstr(path_buffer);
    Aids_String_Slice content = {0};
    if (aids_io_read(&path, &content, "r") != AIDS_OK) {
        aids_log(AIDS_WARNING, "'%s' exists but could not be read: %s",
                 path_buffer, aids_failure_reason());
        return;
    }
    ex->content = content;

    Aids_String_Slice scan = content;
    Aids_String_Slice line = {0};
    Aids_String_Slice bullet = aids_string_slice_from_cstr("- ");
    while (aids_string_slice_tokenize(&scan, '\n', &line)) {
        Aids_String_Slice l = line;
        aids_string_slice_trim(&l);
        if (!aids_string_slice_starts_with(&l, bullet)) {
            continue;
        }
        aids_string_slice_skip(&l, bullet.len);
        Aids_String_Slice huid = {0};
        if (!aids_string_slice_tokenize(&l, ' ', &huid)) {
            continue;
        }
        aids_string_slice_trim(&huid);
        if (!ishuid(&huid)) {
            continue; // prose in the file's preamble, not an exemption line
        }
        Aids_String_Slice rule = {0};
        if (!aids_string_slice_tokenize(&l, ':', &rule)) {
            continue;
        }
        aids_string_slice_trim(&rule);
        if (rule.len == 0) {
            continue;
        }
        Check_Exemption entry = { .huid = huid, .rule = rule, .used = false };
        if (aids_array_append(&ex->entries, &entry) != AIDS_OK) {
            aids_log(AIDS_WARNING, "Failed to record exemption: %s", aids_failure_reason());
            return;
        }
    }
}

// Prints one finding as "<id>: <rule>: <detail>" and returns 1, unless the
// (task, rule) pair is exempted - then it marks the exemption used and returns
// 0. Every check rule reports through here. The format attribute is what makes
// that funnel safe: a detail string built with SS_Fmt but missing its SS_Arg
// is a compile error rather than a garbage finding.
static size_t check_finding(Check_Exemptions *ex, const Aids_String_Slice *huid,
                            const char *rule, const char *fmt, ...) TATR_PRINTF_FORMAT(4, 5);

static size_t check_finding(Check_Exemptions *ex, const Aids_String_Slice *huid,
                            const char *rule, const char *fmt, ...) {
    if (ex != NULL && ex->initialized) {
        Aids_String_Slice rule_slice = aids_string_slice_from_cstr((char *)rule);
        for (size_t i = 0; i < ex->entries.count; ++i) {
            Check_Exemption *entry = NULL;
            if (aids_array_get(&ex->entries, i, (void **)&entry) != AIDS_OK) {
                continue;
            }
            if (aids_string_slice_compare(&entry->huid, huid) == 0 &&
                aids_string_slice_compare(&entry->rule, &rule_slice) == 0) {
                entry->used = true;
                return 0;
            }
        }
    }
    printf(SS_Fmt ": %s: ", SS_Arg(*huid), rule);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    return 1;
}

// Reports every exemption that never fired. Only meaningful after a full scan:
// with `check --id <ID>` the other tasks' rules were never evaluated.
static size_t check_exemptions_report_unused(Check_Exemptions *ex) {
    size_t findings = 0;
    for (size_t i = 0; i < ex->entries.count; ++i) {
        Check_Exemption *entry = NULL;
        if (aids_array_get(&ex->entries, i, (void **)&entry) != AIDS_OK || entry->used) {
            continue;
        }
        printf(SS_Fmt ": unused-exemption: '" SS_Fmt "' is exempted in %s but did not fire\n",
               SS_Arg(entry->huid), SS_Arg(entry->rule), EXEMPTIONS_FILE_NAME_CSTR);
        findings++;
    }
    return findings;
}

// Prints a collected Record_Problems set as findings, each under its own rule
// so it can be exempted individually. This is the ONLY place `check` turns a
// record problem into output: the problems themselves come from the shared
// collectors above, which `flow` reads too, so neither side owns a copy of a
// rule and a transition cannot produce a state the lint would then flag.
static size_t check_report_problems(Check_Exemptions *ex,
                                    const Aids_String_Slice *huid,
                                    const Record_Problems *problems) {
    size_t findings = 0;
    for (size_t i = 0; i < problems->count; ++i) {
        findings += check_finding(ex, huid, problems->items[i].rule, "%s",
                                  problems->items[i].message);
    }
    return findings;
}

// Collects a record's problems and reports them in one call - the shape every
// per-record check below uses.
static size_t check_record(Check_Exemptions *ex, const Aids_String_Slice *huid,
                           Record_Kind kind, Aids_String_Slice doc,
                           const char *const *sections_override) {
    Record_Problems problems = {0};
    record_schema_problems(kind, doc, sections_override, &problems);
    return check_report_problems(ex, huid, &problems);
}

// Lints a task's DECISION.md (only called when the file exists). One line, so
// the collector below stays the single home of every DECISION.md rule and the
// close gate reads exactly what the lint reads.
static size_t check_decision(Check_Exemptions *ex,
                             const Aids_String_Slice *tasks_dir,
                             const Aids_String_Slice *huid,
                             const Aids_String_Slice *decision) {
    Record_Problems problems = {0};
    decision_record_problems(tasks_dir, huid, *decision, &problems);
    return check_report_problems(ex, huid, &problems);
}

// Lints one task directory. Returns the number of findings printed.
static size_t check_task(Check_Exemptions *ex,
                         const Task_Graph *graph,
                         const Aids_String_Slice *tasks_dir,
                         const Aids_String_Slice *huid) {
    size_t findings = 0;
    Aids_String_Slice raw = {0};
    Aids_String_Slice review = {0};
    boolean has_review = false;
    Task task = {0};
    boolean task_loaded = false;

    if (!task_sibling_read(tasks_dir, huid, TASK_FILE_NAME_CSTR, &raw)) {
        findings += check_finding(ex, huid, "malformed-header", "TASK.md missing or unreadable");
        goto review_checks;
    }

    task_init_empty(&task);
    if (task_deserialize(raw, &task) != AIDS_OK) {
        findings += check_finding(ex, huid, "malformed-header",
                                  "TASK.md failed to parse (title and v2 metadata block)");
        AIDS_FREE(raw.str);
        raw = (Aids_String_Slice){0};
        goto review_checks;
    }
    task._buffer = raw.str; // task now owns the buffer
    task_loaded = true;

    // Metadata values no longer need a re-scan here: the v2 parser validates
    // the exact token it consumes (trailing spaces and CRLF tails included),
    // so an invalid STATUS, KIND, FLOW STEP or PLAN STATUS has already been
    // reported above as a parse failure. What the parser does NOT police is the
    // body, so the kind's required sections are checked from the schema table.
    //
    // Only from PLANNED on, though: `## Steps` and `## Definition of Done` ARE
    // the plan gate's output, so demanding them of a BACKLOG record would make
    // every task `tatr new` creates a finding the moment it exists. The same
    // reasoning covers a SPIKE task's SPIKE.md - the research doc is written
    // during the spike, not at the instant the task is filed.
    {
        Record_Problems problems = {0};
        task_record_problems(task.meta.kind, task.meta.flow_step, raw, &problems);
        findings += check_report_problems(ex, huid, &problems);
    }

    // The task's place in the graph: PARENT and DEPENDS ON must resolve, not
    // repeat, not name the task itself, and not form a cycle.
    if (graph != NULL) {
        const Graph_Node *node = task_graph_find(graph, *huid);
        if (node != NULL) {
            Record_Problems problems = {0};
            graph_node_problems(graph, node, &problems);
            findings += check_report_problems(ex, huid, &problems);
        }
    }

    // A SPIKE task's research lives in its SPIKE.md sibling; without one the
    // task records a question and no answer. Its ABSENCE is what depends on
    // the task's KIND; the contents of any SPIKE.md that does exist are
    // checked on presence alone, whatever the task's kind, because
    // `tatr scaffold <id> SPIKE` will write one for any task.
    {
        Aids_String_Slice spike = {0};
        if (!task_sibling_read(tasks_dir, huid, "SPIKE.md", &spike)) {
            if (task.meta.kind == Task_Kind_SPIKE &&
                task.meta.flow_step >= Flow_Step_PLANNED) {
                findings += check_finding(ex, huid, "missing-spike-record",
                                          "KIND: SPIKE task has no SPIKE.md");
            }
        } else {
            Record_Problems problems = {0};
            spike_record_problems(tasks_dir, spike, &problems);
            findings += check_report_problems(ex, huid, &problems);
            AIDS_FREE(spike.str);
        }
    }

    if (task.meta.status == Task_Status_IN_PROGRESS &&
        task.meta.plan_status != Plan_Status_APPROVED &&
        !check_task_is_container(&task)) {
        findings += check_finding(ex, huid, "unplanned-in-progress",
                                  "IN_PROGRESS task lacks PLAN STATUS: APPROVED");
    }

    if (task.meta.status == Task_Status_CLOSED && !check_task_is_container(&task)) {
        size_t unchecked = artifact_count_unchecked_steps(raw);
        if (unchecked > 0) {
            findings += check_finding(ex, huid, "closed-unchecked",
                                      "%zu unchecked Steps item(s) on a CLOSED task", unchecked);
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
                findings += check_finding(ex, huid, "closed-not-approved",
                                          "REVIEW.md has no VERDICT line");
            } else if (aids_string_slice_compare(&last_verdict, &approve) != 0) {
                findings += check_finding(ex, huid, "closed-not-approved",
                                          "latest REVIEW.md verdict is '" SS_Fmt "'",
                                          SS_Arg(last_verdict));
            }
        }

        Record_Problems problems = {0};
        record_schema_problems(Record_Kind_REVIEW, review, NULL, &problems);
        review_round_problems(review, &problems);
        findings += check_report_problems(ex, huid, &problems);
    }

    {
        Aids_String_Slice retro = {0};
        if (task_sibling_read(tasks_dir, huid, "RETRO.md", &retro)) {
            findings += check_record(ex, huid, Record_Kind_RETRO, retro, NULL);
            AIDS_FREE(retro.str);
        }
    }

    if (task_loaded && task.meta.status == Task_Status_CLOSED &&
        !check_task_is_container(&task)) {
        if (!has_review) {
            findings += check_finding(ex, huid, "closed-missing-review",
                                      "CLOSED task has no REVIEW.md");
        }
        if (!task_sibling_exists(tasks_dir, huid, "RETRO.md")) {
            findings += check_finding(ex, huid, "closed-missing-retro",
                                      "CLOSED task has no RETRO.md");
        }
    }

    // DECISION.md checks are presence-gated and independent of TASK.md
    // validity: they only fire when the sibling exists, so a task without one
    // is never touched.
    {
        Aids_String_Slice decision = {0};
        if (task_sibling_read(tasks_dir, huid, "DECISION.md", &decision)) {
            findings += check_decision(ex, tasks_dir, huid, &decision);
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

// ---------------------------------------------------------------------------
// Lessons ledger dispositions.
//
// A lesson that reaches the promotion threshold moves under the ledger's
// "## Pending promotions" heading, where it waits for the user to say what
// should happen to it. The answer is written as an annotation inside the entry's
// own count parens, extending the vocabulary the ledger header already carries:
//
//   - `slug` (x3, PROMOTE 2026-07-30 -> 20260731-101010): <lesson text>
//   - `slug` (x3, DEFER 2026-07-30 at x3: <reason>): <lesson text>
//   - `slug` (x3, RETIRE 2026-07-30: <reason>): <lesson text>
//   - `slug` (x3, ABSORBED 2026-07-30 by <target>): <lesson text>
//
// This grammar is validated ONLY under that heading. Entries decided in the
// past and moved back to their own section keep the applied markers
// promotion-stalled already exempts (PROMOTED / absorbed by / RETIRED), so no
// ledger history has to be rewritten to adopt the rule.
//
// A DEFER records the count it was taken at. Once the lesson recurs and the
// live count passes that number the deferral no longer covers the entry and the
// decision is asked for again - the reason a DEFER cannot become a permanent
// silence, decided without putting a wall clock into `tatr check`.
// ---------------------------------------------------------------------------

typedef enum {
    Ledger_Disposition_NONE = 0,
    Ledger_Disposition_PROMOTE,
    Ledger_Disposition_DEFER,
    Ledger_Disposition_RETIRE,
    Ledger_Disposition_ABSORBED,
} Ledger_Disposition;

#define LEDGER_DISPOSITION_VALUES_CSTR "PROMOTE|DEFER|RETIRE|ABSORBED"

// An occurrence count is a tally of retros, not a number that needs a wide type.
// Bounding the digit run keeps the accumulator from wrapping and reporting a
// fabricated count in a finding.
#define LEDGER_COUNT_MAX_DIGITS 9

static const char *Ledger_Disposition_Strings[] = {
    "", "PROMOTE", "DEFER", "RETIRE", "ABSORBED",
};

static boolean ledger_disposition_from_slice(Aids_String_Slice s, Ledger_Disposition *out) {
    size_t n = sizeof(Ledger_Disposition_Strings) / sizeof(Ledger_Disposition_Strings[0]);
    for (size_t i = 1; i < n; ++i) {
        unsigned long len = strlen(Ledger_Disposition_Strings[i]);
        if (s.len == len && memcmp(s.str, Ledger_Disposition_Strings[i], len) == 0) {
            *out = (Ledger_Disposition)i;
            return true;
        }
    }
    return false;
}

typedef struct {
    Aids_String_Slice slug;        // first `backticked` token, or "(unnamed)"
    unsigned long count;           // N in "(xN"
    const unsigned char *count_end;   // just past the last digit of the count
    const unsigned char *paren_close; // the ')' closing the count group
    boolean annotated;             // a ", ..." follows the count
    Ledger_Disposition disposition;
    unsigned long defer_count;     // DEFER only: the count it was taken at
    Aids_String_Slice payload;     // task ref, target, or reason
    char problem[192];             // empty when the entry is well formed
} Ledger_Entry;

static boolean ledger_slice_is_date(Aids_String_Slice s) {
    if (s.len != 10) {
        return false;
    }
    for (unsigned long i = 0; i < 10; ++i) {
        boolean want_dash = (i == 4 || i == 7);
        boolean is_dash = (s.str[i] == '-');
        boolean is_digit = (s.str[i] >= '0' && s.str[i] <= '9');
        if (want_dash != is_dash || (!want_dash && !is_digit)) {
            return false;
        }
    }
    return true;
}

// Splits off the leading run of non-space bytes, advancing `rest` past it and
// past the whitespace that followed.
static Aids_String_Slice ledger_take_word(Aids_String_Slice *rest) {
    unsigned long i = 0;
    while (i < rest->len && !isspace((unsigned char)rest->str[i])) {
        i++;
    }
    Aids_String_Slice word = aids_string_slice_from_parts(rest->str, i);
    *rest = aids_string_slice_from_parts(rest->str + i, rest->len - i);
    aids_string_slice_trim_left(rest);
    return word;
}

static boolean ledger_slice_starts_with_cstr(Aids_String_Slice s, const char *prefix) {
    unsigned long len = strlen(prefix);
    return s.len >= len && memcmp(s.str, prefix, len) == 0;
}

// A lesson is named by the first `backticked` token on its line. An entry
// without one is still an entry - it is reported as "(unnamed)" rather than
// skipped, so a missing slug cannot hide a missing decision.
static Aids_String_Slice ledger_line_slug(Aids_String_Slice l) {
    for (unsigned long a = 0; a < l.len; ++a) {
        if (l.str[a] != '`') {
            continue;
        }
        unsigned long b = a + 1;
        while (b < l.len && l.str[b] != '`') {
            b++;
        }
        if (b < l.len) {
            return aids_string_slice_from_parts(l.str + a + 1, b - a - 1);
        }
        break;
    }
    return aids_string_slice_from_cstr("(unnamed)");
}

// Reads the payload of one disposition out of `rest`, the annotation text after
// the verb and its date. Fills entry->problem and returns false when the form
// does not hold; the caller reports that as bad-disposition.
static boolean ledger_parse_payload(Ledger_Entry *entry, Aids_String_Slice rest) {
    const char *verb = Ledger_Disposition_Strings[entry->disposition];

    switch (entry->disposition) {
    case Ledger_Disposition_PROMOTE: {
        if (!ledger_slice_starts_with_cstr(rest, "->")) {
            snprintf(entry->problem, sizeof(entry->problem), "%s needs '-> <task-id>'", verb);
            return false;
        }
        rest = aids_string_slice_from_parts(rest.str + 2, rest.len - 2);
        aids_string_slice_trim(&rest);
        if (rest.len == 0) {
            snprintf(entry->problem, sizeof(entry->problem), "%s needs '-> <task-id>'", verb);
            return false;
        }
        entry->payload = rest;
        return true;
    }
    case Ledger_Disposition_DEFER: {
        if (!ledger_slice_starts_with_cstr(rest, "at x")) {
            snprintf(entry->problem, sizeof(entry->problem), "%s needs 'at x<count>'", verb);
            return false;
        }
        unsigned long i = 4;
        unsigned long count = 0;
        while (i < rest.len && rest.str[i] >= '0' && rest.str[i] <= '9') {
            count = count * 10 + (unsigned long)(rest.str[i] - '0');
            i++;
        }
        if (i == 4) {
            snprintf(entry->problem, sizeof(entry->problem), "%s needs 'at x<count>'", verb);
            return false;
        }
        // A DEFER records the count it was TAKEN at, so it can stop covering the
        // entry once the lesson recurs. A hand-written count above the live one
        // would defer past every future occurrence - a permanent silence, which
        // is the failure this rule exists to prevent.
        if (count > entry->count) {
            snprintf(entry->problem, sizeof(entry->problem),
                     "%s at x%lu is above the entry's own x%lu: a DEFER records the count "
                     "it was taken at", verb, count, entry->count);
            return false;
        }
        entry->defer_count = count;
        rest = aids_string_slice_from_parts(rest.str + i, rest.len - i);
        if (rest.len == 0 || rest.str[0] != ':') {
            snprintf(entry->problem, sizeof(entry->problem), "%s needs ': <reason>'", verb);
            return false;
        }
        rest = aids_string_slice_from_parts(rest.str + 1, rest.len - 1);
        aids_string_slice_trim(&rest);
        if (rest.len == 0) {
            snprintf(entry->problem, sizeof(entry->problem), "%s needs ': <reason>'", verb);
            return false;
        }
        entry->payload = rest;
        return true;
    }
    case Ledger_Disposition_RETIRE: {
        if (rest.len == 0 || rest.str[0] != ':') {
            snprintf(entry->problem, sizeof(entry->problem), "%s needs ': <reason>'", verb);
            return false;
        }
        rest = aids_string_slice_from_parts(rest.str + 1, rest.len - 1);
        aids_string_slice_trim(&rest);
        if (rest.len == 0) {
            snprintf(entry->problem, sizeof(entry->problem), "%s needs ': <reason>'", verb);
            return false;
        }
        entry->payload = rest;
        return true;
    }
    case Ledger_Disposition_ABSORBED: {
        if (!ledger_slice_starts_with_cstr(rest, "by ")) {
            snprintf(entry->problem, sizeof(entry->problem), "%s needs 'by <target>'", verb);
            return false;
        }
        rest = aids_string_slice_from_parts(rest.str + 3, rest.len - 3);
        aids_string_slice_trim(&rest);
        if (rest.len == 0) {
            snprintf(entry->problem, sizeof(entry->problem), "%s needs 'by <target>'", verb);
            return false;
        }
        entry->payload = rest;
        return true;
    }
    case Ledger_Disposition_NONE:
    default:
        break;
    }
    AIDS_UNREACHABLE("ledger_parse_payload");
}

// Parses one ledger line as a lesson entry: its slug, its "(xN" count and, when
// the count group carries a ", ..." annotation, the disposition in it. Returns
// false for every line that is not an entry (headings, prose, the continuation
// line of a wrapped entry), so those are simply passed over.
//
// `line` must point into the caller's file buffer: count_end and paren_close are
// pointers into it, which is what lets the recording path splice an annotation
// in without re-rendering any other byte of the ledger.
static boolean ledger_entry_parse(Aids_String_Slice line, Ledger_Entry *out) {
    Ledger_Entry entry = {0};
    Aids_String_Slice l = line;
    aids_string_slice_trim(&l);

    if (!ledger_slice_starts_with_cstr(l, "- ")) {
        return false;
    }

    // A line that opens "(x<digits>" is claiming to be a lesson entry. If the
    // group is then malformed the line must NOT fall through as prose: under
    // Pending that would leave the entry invisible to every rule, which is the
    // silence the whole feature exists to remove. Remember that we saw one so
    // the caller gets a bad-disposition finding instead of nothing.
    boolean saw_count_prefix = false;

    for (unsigned long i = 0; i + 2 < l.len; ++i) {
        if (l.str[i] != '(' || l.str[i + 1] != 'x') {
            continue;
        }
        unsigned long j = i + 2;
        unsigned long count = 0;
        while (j < l.len && l.str[j] >= '0' && l.str[j] <= '9' && j - (i + 2) < LEDGER_COUNT_MAX_DIGITS) {
            count = count * 10 + (unsigned long)(l.str[j] - '0');
            j++;
        }
        if (j == i + 2) {
            continue; // "(x-axis)": not a counter - a real one may follow.
        }
        saw_count_prefix = true;
        // A digit run longer than any real count is not one: accumulating it
        // would wrap and report a fabricated number. Under Pending it is still
        // reported, as a count that is not a count.
        if (j < l.len && l.str[j] >= '0' && l.str[j] <= '9') {
            continue;
        }
        if (j >= l.len || (l.str[j] != ')' && l.str[j] != ',')) {
            continue;
        }

        entry.count = count;
        entry.count_end = l.str + j;

        // The annotation may itself contain parentheses, so the group's end is
        // the balanced match, not the next ')'.
        unsigned long depth = 0;
        unsigned long close = l.len;
        for (unsigned long k = i; k < l.len; ++k) {
            if (l.str[k] == '(') {
                depth++;
            } else if (l.str[k] == ')') {
                depth--;
                if (depth == 0) {
                    close = k;
                    break;
                }
            }
        }
        if (close == l.len) {
            snprintf(entry.problem, sizeof(entry.problem), "the count group has no closing ')'");
            entry.paren_close = l.str + l.len;
        } else {
            entry.paren_close = l.str + close;
        }
        entry.annotated = (l.str[j] == ',');

        entry.slug = ledger_line_slug(l);

        if (entry.annotated && entry.problem[0] == '\0') {
            Aids_String_Slice annotation =
                aids_string_slice_from_parts(l.str + j + 1, close - (j + 1));
            aids_string_slice_trim(&annotation);
            Aids_String_Slice verb = ledger_take_word(&annotation);
            if (!ledger_disposition_from_slice(verb, &entry.disposition)) {
                snprintf(entry.problem, sizeof(entry.problem),
                         "'" SS_Fmt "' is not " LEDGER_DISPOSITION_VALUES_CSTR, SS_Arg(verb));
            } else {
                Aids_String_Slice date = annotation;
                if (date.len > 10) {
                    date.len = 10;
                }
                if (!ledger_slice_is_date(date)) {
                    snprintf(entry.problem, sizeof(entry.problem), "%s needs a YYYY-MM-DD date",
                             Ledger_Disposition_Strings[entry.disposition]);
                } else {
                    Aids_String_Slice rest =
                        aids_string_slice_from_parts(annotation.str + 10, annotation.len - 10);
                    aids_string_slice_trim_left(&rest);
                    (void)ledger_parse_payload(&entry, rest);
                }
            }
        }

        *out = entry;
        return true;
    }

    // "(x3:" or a "(x3" that runs off the end of the line: it opened a count
    // group and never finished one. Report it rather than passing it over.
    if (saw_count_prefix) {
        entry.slug = ledger_line_slug(l);
        snprintf(entry.problem, sizeof(entry.problem),
                 "the count is not '(xN)' or '(xN, <disposition>)'");
        *out = entry;
        return true;
    }

    return false;
}

// A PROMOTE hands the change to a tatr task, so the reference has to name one:
// the promoted edit then goes through the ordinary plan, review, retro and
// close guards instead of being applied straight out of the ledger.
static size_t ledger_check_promotion_task(const Aids_String_Slice *tasks_dir,
                                          const Ledger_Entry *entry) {
    if (!ishuid(&entry->payload)) {
        printf("ledger: dangling-promotion-task: " SS_Fmt ": '" SS_Fmt "' is not a task ID\n",
               SS_Arg(entry->slug), SS_Arg(entry->payload));
        return 1;
    }

    char path_buffer[PATH_MAX];
    int written = snprintf(path_buffer, sizeof(path_buffer), SS_Fmt "/" SS_Fmt "/%s",
                           SS_Arg(*tasks_dir), SS_Arg(entry->payload), TASK_FILE_NAME_CSTR);
    if (written < 0 || (size_t)written >= sizeof(path_buffer) || access(path_buffer, F_OK) != 0) {
        printf("ledger: dangling-promotion-task: " SS_Fmt ": task '" SS_Fmt "' does not exist\n",
               SS_Arg(entry->slug), SS_Arg(entry->payload));
        return 1;
    }

    return 0;
}

// Every entry under "## Pending promotions" owes a disposition. Ledger findings
// carry no task ID, so they are printed here rather than through check_finding:
// tasks/EXEMPTIONS.md keys on a task, and there is nothing to key on.
static size_t ledger_check_pending_entry(const Aids_String_Slice *tasks_dir,
                                         Aids_String_Slice line) {
    Ledger_Entry entry = {0};
    if (!ledger_entry_parse(line, &entry)) {
        // A bullet under Pending is an entry making a claim, so it owes a count
        // to make the claim about. Dropping the count entirely would otherwise
        // park the lesson silently - the same invisibility a malformed count
        // group has, and the same answer. A wrapped continuation line is not
        // "- " prefixed once trimmed, so only real bullets reach this.
        Aids_String_Slice bullet = aids_string_slice_from_cstr("- ");
        if (aids_string_slice_starts_with(&line, bullet)) {
            printf("ledger: bad-disposition: " SS_Fmt ": a pending entry needs an '(xN)' count\n",
                   SS_Arg(ledger_line_slug(line)));
            return 1;
        }
        return 0;
    }

    if (entry.problem[0] != '\0') {
        printf("ledger: bad-disposition: " SS_Fmt ": %s\n", SS_Arg(entry.slug), entry.problem);
        return 1;
    }

    if (entry.disposition == Ledger_Disposition_NONE) {
        printf("ledger: promotion-awaiting-decision: " SS_Fmt " (x%lu) has no disposition\n",
               SS_Arg(entry.slug), entry.count);
        return 1;
    }

    // The deferral covered the count it was taken at. The lesson has recurred
    // since, so the question is live again.
    if (entry.disposition == Ledger_Disposition_DEFER && entry.count > entry.defer_count) {
        printf("ledger: promotion-awaiting-decision: " SS_Fmt " (x%lu) was deferred at x%lu\n",
               SS_Arg(entry.slug), entry.count, entry.defer_count);
        return 1;
    }

    if (entry.disposition == Ledger_Disposition_PROMOTE) {
        return ledger_check_promotion_task(tasks_dir, &entry);
    }

    return 0;
}

// Builds the ledger path the way check and the ledger command both accept it:
// absolute as given, otherwise relative to the root tatr is running against.
static boolean ledger_path_build(const Aids_String_Slice *cwd, const char *ledger_arg,
                                 char *buffer, size_t buffer_size) {
    int written;
    if (ledger_arg[0] == '/') {
        written = snprintf(buffer, buffer_size, "%s", ledger_arg);
    } else {
        written = snprintf(buffer, buffer_size, SS_Fmt "/%s", SS_Arg(*cwd), ledger_arg);
    }
    return written >= 0 && (size_t)written < buffer_size;
}

// Lints the lessons ledger: outside "## Pending promotions" a lesson at three or
// more occurrences is a stalled promotion; inside it, every entry owes a
// disposition. Returns findings printed.
static size_t check_ledger(const Aids_String_Slice *cwd, const Aids_String_Slice *tasks_dir,
                           const char *ledger_arg) {
    size_t findings = 0;
    Aids_String_Slice content = {0};
    char path_buffer[PATH_MAX];

    if (!ledger_path_build(cwd, ledger_arg, path_buffer, sizeof(path_buffer))) {
        printf("ledger: unreadable: '%s'\n", ledger_arg);
        return 1;
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
            findings += ledger_check_pending_entry(tasks_dir, l);
            continue;
        }
        // Find "(xN)" with N >= 3.
        for (unsigned long i = 0; i + 3 < l.len; ++i) {
            if (l.str[i] != '(' || l.str[i + 1] != 'x') {
                continue;
            }
            unsigned long j = i + 2;
            unsigned long count = 0;
            while (j < l.len && l.str[j] >= '0' && l.str[j] <= '9' &&
                   j - (i + 2) < LEDGER_COUNT_MAX_DIGITS) {
                count = count * 10 + (unsigned long)(l.str[j] - '0');
                j++;
            }
            if (j >= l.len || l.str[j] != ')' || j <= i + 2) {
                // "(x-axis)", "(x2, enforced)", a digit run too long to be a
                // real count: not a counter here - keep scanning; a real one may
                // follow later on the line.
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
    Check_Exemptions exemptions = {0};
    Task_Graph graph = {0};
    boolean graph_loaded = false;
    boolean full_scan = false;

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

    check_exemptions_init(&exemptions);

    if (id_str != NULL) {
        Aids_String_Slice id = aids_string_slice_from_cstr(id_str);
        if (task_resolve(&ctx->cwd, &id, &tasks_dir, &task_file_path) != AIDS_OK) {
            return_defer(1);
        }
        check_exemptions_load(&exemptions, &tasks_dir);
        task_graph_init(&graph);
        graph_loaded = true;
        if (task_graph_load(&tasks_dir, &graph) != AIDS_OK) {
            return_defer(1);
        }
        findings += check_task(&exemptions, &graph, &tasks_dir, &id);
    } else {
        full_scan = true;
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
            check_exemptions_load(&exemptions, &tasks_path);
            if (graph_loaded) {
                task_graph_free(&graph);
            }
            task_graph_init(&graph);
            graph_loaded = true;
            if (task_graph_load(&tasks_path, &graph) != AIDS_OK) {
                return_defer(1);
            }
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
                findings += check_task(&exemptions, &graph, &tasks_path, huid_entry);
            }
            cleanup_string_slice_array(&entries);
        }
    }

    // Only a full scan evaluated every task's rules, so only a full scan can
    // tell an exemption that no longer applies from one whose task was simply
    // not looked at.
    if (full_scan) {
        findings += check_exemptions_report_unused(&exemptions);
    }

    if (ledger_arg != NULL) {
        // A PROMOTE disposition names a task, so the ledger check needs the same
        // tasks dir every other command resolves - not one of the project dirs a
        // recursive scan happened to walk. The per-ID branch already resolved it
        // through task_resolve; only a full scan has to walk for it.
        Aids_String_Slice ledger_tasks_dir = {0};
        if (tasks_dir.str == NULL &&
            tasks_dir_path_build(&ctx->cwd, &ledger_tasks_dir) != AIDS_OK) {
            return_defer(1);
        }
        findings += check_ledger(&ctx->cwd,
                                 tasks_dir.str != NULL ? &tasks_dir : &ledger_tasks_dir,
                                 ledger_arg);
        if (ledger_tasks_dir.str != NULL) {
            AIDS_FREE(ledger_tasks_dir.str);
        }
    }

    if (findings > 0) {
        result = 1;
    }

defer:
    if (graph_loaded) {
        task_graph_free(&graph);
    }
    check_exemptions_free(&exemptions);
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

// ---------------------------------------------------------------------------
// ledger: list the promotions waiting on a decision, and record the one the
// user made. Recording writes the ledger file and NOTHING else - applying a
// promotion is the referenced task's job, so the change goes through the
// ordinary plan, review, retro and close guards rather than being made here.
// ---------------------------------------------------------------------------

// Renders what a pending entry is waiting for, as the third field of a listing
// row. A re-raised DEFER reads as awaiting-decision, because it is.
static void ledger_entry_print_state(const Ledger_Entry *entry) {
    if (entry->problem[0] != '\0') {
        printf("invalid: %s", entry->problem);
        return;
    }
    switch (entry->disposition) {
    case Ledger_Disposition_NONE:
        printf("awaiting-decision");
        return;
    case Ledger_Disposition_PROMOTE:
        printf("PROMOTE " SS_Fmt, SS_Arg(entry->payload));
        return;
    case Ledger_Disposition_DEFER:
        if (entry->count > entry->defer_count) {
            printf("awaiting-decision (deferred at x%lu)", entry->defer_count);
        } else {
            printf("DEFER at x%lu", entry->defer_count);
        }
        return;
    case Ledger_Disposition_RETIRE:
        printf("RETIRE");
        return;
    case Ledger_Disposition_ABSORBED:
        printf("ABSORBED " SS_Fmt, SS_Arg(entry->payload));
        return;
    }
    AIDS_UNREACHABLE("ledger_entry_print_state");
}

// A recorded disposition is settled unless it is a DEFER the count has passed:
// that one asked to be revisited, and this is the revisit.
static boolean ledger_entry_is_decidable(const Ledger_Entry *entry) {
    if (entry->disposition == Ledger_Disposition_NONE) {
        return true;
    }
    return entry->disposition == Ledger_Disposition_DEFER && entry->count > entry->defer_count;
}

// A disposition is one annotation on one line, so a payload carrying a line
// break or a tab would split the entry in two and no re-parse could put it back
// together. Those bytes are refused at the argument.
//
// Parentheses are NOT refused here: a balanced pair is ordinary prose ("wait for
// the (rare) third sighting") and the entry parser matches parens by depth. An
// UNBALANCED one does break the grammar, and the re-parse before the write is
// what catches it - the one guard that can tell the two cases apart. That guard
// has to compare the payload to catch a stray ')', which closes the group early
// rather than leaving it open; comparing only the disposition and count would
// let a truncated decision through.
static boolean ledger_payload_is_writable(const char *value, const char *flag) {
    for (const char *p = value; *p != '\0'; ++p) {
        if (*p == '\n' || *p == '\r' || *p == '\t') {
            aids_log(AIDS_ERROR, "Invalid %s: a disposition is one annotation on one line, "
                     "so it must not contain a newline or a tab", flag);
            return false;
        }
    }
    if (value[0] == '\0') {
        aids_log(AIDS_ERROR, "Invalid %s: empty", flag);
        return false;
    }
    return true;
}

static int main_ledger(const Tatr_Context *ctx) {
    int result = 0;
    Argparse_Parser parser = {0};
    Aids_String_Slice content = {0};
    Aids_String_Slice tasks_dir = {0};
    Aids_String_Builder annotation_sb = {0};
    Aids_String_Builder line_sb = {0};
    Aids_String_Builder out_sb = {0};
    boolean builders_initialized = false;
    char path_buffer[PATH_MAX];

    argparse_parser_init(&parser, "tatr ledger",
                         "List the lessons ledger's pending promotions and record a decision",
                         TATR_VERSION);

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'L',
        .long_name = "ledger",
        .description = "Ledger file to read (default: LESSONS.md, relative to the root)",
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 's',
        .long_name = "slug",
        .description = "Lesson slug to record a disposition for (default: list them all)",
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'D',
        .long_name = "disposition",
        .description = "The user's decision: " LEDGER_DISPOSITION_VALUES_CSTR,
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 't',
        .long_name = "task",
        .description = "PROMOTE: the task that will make the promoted change",
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'R',
        .long_name = "reason",
        .description = "DEFER, RETIRE: why",
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'T',
        .long_name = "target",
        .description = "ABSORBED: the tool or template that made the lesson unnecessary",
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    if (argparse_parse(&parser, ctx->argc, ctx->argv) != ARG_OK) {
        return_defer(1);
    }

    char *ledger_arg = argparse_get_value(&parser, "ledger");
    char *slug_arg = argparse_get_value(&parser, "slug");
    char *disposition_arg = argparse_get_value(&parser, "disposition");
    char *task_arg = argparse_get_value(&parser, "task");
    char *reason_arg = argparse_get_value(&parser, "reason");
    char *target_arg = argparse_get_value(&parser, "target");

    if (ledger_arg == NULL) {
        ledger_arg = (char *)"LESSONS.md";
    }

    if (slug_arg == NULL && disposition_arg != NULL) {
        aids_log(AIDS_ERROR, "--disposition needs --slug: name the lesson the decision is about");
        return_defer(1);
    }

    // Listing takes no payload. Accepting one silently would report the ledger
    // unchanged while looking like a decision had been recorded.
    if (disposition_arg == NULL &&
        (task_arg != NULL || reason_arg != NULL || target_arg != NULL)) {
        aids_log(AIDS_ERROR, "--task, --reason and --target belong to a decision: pass "
                 "--slug and --disposition, or none of them to list");
        return_defer(1);
    }

    if (!ledger_path_build(&ctx->cwd, ledger_arg, path_buffer, sizeof(path_buffer))) {
        aids_log(AIDS_ERROR, "Failed to build ledger path: path too long");
        return_defer(1);
    }

    Aids_String_Slice path = aids_string_slice_from_cstr(path_buffer);
    if (aids_io_isdir(&path) || aids_io_read(&path, &content, "r") != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to read ledger '%s'", ledger_arg);
        return_defer(1);
    }

    // ------------------------------------------------------------------
    // Validate every argument before a single byte is built, so a refusal
    // leaves the ledger exactly as it was.
    // ------------------------------------------------------------------
    Ledger_Disposition disposition = Ledger_Disposition_NONE;
    if (disposition_arg != NULL) {
        Aids_String_Slice d = aids_string_slice_from_cstr(disposition_arg);
        if (!ledger_disposition_from_slice(d, &disposition)) {
            aids_log(AIDS_ERROR, "Invalid disposition '%s': expected "
                     LEDGER_DISPOSITION_VALUES_CSTR, disposition_arg);
            return_defer(1);
        }
    } else if (slug_arg != NULL) {
        aids_log(AIDS_ERROR, "--slug needs --disposition: expected "
                 LEDGER_DISPOSITION_VALUES_CSTR);
        return_defer(1);
    }

    if (disposition != Ledger_Disposition_NONE) {
        const char *verb = Ledger_Disposition_Strings[disposition];
        boolean wants_task = (disposition == Ledger_Disposition_PROMOTE);
        boolean wants_reason = (disposition == Ledger_Disposition_DEFER ||
                                disposition == Ledger_Disposition_RETIRE);
        boolean wants_target = (disposition == Ledger_Disposition_ABSORBED);

        if (wants_task && task_arg == NULL) {
            aids_log(AIDS_ERROR, "PROMOTE requires --task <id>: the promoted change is made by "
                     "a task, so it goes through the ordinary plan, review and close guards");
            return_defer(1);
        }
        if (wants_reason && reason_arg == NULL) {
            aids_log(AIDS_ERROR, "%s requires --reason <text>", verb);
            return_defer(1);
        }
        if (wants_target && target_arg == NULL) {
            aids_log(AIDS_ERROR, "ABSORBED requires --target <text>: name the tool or template "
                     "that made the lesson unnecessary");
            return_defer(1);
        }
        if (!wants_task && task_arg != NULL) {
            aids_log(AIDS_ERROR, "%s takes no --task", verb);
            return_defer(1);
        }
        if (!wants_reason && reason_arg != NULL) {
            aids_log(AIDS_ERROR, "%s takes no --reason", verb);
            return_defer(1);
        }
        if (!wants_target && target_arg != NULL) {
            aids_log(AIDS_ERROR, "%s takes no --target", verb);
            return_defer(1);
        }
        if (reason_arg != NULL && !ledger_payload_is_writable(reason_arg, "--reason")) {
            return_defer(1);
        }
        if (target_arg != NULL && !ledger_payload_is_writable(target_arg, "--target")) {
            return_defer(1);
        }

        if (wants_task) {
            Aids_String_Slice task_id = aids_string_slice_from_cstr(task_arg);
            Aids_String_Slice task_file_path = {0};
            if (task_resolve(&ctx->cwd, &task_id, &tasks_dir, &task_file_path) != AIDS_OK) {
                return_defer(1);
            }
            AIDS_FREE(task_file_path.str);
        }
    }

    // ------------------------------------------------------------------
    // Walk the file by byte offset rather than by token, so the entry's
    // parens can be spliced without re-rendering any other line.
    // ------------------------------------------------------------------
    Aids_String_Slice any_heading = aids_string_slice_from_cstr("## ");
    Aids_String_Slice wanted_slug = (slug_arg != NULL)
        ? aids_string_slice_from_cstr(slug_arg)
        : (Aids_String_Slice){0};
    boolean in_pending = false;
    size_t matches = 0;
    Ledger_Entry match = {0};
    Aids_String_Slice match_line = {0};

    unsigned long pos = 0;
    while (pos < content.len) {
        unsigned long end = pos;
        while (end < content.len && content.str[end] != '\n') {
            end++;
        }
        Aids_String_Slice line = aids_string_slice_from_parts(content.str + pos, end - pos);

        Aids_String_Slice trimmed = line;
        aids_string_slice_trim(&trimmed);
        if (aids_string_slice_starts_with(&trimmed, any_heading)) {
            in_pending = slice_contains_cstr(trimmed, "Pending promotions");
        } else if (in_pending) {
            Ledger_Entry entry = {0};
            if (ledger_entry_parse(line, &entry)) {
                if (slug_arg == NULL) {
                    printf(SS_Fmt "\tx%lu\t", SS_Arg(entry.slug), entry.count);
                    ledger_entry_print_state(&entry);
                    printf("\n");
                } else if (entry.slug.len == wanted_slug.len &&
                           memcmp(entry.slug.str, wanted_slug.str, wanted_slug.len) == 0) {
                    matches++;
                    match = entry;
                    match_line = line;
                }
            }
        }

        pos = (end < content.len) ? end + 1 : end;
    }

    if (slug_arg == NULL) {
        return_defer(0);
    }

    if (matches == 0) {
        aids_log(AIDS_ERROR, "No entry '%s' under '## Pending promotions' in '%s'",
                 slug_arg, ledger_arg);
        return_defer(1);
    }
    if (matches > 1) {
        aids_log(AIDS_ERROR, "'%s' appears %zu times under '## Pending promotions': "
                 "fix the duplicate by hand", slug_arg, matches);
        return_defer(1);
    }
    if (match.problem[0] != '\0') {
        aids_log(AIDS_ERROR, "'%s' carries a malformed disposition (%s): fix it by hand",
                 slug_arg, match.problem);
        return_defer(1);
    }
    if (!ledger_entry_is_decidable(&match)) {
        aids_log(AIDS_ERROR, "'%s' already carries a %s disposition: edit '%s' by hand to "
                 "change a decision that was already made", slug_arg,
                 Ledger_Disposition_Strings[match.disposition], ledger_arg);
        return_defer(1);
    }

    // ------------------------------------------------------------------
    // Render the annotation, splice it in, and verify the result parses back
    // to the decision that was asked for before anything reaches disk.
    // ------------------------------------------------------------------
    aids_string_builder_init(&annotation_sb);
    aids_string_builder_init(&line_sb);
    aids_string_builder_init(&out_sb);
    builders_initialized = true;

    char date_buffer[16];
    time_t current_time;
    struct tm *time_info;
    time(&current_time);
    time_info = localtime(&current_time);
    if (time_info == NULL || strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d", time_info) == 0) {
        aids_log(AIDS_ERROR, "Failed to render today's date");
        return_defer(1);
    }

    switch (disposition) {
    case Ledger_Disposition_PROMOTE:
        aids_string_builder_append(&annotation_sb, "PROMOTE %s -> %s", date_buffer, task_arg);
        break;
    case Ledger_Disposition_DEFER:
        aids_string_builder_append(&annotation_sb, "DEFER %s at x%lu: %s",
                                   date_buffer, match.count, reason_arg);
        break;
    case Ledger_Disposition_RETIRE:
        aids_string_builder_append(&annotation_sb, "RETIRE %s: %s", date_buffer, reason_arg);
        break;
    case Ledger_Disposition_ABSORBED:
        aids_string_builder_append(&annotation_sb, "ABSORBED %s by %s", date_buffer, target_arg);
        break;
    case Ledger_Disposition_NONE:
    default:
        AIDS_UNREACHABLE("main_ledger disposition");
    }
    Aids_String_Slice annotation = {0};
    aids_string_builder_to_slice(&annotation_sb, &annotation);

    // The splice below is pointer arithmetic, so it needs a real count group to
    // splice into. Only an entry carrying a `problem` lacks one, and the refusal
    // above already returned - but if that guard ever goes, the subtraction
    // wraps into a runaway length and the process hangs building a string
    // instead of reporting anything. Fail loudly at the invariant instead.
    if (match.count_end == NULL || match.paren_close == NULL) {
        AIDS_UNREACHABLE("main_ledger: spliceable entry without a count group");
    }

    // The new line keeps every byte of the old one except the count group's
    // annotation: the lesson text, the wrap, the indentation and the count are
    // the author's, not tatr's.
    unsigned long head_len = (unsigned long)(match.count_end - match_line.str);
    unsigned long tail_len = match_line.len - (unsigned long)(match.paren_close - match_line.str);
    aids_string_builder_append_slice(&line_sb,
        aids_string_slice_from_parts(match_line.str, head_len));
    aids_string_builder_append(&line_sb, ", " SS_Fmt, SS_Arg(annotation));
    aids_string_builder_append_slice(&line_sb,
        aids_string_slice_from_parts(match.paren_close, tail_len));
    Aids_String_Slice new_line = {0};
    aids_string_builder_to_slice(&line_sb, &new_line);

    // tatr never writes a record it cannot read back. Re-parse the rendered line
    // and require the WHOLE decision back, payload included: an unbalanced
    // parenthesis in the reason or target either leaves the group unclosed (a
    // parse problem) or closes it early, which truncates the payload and spills
    // the remainder into the lesson text while everything else still looks
    // right. Comparing the payload is the only thing that catches the second.
    const char *intended_payload = (disposition == Ledger_Disposition_PROMOTE) ? task_arg
                                 : (disposition == Ledger_Disposition_ABSORBED) ? target_arg
                                 : reason_arg;
    unsigned long intended_len = strlen(intended_payload);
    Ledger_Entry verify = {0};
    if (!ledger_entry_parse(new_line, &verify) || verify.problem[0] != '\0' ||
        verify.disposition != disposition || verify.count != match.count ||
        verify.payload.len != intended_len ||
        memcmp(verify.payload.str, intended_payload, intended_len) != 0 ||
        (disposition == Ledger_Disposition_DEFER && verify.defer_count != match.count)) {
        aids_log(AIDS_ERROR, "Refusing to write '%s': the entry would not parse back as a %s "
                 "disposition (an unbalanced parenthesis in the text is the usual cause)",
                 ledger_arg, Ledger_Disposition_Strings[disposition]);
        return_defer(1);
    }

    unsigned long line_offset = (unsigned long)(match_line.str - content.str);
    aids_string_builder_append_slice(&out_sb,
        aids_string_slice_from_parts(content.str, line_offset));
    aids_string_builder_append_slice(&out_sb, new_line);
    aids_string_builder_append_slice(&out_sb,
        aids_string_slice_from_parts(content.str + line_offset + match_line.len,
                                     content.len - line_offset - match_line.len));
    Aids_String_Slice out = {0};
    aids_string_builder_to_slice(&out_sb, &out);

    if (aids_io_write(&path, &out, "w") != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to write ledger '%s': %s", ledger_arg, aids_failure_reason());
        return_defer(1);
    }

    printf("%s\t%s\t", slug_arg, Ledger_Disposition_Strings[disposition]);
    switch (disposition) {
    case Ledger_Disposition_PROMOTE: printf("%s\n", task_arg); break;
    case Ledger_Disposition_DEFER:   printf("at x%lu\n", match.count); break;
    case Ledger_Disposition_RETIRE:  printf("%s\n", reason_arg); break;
    case Ledger_Disposition_ABSORBED: printf("%s\n", target_arg); break;
    case Ledger_Disposition_NONE:
    default: break;
    }

defer:
    if (builders_initialized) {
        aids_string_builder_free(&annotation_sb);
        aids_string_builder_free(&line_sb);
        aids_string_builder_free(&out_sb);
    }
    if (content.str != NULL) {
        AIDS_FREE(content.str);
    }
    if (tasks_dir.str != NULL) {
        AIDS_FREE(tasks_dir.str);
    }
    argparse_parser_free(&parser);
    return result;
}

// ---------------------------------------------------------------------------
// scaffold: create a missing sibling record from RECORD_SCHEMAS. The same table
// `check` validates against, so a freshly scaffolded record passes the lint
// with its placeholders still in place - the author fills them in, they do not
// have to guess the shape first. There is no --force: an existing record is
// edited by hand, in the diff, where a reviewer sees the change.
// ---------------------------------------------------------------------------

// The default value a scaffolded header field carries. TASK and DATE resolve to
// real data and STATUS to a value its own rule accepts - a scaffolded record
// must pass `tatr check` with its placeholders still in place, or the author
// has to guess the shape before they can write anything. Everything else gets
// a TODO the author replaces.
static Aids_Result scaffold_field_value(Record_Kind kind,
                                        const char *field,
                                        Aids_String_Slice task_huid,
                                        Aids_String_Builder *out) {
    if (strcmp(field, "- TASK: ") == 0) {
        return aids_string_builder_append(out, SS_Fmt, SS_Arg(task_huid));
    }
    if (strcmp(field, "- DATE: ") == 0) {
        char now[HUID_LENGTH] = {0};
        if (huid(now) != AIDS_OK) {
            return AIDS_ERR;
        }
        return aids_string_builder_append(out, "%s", now);
    }
    if (strcmp(field, "- STATUS: ") == 0) {
        // The first value of the kind's vocabulary: a decision record is
        // written when the decision is made, a spike doc when it has a
        // recommendation.
        if (kind == Record_Kind_SPIKE) {
            return aids_string_builder_append(out, SS_Fmt, SS_Arg(Spike_Status_Strings[0]));
        }
        return aids_string_builder_append(out, "ACCEPTED");
    }
    if (strcmp(field, "- REVIEW ROUNDS: ") == 0) {
        return aids_string_builder_append(out, "0");
    }
    return aids_string_builder_append(out, "TODO");
}

// Renders a record from its schema: the title line, the required header fields,
// then either the literal body template or one stub per required section.
static Aids_Result scaffold_render(Record_Kind kind,
                                   Aids_String_Slice task_huid,
                                   Aids_String_Slice title,
                                   Aids_String_Slice *out) {
    const Record_Schema *schema = &RECORD_SCHEMAS[kind];
    Aids_String_Builder sb = {0};
    Aids_Result result = AIDS_OK;

    aids_string_builder_init(&sb);

    if (aids_string_builder_append(&sb, "%s" SS_Fmt "\n\n",
                                   schema->title_prefix, SS_Arg(title)) != AIDS_OK) {
        return_defer(AIDS_ERR);
    }

    for (size_t i = 0; i < RECORD_MAX_FIELDS && schema->fields[i] != NULL; ++i) {
        if (aids_string_builder_append(&sb, "%s", schema->fields[i]) != AIDS_OK) {
            return_defer(AIDS_ERR);
        }
        if (scaffold_field_value(kind, schema->fields[i], task_huid, &sb) != AIDS_OK) {
            return_defer(AIDS_ERR);
        }
        if (aids_string_builder_append(&sb, "\n") != AIDS_OK) {
            return_defer(AIDS_ERR);
        }
    }
    if (aids_string_builder_append(&sb, "\n") != AIDS_OK) {
        return_defer(AIDS_ERR);
    }

    if (schema->body_template != NULL) {
        if (aids_string_builder_append(&sb, "%s", schema->body_template) != AIDS_OK) {
            return_defer(AIDS_ERR);
        }
    } else {
        for (size_t i = 0; schema->sections[i] != NULL; ++i) {
            // The stub is derived from the heading, so a new section needs no
            // second table entry - and it is non-blank, so the scaffolded
            // record is already schema-clean.
            const char *heading = schema->sections[i];
            if (aids_string_builder_append(&sb, "%s\n\nTODO: %s\n", heading, heading + 3) != AIDS_OK) {
                return_defer(AIDS_ERR);
            }
            if (schema->sections[i + 1] != NULL &&
                aids_string_builder_append(&sb, "\n") != AIDS_OK) {
                return_defer(AIDS_ERR);
            }
        }
    }

    aids_string_builder_to_slice(&sb, out);

defer:
    if (result != AIDS_OK) {
        aids_string_builder_free(&sb);
    }
    return result;
}

static int main_scaffold(const Tatr_Context *ctx) {
    int result = 0;
    Argparse_Parser parser = {0};
    Aids_String_Slice tasks_dir = {0};
    Aids_String_Slice task_file_path = {0};
    Aids_String_Slice content = {0};
    Task task = {0};
    boolean task_initialized = false;

    argparse_parser_init(&parser, "tatr scaffold",
                         "Create a missing sibling record from the schema", TATR_VERSION);

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'I',
        .long_name = "id",
        .description = "Task ID (format YYYYMMDD-HHMMSS)",
        .type = ARGUMENT_TYPE_POSITIONAL,
        .required = 1
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'K',
        .long_name = "record",
        .description = "Record kind (" RECORD_VALUES_CSTR ")",
        .type = ARGUMENT_TYPE_POSITIONAL,
        .required = 0
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'l',
        .long_name = "list",
        .description = "List every record kind for the task with its path and presence",
        .type = ARGUMENT_TYPE_FLAG,
        .required = 0
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'n',
        .long_name = "dry-run",
        .description = "Print the path and record kind that would be written, and write nothing",
        .type = ARGUMENT_TYPE_FLAG,
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

    boolean list = argparse_get_flag(&parser, "list");
    boolean dry_run = argparse_get_flag(&parser, "dry-run");
    char *record_str = argparse_get_value(&parser, "record");

    if (list) {
        for (size_t i = 0; i < RECORD_KIND_COUNT; ++i) {
            const Record_Schema *schema = &RECORD_SCHEMAS[i];
            printf("%s\t" SS_Fmt "/" SS_Fmt "/%s\t%s\n",
                   schema->name, SS_Arg(tasks_dir), SS_Arg(id), schema->file_name,
                   task_sibling_exists(&tasks_dir, &id, schema->file_name) ? "present" : "missing");
        }
        return_defer(0);
    }

    if (record_str == NULL) {
        aids_log(AIDS_ERROR, "Missing record kind: expected " RECORD_VALUES_CSTR " (or --list)");
        return_defer(1);
    }
    Aids_String_Slice record_slice = aids_string_slice_from_cstr(record_str);
    Record_Kind kind = Record_Kind_TASK;
    if (!record_kind_from_string(&record_slice, &kind)) {
        aids_log(AIDS_ERROR, "Invalid record kind '%s': expected " RECORD_VALUES_CSTR, record_str);
        return_defer(1);
    }
    const Record_Schema *schema = &RECORD_SCHEMAS[kind];

    if (kind == Record_Kind_TASK) {
        aids_log(AIDS_ERROR, "TASK.md is created by `tatr new`, not scaffolded: it is typed "
                 "metadata rather than a prose record");
        return_defer(1);
    }

    char path_buffer[PATH_MAX];
    int written = snprintf(path_buffer, sizeof(path_buffer), SS_Fmt "/" SS_Fmt "/%s",
                           SS_Arg(tasks_dir), SS_Arg(id), schema->file_name);
    if (written < 0 || (size_t)written >= sizeof(path_buffer)) {
        aids_log(AIDS_ERROR, "Failed to build record path: path too long");
        return_defer(1);
    }

    // Refuse before rendering anything: a scaffold that would clobber an
    // existing record must not do half of the work first.
    if (access(path_buffer, F_OK) == 0) {
        aids_log(AIDS_ERROR, "Refusing to overwrite '%s': edit the existing record by hand", path_buffer);
        return_defer(1);
    }

    if (dry_run) {
        printf("%s\t%s\n", path_buffer, schema->name);
        return_defer(0);
    }

    // The record's title carries the task's own title, so a reviewer reading
    // REVIEW.md alone knows what was reviewed.
    task_init_empty(&task);
    task_initialized = true;
    if (task_load(&task_file_path, &task) != AIDS_OK) {
        return_defer(1);
    }

    if (scaffold_render(kind, id, task.title, &content) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to render %s: %s", schema->file_name, aids_failure_reason());
        return_defer(1);
    }

    Aids_String_Slice path = aids_string_slice_from_cstr(path_buffer);
    if (aids_io_write(&path, &content, "w") != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to write '%s': %s", path_buffer, aids_failure_reason());
        return_defer(1);
    }

    printf("%s\t%s\n", path_buffer, schema->name);

defer:
    if (task_initialized) {
        task_cleanup(&task);
    }
    if (content.str != NULL) {
        AIDS_FREE(content.str);
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
// proofs: print each Definition of Done proof as one
// "<n><TAB><kind><TAB><text>" line. tatr does not execute anything - a `cmd:`
// proof's shell text round-trips verbatim, and running it is the caller's
// decision, made in the caller's shell where the user can see the command.
// ---------------------------------------------------------------------------

// Prints a proof's text on one line. A whitespace run collapses to a single
// space only when it contains a byte that would break the record format - a
// newline (the line wrap of a continued bullet) or a tab (the field separator
// of the output itself). Everything else is passed through byte for byte, so a
// `cmd:` proof whose shell text depends on its spacing (`grep -q "a  b"`)
// round-trips verbatim rather than coming back subtly different from what the
// author wrote.
static void proof_print_text(Aids_String_Slice text) {
    size_t i = 0;
    while (i < text.len) {
        if (!isspace(text.str[i])) {
            putchar(text.str[i]);
            i++;
            continue;
        }
        size_t run = i;
        boolean collapse = false;
        while (run < text.len && isspace(text.str[run])) {
            char c = text.str[run];
            // Exactly the two bytes that would break the record format, and no
            // others: a newline ends the line, a tab starts a fourth field.
            // A vertical tab or form feed breaks neither, so it survives like
            // any other byte the author wrote.
            if (c == '\n' || c == '\r' || c == '\t') {
                collapse = true;
            }
            run++;
        }
        if (collapse) {
            putchar(' ');
        } else {
            for (size_t j = i; j < run; ++j) {
                putchar(text.str[j]);
            }
        }
        i = run;
    }
}

static int main_proofs(const Tatr_Context *ctx) {
    int result = 0;
    Argparse_Parser parser = {0};
    Aids_String_Slice tasks_dir = {0};
    Aids_String_Slice task_file_path = {0};
    Aids_String_Slice raw = {0};
    Task task = {0};
    boolean task_initialized = false;

    argparse_parser_init(&parser, "tatr proofs",
                         "List the Definition of Done proofs of a task", TATR_VERSION);

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'I',
        .long_name = "id",
        .description = "Task ID (format YYYYMMDD-HHMMSS)",
        .type = ARGUMENT_TYPE_POSITIONAL,
        .required = 1
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'k',
        .long_name = "kind",
        .description = "Only list proofs of this kind (test, cmd or manual)",
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

    char *kind_str = argparse_get_value(&parser, "kind");
    int wanted = -1;
    if (kind_str != NULL) {
        for (size_t i = 0; i < ENUM_COUNT(PROOF_KIND_NAMES); ++i) {
            if (strcmp(kind_str, PROOF_KIND_NAMES[i]) == 0) {
                wanted = (int)i;
                break;
            }
        }
        if (wanted < 0) {
            aids_log(AIDS_ERROR, "Invalid proof kind '%s': expected " PROOF_MARKERS_CSTR
                     " without the colon", kind_str);
            return_defer(1);
        }
    }

    if (task_resolve(&ctx->cwd, &id, &tasks_dir, &task_file_path) != AIDS_OK) {
        return_defer(1);
    }

    task_init_empty(&task);
    task_initialized = true;
    if (task_load_raw(&task_file_path, &task, &raw) != AIDS_OK) {
        return_defer(1);
    }

    size_t n = 0;
    size_t cursor = 0;
    Aids_String_Slice item = {0};
    while (artifact_next_dod_item(raw, &cursor, &item)) {
        size_t proof_cursor = 0;
        Proof_Kind kind = Proof_Kind_TEST;
        Aids_String_Slice text = {0};
        while (artifact_next_proof(item, &proof_cursor, &kind, &text)) {
            if (wanted >= 0 && (int)kind != wanted) {
                continue;
            }
            n++;
            printf("%zu\t%s\t", n, PROOF_KIND_NAMES[kind]);
            proof_print_text(text);
            printf("\n");
        }
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

// ---------------------------------------------------------------------------
// claim / release / claims: the parallel-session coordination verbs.
// ---------------------------------------------------------------------------

static int main_claim(const Tatr_Context *ctx) {
    int result = 0;
    Argparse_Parser parser = {0};
    Aids_String_Slice tasks_dir = {0};
    Aids_String_Slice task_file_path = {0};
    Claim existing = {0};

    argparse_parser_init(&parser, "tatr claim", "Claim a task for this session", TATR_VERSION);

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

    switch (claim_take(&tasks_dir, id)) {
    case Claim_Result_TAKEN:
        printf("Claimed " SS_Fmt "\n", SS_Arg(id));
        break;
    case Claim_Result_CONTENDED:
        // Naming the holder is the whole point: a contended claim is a
        // question about who, not a failure to report.
        if (claim_read(&tasks_dir, id, &existing) && existing.session.len > 0) {
            aids_log(AIDS_ERROR, "Task " SS_Fmt " is already claimed by session '" SS_Fmt "' ("
                     SS_Fmt "@" SS_Fmt ", since " SS_Fmt ")",
                     SS_Arg(id), SS_Arg(existing.session), SS_Arg(existing.owner),
                     SS_Arg(existing.host), SS_Arg(existing.since));
        } else {
            aids_log(AIDS_ERROR, "Task " SS_Fmt " is already claimed", SS_Arg(id));
        }
        fprintf(stderr, "  if that session is gone, recover it with "
                "`tatr release " SS_Fmt " --force`\n", SS_Arg(id));
        return_defer(1);
    case Claim_Result_ERROR:
        return_defer(1);
    }

defer:
    claim_free(&existing);
    if (tasks_dir.str != NULL) {
        AIDS_FREE(tasks_dir.str);
    }
    if (task_file_path.str != NULL) {
        AIDS_FREE(task_file_path.str);
    }
    argparse_parser_free(&parser);
    return result;
}

static int main_release(const Tatr_Context *ctx) {
    int result = 0;
    Argparse_Parser parser = {0};
    Aids_String_Slice tasks_dir = {0};
    Aids_String_Slice task_file_path = {0};
    Claim existing = {0};

    argparse_parser_init(&parser, "tatr release", "Release a claim on a task", TATR_VERSION);

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'I',
        .long_name = "id",
        .description = "Task ID (format YYYYMMDD-HHMMSS)",
        .type = ARGUMENT_TYPE_POSITIONAL,
        .required = 1
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'F',
        .long_name = "force",
        .description = "Release a claim held by another session (recovers a stale claim)",
        .type = ARGUMENT_TYPE_FLAG,
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

    if (!claim_read(&tasks_dir, id, &existing)) {
        aids_log(AIDS_ERROR, "Task " SS_Fmt " is not claimed", SS_Arg(id));
        return_defer(1);
    }

    // Releasing someone else's claim is a deliberate recovery, never a
    // side effect: tatr cannot tell a dead session from a slow one, so it
    // refuses to guess and makes the human say so.
    boolean force = argparse_get_flag(&parser, "force");
    if (!force && claim_held_by_other_session(&existing)) {
        aids_log(AIDS_ERROR, "Task " SS_Fmt " is claimed by session '" SS_Fmt "' ("
                 SS_Fmt "@" SS_Fmt ", since " SS_Fmt "), not by this one",
                 SS_Arg(id), SS_Arg(existing.session), SS_Arg(existing.owner),
                 SS_Arg(existing.host), SS_Arg(existing.since));
        fprintf(stderr, "  set TATR_SESSION to that id if it is yours, "
                "or recover the claim with --force if that session is gone\n");
        return_defer(1);
    }

    char path_buffer[PATH_MAX];
    if (!claim_path_build(&tasks_dir, id, path_buffer, sizeof(path_buffer))) {
        aids_log(AIDS_ERROR, "Failed to build claim path: path too long");
        return_defer(1);
    }
    // Re-read immediately before unlinking. This narrows, but cannot close, the
    // window in which the claim we decided about was released and re-taken by
    // another session: POSIX has no "unlink this exact file" primitive to hold
    // it open across. A `--force` release is deliberate anyway, and a
    // same-session one is racing only itself.
    if (!force) {
        Claim recheck = {0};
        boolean still_ours = claim_read(&tasks_dir, id, &recheck) &&
                             !claim_held_by_other_session(&recheck);
        claim_free(&recheck);
        if (!still_ours) {
            aids_log(AIDS_ERROR, "Task " SS_Fmt " changed hands while releasing it; nothing was removed",
                     SS_Arg(id));
            return_defer(1);
        }
    }
    if (unlink(path_buffer) != 0) {
        aids_log(AIDS_ERROR, "Failed to remove claim '%s': %s", path_buffer, strerror(errno));
        return_defer(1);
    }

    printf("Released " SS_Fmt "\n", SS_Arg(id));

defer:
    claim_free(&existing);
    if (tasks_dir.str != NULL) {
        AIDS_FREE(tasks_dir.str);
    }
    if (task_file_path.str != NULL) {
        AIDS_FREE(task_file_path.str);
    }
    argparse_parser_free(&parser);
    return result;
}

static int main_claims(const Tatr_Context *ctx) {
    int result = 0;
    Argparse_Parser parser = {0};
    Aids_String_Slice tasks_dir = {0};
    Aids_String_Slice claims_dir = {0};
    Aids_Array entries = {0};
    boolean entries_initialized = false;

    argparse_parser_init(&parser, "tatr claims", "List the claims in this tasks directory", TATR_VERSION);

    if (argparse_parse(&parser, ctx->argc, ctx->argv) != ARG_OK) {
        return_defer(1);
    }

    if (tasks_dir_path_build(&ctx->cwd, &tasks_dir) != AIDS_OK) {
        return_defer(1);
    }
    if (claims_dir_path_build(&tasks_dir, &claims_dir) != AIDS_OK) {
        return_defer(1);
    }

    // The directory is named on stderr, not stdout: a claim is scoped to one
    // tasks dir, and a caller reading the list needs to know WHICH - but the
    // machine-readable rows stay alone on stdout.
    fprintf(stderr, "claims in " SS_Fmt "\n", SS_Arg(claims_dir));

    if (!aids_io_isdir(&claims_dir)) {
        return_defer(0); // no claims directory means no claims
    }

    aids_array_init(&entries, sizeof(Aids_String_Slice));
    entries_initialized = true;
    if (aids_io_listdir(&claims_dir, &entries) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to list claims directory: %s", aids_failure_reason());
        return_defer(1);
    }
    aids_array_sort(&entries, string_slice_compare_fn);

    for (size_t i = 0; i < entries.count; ++i) {
        Aids_String_Slice *name = NULL;
        if (aids_array_get(&entries, i, (void **)&name) != AIDS_OK || !ishuid(name)) {
            continue;
        }
        Claim claim = {0};
        if (!claim_read(&tasks_dir, *name, &claim)) {
            continue;
        }
        printf(SS_Fmt "\t" SS_Fmt "\t" SS_Fmt "@" SS_Fmt "\t" SS_Fmt "\n",
               SS_Arg(*name), SS_Arg(claim.session), SS_Arg(claim.owner),
               SS_Arg(claim.host), SS_Arg(claim.since));
        claim_free(&claim);
    }

defer:
    if (entries_initialized) {
        cleanup_string_slice_array(&entries);
    }
    if (tasks_dir.str != NULL) {
        AIDS_FREE(tasks_dir.str);
    }
    if (claims_dir.str != NULL) {
        AIDS_FREE(claims_dir.str);
    }
    argparse_parser_free(&parser);
    return result;
}

// ---------------------------------------------------------------------------
// frontier: the open work under an Epic, at a glance. Prints one row per child,
// never a task body - the point is to decide what to pick up without loading
// the whole map. Deterministic: READY before BLOCKED before CLAIMED, then
// priority descending, then ID ascending.
// ---------------------------------------------------------------------------

typedef enum {
    Frontier_State_READY,
    Frontier_State_BLOCKED,
    Frontier_State_CLAIMED
} Frontier_State;

static const char *FRONTIER_STATE_NAMES[] = {
    [Frontier_State_READY] = "READY",
    [Frontier_State_BLOCKED] = "BLOCKED",
    [Frontier_State_CLAIMED] = "CLAIMED"
};

typedef struct {
    const Graph_Node *node;
    Frontier_State state;
} Frontier_Row;

static int frontier_compare(const void *a, const void *b) {
    const Frontier_Row *ra = (const Frontier_Row *)a;
    const Frontier_Row *rb = (const Frontier_Row *)b;
    if (ra->state != rb->state) {
        return ra->state < rb->state ? -1 : 1;
    }
    if (ra->node->priority != rb->node->priority) {
        return ra->node->priority > rb->node->priority ? -1 : 1; // higher first
    }
    return aids_string_slice_compare(&ra->node->huid, &rb->node->huid);
}

static int main_frontier(const Tatr_Context *ctx) {
    int result = 0;
    Argparse_Parser parser = {0};
    Aids_String_Slice tasks_dir = {0};
    Aids_String_Slice task_file_path = {0};
    Task_Graph graph = {0};
    boolean graph_loaded = false;
    Aids_Array rows = {0};
    boolean rows_initialized = false;

    argparse_parser_init(&parser, "tatr frontier", "Show the open work under an Epic", TATR_VERSION);

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'I',
        .long_name = "id",
        .description = "Epic task ID (format YYYYMMDD-HHMMSS)",
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

    task_graph_init(&graph);
    graph_loaded = true;
    if (task_graph_load(&tasks_dir, &graph) != AIDS_OK) {
        return_defer(1);
    }

    Graph_Node *epic = task_graph_find(&graph, id);
    if (epic == NULL || !epic->parsed) {
        aids_log(AIDS_ERROR, "Task " SS_Fmt " could not be read", SS_Arg(id));
        return_defer(1);
    }
    if (epic->kind != Task_Kind_EPIC) {
        aids_log(AIDS_ERROR, "Task " SS_Fmt " is KIND: " SS_Fmt ", not EPIC: a frontier is the open work under a container",
                 SS_Arg(id), SS_Arg(Task_Kind_Strings[epic->kind]));
        return_defer(1);
    }

    aids_array_init(&rows, sizeof(Frontier_Row));
    rows_initialized = true;

    for (size_t i = 0; i < graph.nodes.count; ++i) {
        Graph_Node *node = NULL;
        if (aids_array_get(&graph.nodes, i, (void **)&node) != AIDS_OK || !node->parsed) {
            continue;
        }
        if (node->parent.len == 0 || aids_string_slice_compare(&node->parent, &id) != 0) {
            continue;
        }
        if (node->status == Task_Status_CLOSED) {
            continue; // the frontier is OPEN work
        }

        Frontier_State state = Frontier_State_READY;
        for (size_t d = 0; d < node->depends_on.count; ++d) {
            Aids_String_Slice *dep = NULL;
            if (aids_array_get(&node->depends_on, d, (void **)&dep) != AIDS_OK) {
                continue;
            }
            Graph_Node *dep_node = task_graph_find(&graph, *dep);
            // An unresolvable or unreadable dependency blocks: it cannot be
            // shown to be CLOSED, and check reports the dangling edge itself.
            if (dep_node == NULL || !dep_node->parsed || dep_node->status != Task_Status_CLOSED) {
                state = Frontier_State_BLOCKED;
                break;
            }
        }
        // A claim outranks readiness in the display: work someone already holds
        // is not work to pick up, whether or not its blockers cleared.
        if (task_is_claimed(&tasks_dir, node->huid)) {
            state = Frontier_State_CLAIMED;
        }

        Frontier_Row row = { .node = node, .state = state };
        if (aids_array_append(&rows, &row) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to append frontier row: %s", aids_failure_reason());
            return_defer(1);
        }
    }

    aids_array_sort(&rows, frontier_compare);

    for (size_t i = 0; i < rows.count; ++i) {
        Frontier_Row *row = NULL;
        if (aids_array_get(&rows, i, (void **)&row) != AIDS_OK) {
            continue;
        }
        printf("%s\t" SS_Fmt "\tp%u\t" SS_Fmt "\t" SS_Fmt,
               FRONTIER_STATE_NAMES[row->state], SS_Arg(row->node->huid),
               row->node->priority, SS_Arg(Flow_Step_Strings[row->node->flow_step]),
               SS_Arg(row->node->title));
        if (row->state == Frontier_State_CLAIMED) {
            // Naming the holder is what makes a CLAIMED row actionable: your
            // own claim is work you already picked up, someone else's is not
            // yours to take.
            Claim holder = {0};
            if (claim_read(&tasks_dir, row->node->huid, &holder)) {
                Aids_String_Slice who = holder.session.len > 0
                    ? holder.session
                    : aids_string_slice_from_cstr("(unattributed)");
                printf("\tclaimed-by=" SS_Fmt, SS_Arg(who));
            }
            claim_free(&holder);
        }
        if (row->state == Frontier_State_BLOCKED) {
            // The blockers are the actionable part of a BLOCKED row: they name
            // what to finish first.
            printf("\tblocked-by=");
            boolean first = true;
            for (size_t d = 0; d < row->node->depends_on.count; ++d) {
                Aids_String_Slice *dep = NULL;
                if (aids_array_get((Aids_Array *)&row->node->depends_on, d, (void **)&dep) != AIDS_OK) {
                    continue;
                }
                Graph_Node *dep_node = task_graph_find(&graph, *dep);
                if (dep_node != NULL && dep_node->parsed && dep_node->status == Task_Status_CLOSED) {
                    continue;
                }
                printf("%s" SS_Fmt, first ? "" : ",", SS_Arg(*dep));
                first = false;
            }
        }
        printf("\n");
    }

defer:
    if (rows_initialized) {
        aids_array_free(&rows);
    }
    if (graph_loaded) {
        task_graph_free(&graph);
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
// context: the minimal artifact path set a flow phase needs. Paths only, never
// contents - a caller reads what it decides to read, and a phase that owns a
// record it has not written yet still needs the path to create it.
// ---------------------------------------------------------------------------

typedef enum {
    Phase_UNDERSTAND,
    Phase_PLAN,
    Phase_WORK,
    Phase_REVIEW,
    Phase_COMPOUND,
    Phase_RESUME,
    Phase_LANDING
} Phase;

static Aids_String_Slice Phase_Strings[] = {
    [Phase_UNDERSTAND] = (Aids_String_Slice) { .str = (unsigned char *)"understand", .len = 10 },
    [Phase_PLAN] = (Aids_String_Slice) { .str = (unsigned char *)"plan", .len = 4 },
    [Phase_WORK] = (Aids_String_Slice) { .str = (unsigned char *)"work", .len = 4 },
    [Phase_REVIEW] = (Aids_String_Slice) { .str = (unsigned char *)"review", .len = 6 },
    [Phase_COMPOUND] = (Aids_String_Slice) { .str = (unsigned char *)"compound", .len = 8 },
    [Phase_RESUME] = (Aids_String_Slice) { .str = (unsigned char *)"resume", .len = 6 },
    [Phase_LANDING] = (Aids_String_Slice) { .str = (unsigned char *)"landing", .len = 7 }
};

#define PHASE_VALUES_CSTR "understand, plan, work, review, compound, resume or landing"

// The record kinds each phase owns, terminated by -1. Derived from what the
// flow skills actually read and write at each step: understanding reads the
// research and the decision, planning writes the decision, work reads the
// review it must answer, compound writes the retro, and landing needs the
// records that go into the commit. `resume` is the one phase that wants
// everything, because it has no idea where it is picking up from.
static const int *phase_record_kinds(Phase phase) {
    static const int understand[] = {Record_Kind_TASK, Record_Kind_SPIKE, Record_Kind_DECISION, -1};
    static const int plan[]       = {Record_Kind_TASK, Record_Kind_SPIKE, Record_Kind_DECISION, -1};
    static const int work[]       = {Record_Kind_TASK, Record_Kind_DECISION, Record_Kind_REVIEW, -1};
    static const int review[]     = {Record_Kind_TASK, Record_Kind_REVIEW, -1};
    static const int compound[]   = {Record_Kind_TASK, Record_Kind_REVIEW, Record_Kind_RETRO, -1};
    static const int resume[]     = {Record_Kind_TASK, Record_Kind_SPIKE, Record_Kind_DECISION,
                                     Record_Kind_REVIEW, Record_Kind_RETRO, -1};
    static const int landing[]    = {Record_Kind_TASK, Record_Kind_REVIEW, Record_Kind_RETRO, -1};
    switch (phase) {
    case Phase_UNDERSTAND: return understand;
    case Phase_PLAN:       return plan;
    case Phase_WORK:       return work;
    case Phase_REVIEW:     return review;
    case Phase_COMPOUND:   return compound;
    case Phase_RESUME:     return resume;
    case Phase_LANDING:    return landing;
    }
    return resume; // unreachable: the enum is closed
}

// The understand phase is the one that needs to look OUTWARD: a Story cannot be
// understood without the Epic that gave it its shape.
static boolean phase_includes_parent(Phase phase) {
    return phase == Phase_UNDERSTAND || phase == Phase_PLAN || phase == Phase_RESUME;
}

static int main_context(const Tatr_Context *ctx) {
    int result = 0;
    Argparse_Parser parser = {0};
    Aids_String_Slice tasks_dir = {0};
    Aids_String_Slice task_file_path = {0};
    Task task = {0};
    boolean task_initialized = false;

    argparse_parser_init(&parser, "tatr context",
                         "Show the artifact paths a flow phase needs", TATR_VERSION);

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'I',
        .long_name = "id",
        .description = "Task ID (format YYYYMMDD-HHMMSS)",
        .type = ARGUMENT_TYPE_POSITIONAL,
        .required = 1
    });

    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'P',
        .long_name = "phase",
        .description = "Flow phase (" PHASE_VALUES_CSTR "; default: resume)",
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

    Phase phase = Phase_RESUME;
    char *phase_str = argparse_get_value(&parser, "phase");
    if (phase_str != NULL) {
        Aids_String_Slice phase_slice = aids_string_slice_from_cstr(phase_str);
        int value = 0;
        if (!enum_from_string(&phase_slice, Phase_Strings, ENUM_COUNT(Phase_Strings), &value)) {
            aids_log(AIDS_ERROR, "Invalid phase '%s': expected " PHASE_VALUES_CSTR, phase_str);
            return_defer(1);
        }
        phase = (Phase)value;
    }

    if (task_resolve(&ctx->cwd, &id, &tasks_dir, &task_file_path) != AIDS_OK) {
        return_defer(1);
    }

    task_init_empty(&task);
    task_initialized = true;
    if (task_load(&task_file_path, &task) != AIDS_OK) {
        return_defer(1);
    }

    const int *kinds = phase_record_kinds(phase);
    for (size_t i = 0; kinds[i] >= 0; ++i) {
        const Record_Schema *schema = &RECORD_SCHEMAS[kinds[i]];
        printf(SS_Fmt "/" SS_Fmt "/%s\t%s\n", SS_Arg(tasks_dir), SS_Arg(id), schema->file_name,
               task_sibling_exists(&tasks_dir, &id, schema->file_name) ? "present" : "missing");
    }

    if (phase_includes_parent(phase) && task.meta.parent.len > 0) {
        printf(SS_Fmt "/" SS_Fmt "/%s\t%s\n", SS_Arg(tasks_dir), SS_Arg(task.meta.parent),
               TASK_FILE_NAME_CSTR,
               task_sibling_exists(&tasks_dir, &task.meta.parent, TASK_FILE_NAME_CSTR)
                   ? "present" : "missing");
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
    fprintf(stderr, "  scaffold     Create a missing sibling record from the schema\n");
    fprintf(stderr, "  proofs       List the Definition of Done proofs of a task\n");
    fprintf(stderr, "  frontier     Show the open work under an Epic\n");
    fprintf(stderr, "  context      Show the artifact paths a flow phase needs\n");
    fprintf(stderr, "  claim        Claim a task for this session\n");
    fprintf(stderr, "  release      Release a claim on a task\n");
    fprintf(stderr, "  claims       List the claims in this tasks directory\n");
    fprintf(stderr, "  check        Lint task artifacts for process drift\n");
    fprintf(stderr, "  ledger       List pending lesson promotions and record a decision\n");
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
    } else if (strcmp(subcommand, "scaffold") == 0) {
        result = main_scaffold(&ctx);
        return_defer(result);
    } else if (strcmp(subcommand, "proofs") == 0) {
        result = main_proofs(&ctx);
        return_defer(result);
    } else if (strcmp(subcommand, "frontier") == 0) {
        result = main_frontier(&ctx);
        return_defer(result);
    } else if (strcmp(subcommand, "context") == 0) {
        result = main_context(&ctx);
        return_defer(result);
    } else if (strcmp(subcommand, "claim") == 0) {
        result = main_claim(&ctx);
        return_defer(result);
    } else if (strcmp(subcommand, "release") == 0) {
        result = main_release(&ctx);
        return_defer(result);
    } else if (strcmp(subcommand, "claims") == 0) {
        result = main_claims(&ctx);
        return_defer(result);
    } else if (strcmp(subcommand, "check") == 0) {
        result = main_check(&ctx);
        return_defer(result);
    } else if (strcmp(subcommand, "ledger") == 0) {
        result = main_ledger(&ctx);
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
