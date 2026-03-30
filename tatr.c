#include "aids.h"
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

#define TASKS_PATH_CSTR "tasks"
#define TASK_FILE_NAME_CSTR "TASK.md"

static Aids_String_Slice TASKS_PATH = (Aids_String_Slice) { .str = (unsigned char *)TASKS_PATH_CSTR, .len = sizeof(TASKS_PATH_CSTR) - 1 };
static Aids_String_Slice TASK_FILE_NAME = (Aids_String_Slice) { .str = (unsigned char *)TASK_FILE_NAME_CSTR, .len = sizeof(TASK_FILE_NAME_CSTR) - 1 };

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

static Task_Status task_status_from_string(const Aids_String_Slice *slice) {
    for (size_t i = 0; i < sizeof(Task_Status_Strings) / sizeof(Task_Status_Strings[0]); ++i) {
        if (aids_string_slice_compare(slice, &Task_Status_Strings[i]) == 0) {
            return (Task_Status)i;
        }
    }

    return Task_Status_OPEN; // Default to OPEN if not found
}

typedef struct {
    Task_Status status;
    unsigned int priority;
    Aids_Array tags; /* Aids_String_Slice */
} Task_Meta;

Aids_String_Slice STATUS_FORMAT = (Aids_String_Slice) { .str = (unsigned char *)"- STATUS: ", .len = 10 };
Aids_String_Slice PRIORITY_FORMAT = (Aids_String_Slice) { .str = (unsigned char *)"- PRIORITY: ", .len = 12 };
Aids_String_Slice TAGS_FORMAT = (Aids_String_Slice) { .str = (unsigned char *)"- TAGS: ", .len = 8 };

typedef struct {
    Aids_String_Slice title;
    Aids_String_Slice description;
    Task_Meta meta;
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
    aids_array_init(&task->meta.tags, sizeof(Aids_String_Slice));
}

static void task_free(Task *task) {
    if (task == NULL) {
        return;
    }

    aids_array_free(&task->meta.tags);
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

    // Metadata
    // - STATUS: OPEN | IN_PROGRESS | CLOSED
    // - PRIORITY: 100
    // - TAGS: tag1, tag2, tag3
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
    if (aids_string_builder_append(&builder, SS_Fmt, SS_Arg(TAGS_FORMAT)) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_serialize: Failed to append tags format: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }
    for (size_t i = 0; i < task.meta.tags.count; ++i) {
        Aids_String_Slice *tag = NULL;
        if (aids_array_get(&task.meta.tags, i, (void **)&tag) != AIDS_OK) {
            aids_log(AIDS_ERROR, "task_serialize: Failed to get tag at index %zu: %s", i, aids_failure_reason());
            return_defer(AIDS_ERR);
        }

        if (aids_string_builder_append_slice(&builder, *tag) != AIDS_OK) {
            aids_log(AIDS_ERROR, "task_serialize: Failed to append tag at index %zu: %s", i, aids_failure_reason());
            return_defer(AIDS_ERR);
        }
        if (i < task.meta.tags.count - 1) {
            if (aids_string_builder_append(&builder, ", ") != AIDS_OK) {
                aids_log(AIDS_ERROR, "task_serialize: Failed to append tag separator: %s", aids_failure_reason());
                return_defer(AIDS_ERR);
            }
        }
    }
    if (aids_string_builder_append(&builder, "\n\n") != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_serialize: Failed to append tags suffix: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }

    // Description
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

    // Initialize task
    task_init_empty(task);

    // # Title
    if (!aids_string_slice_tokenize(&buffer, '\n', &task->title)) {
        aids_log(AIDS_ERROR, "task_deserialize: Failed to parse title from buffer");
        return_defer(AIDS_ERR);
    }
    aids_string_slice_skip_while(&buffer, isspace);

    // Metadata
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
    task->meta.status = task_status_from_string(&status_slice);
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

    while (tags_slice.len > 0) {
        Aids_String_Slice tag;
        if (!aids_string_slice_tokenize(&tags_slice, ',', &tag)) {
            tag = tags_slice;
            tags_slice.len = 0;
        }
        aids_string_slice_trim(&tag);
        if (tag.len > 0) {
            if (aids_array_append(&task->meta.tags, &tag) != AIDS_OK) {
                aids_log(AIDS_ERROR, "task_deserialize: Failed to append tag to array: %s", aids_failure_reason());
                return_defer(AIDS_ERR);
            }
        }
    }
    aids_string_slice_skip_while(&buffer, isspace);

    // The rest is description
    task->description = buffer;

defer:
    if (result != AIDS_OK) {
        task_free(task);
    }
    return result;
}

static Aids_String_Slice huid() {
    time_t current_time;
    struct tm *time_info;
    char time_buffer[20];

    time(&current_time);
    time_info = localtime(&current_time);

    if (time_info == NULL) {
        aids_log(AIDS_ERROR, "huid: Failed to get local time");
        return (Aids_String_Slice) {0};
    }

    if (strftime(time_buffer, sizeof(time_buffer), "%Y%m%d-%H%M%S", time_info) == 0) {
        aids_log(AIDS_ERROR, "huid: Failed to format time string");
        return (Aids_String_Slice) {0};
    }

    return aids_string_slice_from_cstr(time_buffer);
}

static Aids_Result task_new(Aids_String_Slice huid, Task task) {
    Aids_Result result = AIDS_OK;
    Aids_String_Builder tasks_dir_sb = {0};
    Aids_String_Builder task_file_sb = {0};
    Aids_String_Slice serialized_task = {0};
    Aids_String_Slice tasks_dir = {0};
    Aids_String_Slice task_file_path = {0};
    boolean temp_restored = false;

    // Validate input
    if (huid.str == NULL || huid.len == 0) {
        aids_log(AIDS_ERROR, "task_new: Invalid huid provided");
        return AIDS_ERR;
    }

    // Save temp allocator state to restore later (cwd uses temp allocator)
    size_t temp_checkpoint = aids_temp_save();

    // Get current working directory (allocates from temp allocator)
    Aids_String_Slice cwd = {0};
    if (aids_io_getcwd(&cwd) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_new: Failed to get current working directory: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }

    // Build task directory path: cwd/tasks/huid
    aids_string_builder_init(&tasks_dir_sb);
    if (aids_string_builder_append(&tasks_dir_sb, SS_Fmt "/" SS_Fmt "/" SS_Fmt, SS_Arg(cwd), SS_Arg(TASKS_PATH), SS_Arg(huid)) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_new: Failed to build task directory path: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }
    aids_string_builder_to_slice(&tasks_dir_sb, &tasks_dir);

    // Restore temp allocator (cwd no longer needed)
    aids_temp_load(temp_checkpoint);
    temp_restored = true;

    // Create task directory
    if (aids_io_mkdir(&tasks_dir, true) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_new: Failed to create task directory '%.*s': %s",
                 (int)tasks_dir.len, tasks_dir.str, aids_failure_reason());
        return_defer(AIDS_ERR);
    }

    // Serialize task
    if (task_serialize(task, &serialized_task) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_new: Failed to serialize task: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }

    // Build task file path: tasks_dir/TASK.md
    aids_string_builder_init(&task_file_sb);
    if (aids_string_builder_append(&task_file_sb, SS_Fmt "/" SS_Fmt, SS_Arg(tasks_dir), SS_Arg(TASK_FILE_NAME)) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_new: Failed to build task file path: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }
    aids_string_builder_to_slice(&task_file_sb, &task_file_path);

    // Write task to file
    if (aids_io_write(&task_file_path, &serialized_task, "w") != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_new: Failed to write task to file '%.*s': %s",
                 (int)task_file_path.len, task_file_path.str, aids_failure_reason());
        return_defer(AIDS_ERR);
    }

defer:
    if (!temp_restored) {
        aids_temp_load(temp_checkpoint);
    }
    if (tasks_dir.str != NULL) {
        AIDS_FREE(tasks_dir.str);
    }
    if (task_file_path.str != NULL) {
        AIDS_FREE(task_file_path.str);
    }
    if (serialized_task.str != NULL) {
        AIDS_FREE(serialized_task.str);
    }
    return result;
}

static int main_new(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Task task = {0};
    task_init_empty(&task);

    Aids_String_Slice id = huid();
    if (id.str == NULL || id.len == 0) {
        aids_log(AIDS_ERROR, "Failed to generate huid");
        return 1;
    }

    if (task_new(id, task) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to create new task: %s", aids_failure_reason());
        return 1;
    }

    return 0;
}

static void print_usage(const char *program) {
    fprintf(stderr, "Usage: %s <subcommand> [options]\n", program);
    fprintf(stderr, "\n");
    fprintf(stderr, "Subcommands:\n");
    fprintf(stderr, "  new        Create a new task\n");
    fprintf(stderr, "\n");
}

int main(int argc, char **argv) {
    // TODO(20260329-123700): Implement Task Tracker

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *subcommand = argv[1];

    if (strcmp(subcommand, "new") == 0) {
        return main_new(argc - 1, argv + 1);
    } else {
        fprintf(stderr, "Unknown subcommand: %s\n", subcommand);
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}

#define AIDS_IMPLEMENTATION
#include "aids.h"
