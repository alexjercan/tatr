#include "aids.h"
#include "argparse.h"
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

#define TATR_VERSION "0.1.0"

#define HUID_FORMAT_CSTR "%Y%m%d-%H%M%S"
#define HUID_LENGTH 16 // "20240630-235959" + null terminator

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

static void task_cleanup(Task *task) {
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
    if (!aids_string_slice_starts_with(&task->title, (Aids_String_Slice) { .str = (unsigned char *)"# ", .len = 2 })) {
        aids_log(AIDS_ERROR, "task_deserialize: Title does not start with expected prefix '# '");
        return_defer(AIDS_ERR);
    }
    aids_string_slice_skip(&task->title, 2);
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

    // Check format YYYYMMDD-HHMMSS
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

static Aids_Result task_new(Aids_String_Slice huid, Task task) {
    Aids_Result result = AIDS_OK;
    Aids_String_Builder tasks_dir_sb = {0};
    Aids_String_Builder task_file_sb = {0};
    Aids_String_Slice serialized_task = {0};
    Aids_String_Slice tasks_dir = {0};
    Aids_String_Slice task_file_path = {0};
    Aids_String_Slice cwd = {0};

    // Validate input
    if (huid.str == NULL || huid.len == 0) {
        aids_log(AIDS_ERROR, "task_new: Invalid huid provided");
        return_defer(AIDS_ERR);
    }

    // Get current working directory
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
    if (cwd.str != NULL) {
        AIDS_FREE(cwd.str);
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
    int result = 0;
    Argparse_Parser parser = {0};
    Task task = {0};
    boolean task_initialized = false;

    argparse_parser_init(&parser, "tatr new", "Create a new task", TATR_VERSION);

    // Add positional argument for title
    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'T',
        .long_name = "title",
        .description = "Task title",
        .type = ARGUMENT_TYPE_POSITIONAL,
        .required = 0
    });

    // Add priority option
    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 'p',
        .long_name = "priority",
        .description = "Task priority (default: 0)",
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    // Add tags option
    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 't',
        .long_name = "tags",
        .description = "Task tags (comma-separated)",
        .type = ARGUMENT_TYPE_VALUE_ARRAY,
        .required = 0
    });

    // Add status option
    argparse_add_argument(&parser, (Argparse_Options){
        .short_name = 's',
        .long_name = "status",
        .description = "Task status (OPEN, IN_PROGRESS, CLOSED)",
        .type = ARGUMENT_TYPE_VALUE,
        .required = 0
    });

    // Parse arguments
    if (argparse_parse(&parser, argc, argv) != ARG_OK) {
        return_defer(1);
    }

    // Initialize task
    task_init_empty(&task);
    task_initialized = true;

    // Get title
    char *title = argparse_get_value(&parser, "title");
    if (title != NULL) {
        task.title = aids_string_slice_from_cstr(title);
    }

    // Get priority
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

    // Get tags
    char *tags[ARGPARSE_CAPACITY];
    unsigned long tag_count = argparse_get_values(&parser, "tags", tags);
    for (unsigned long i = 0; i < tag_count; ++i) {
        Aids_String_Slice tag = aids_string_slice_from_cstr(tags[i]);
        if (aids_array_append(&task.meta.tags, &tag) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to append tag: %s", aids_failure_reason());
            return_defer(1);
        }
    }

    // Get status
    char *status_str = argparse_get_value(&parser, "status");
    if (status_str != NULL) {
        Aids_String_Slice status_slice = aids_string_slice_from_cstr(status_str);
        task.meta.status = task_status_from_string(&status_slice);
    }

    // Generate HUID
    char huid_str[HUID_LENGTH] = {0};
    if (huid(huid_str) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to generate huid");
        return_defer(1);
    }
    Aids_String_Slice id = aids_string_slice_from_cstr(huid_str);

    // Create task (this will use and restore temp allocator internally)
    if (task_new(id, task) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to create new task: %s", aids_failure_reason());
        return_defer(1);
    }

    printf("Task created successfully with ID: " SS_Fmt "\n", SS_Arg(id));

defer:
    if (task_initialized) {
        task_cleanup(&task);
    }
    argparse_parser_free(&parser);
    return result;
}

static int main_ls(int argc, char **argv) {
    int result = 0;
    Aids_String_Builder tasks_dir_sb = {0};
    Aids_String_Builder task_dir_sb = {0};
    Aids_String_Builder task_file_sb = {0};
    Aids_String_Slice tasks_dir = {0};
    Aids_String_Slice cwd = {0};
    Aids_Array tasks_files = {0}; /* Aids_String_Slice */
    Aids_Array task_huids = {0}; /* Aids_String_Slice */
    Aids_Array tasks = {0}; /* Task */
    Argparse_Parser parser = {0};

    argparse_parser_init(&parser, "tatr ls", "List tasks", TATR_VERSION);

    if (argparse_parse(&parser, argc, argv) != ARG_OK) {
        return_defer(1);
    }

    // Get current working directory
    if (aids_io_getcwd(&cwd) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_new: Failed to get current working directory: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }

    // Build task directory path: cwd/tasks
    aids_string_builder_init(&tasks_dir_sb);
    if (aids_string_builder_append(&tasks_dir_sb, SS_Fmt "/" SS_Fmt, SS_Arg(cwd), SS_Arg(TASKS_PATH)) != AIDS_OK) {
        aids_log(AIDS_ERROR, "task_new: Failed to build task directory path: %s", aids_failure_reason());
        return_defer(AIDS_ERR);
    }
    aids_string_builder_to_slice(&tasks_dir_sb, &tasks_dir);

    // List task directories
    aids_array_init(&tasks_files, sizeof(Aids_String_Slice));
    if (aids_io_listdir(&tasks_dir, &tasks_files) != AIDS_OK) {
        aids_log(AIDS_ERROR, "Failed to list tasks directory '" SS_Fmt "': %s", SS_Arg(tasks_dir), aids_failure_reason());
        return_defer(AIDS_ERR);
    }

    // For each task directory, read the TASK.md file and deserialize the task
    aids_string_builder_init(&task_dir_sb);
    aids_string_builder_init(&task_file_sb);
    aids_array_init(&tasks, sizeof(Task));
    aids_array_init(&task_huids, sizeof(Aids_String_Slice));
    for (size_t i = 0; i < tasks_files.count; ++i) {
        Aids_String_Slice *huid = NULL;
        AIDS_ASSERT(aids_array_get(&tasks_files, i, (void **)&huid) == AIDS_OK, "Failed to get huid at index %zu: %s", i, aids_failure_reason());

        // Build task directory path: cwd/tasks/huid
        aids_string_builder_clear(&task_dir_sb);
        if (aids_string_builder_append(&task_dir_sb, SS_Fmt "/" SS_Fmt "/" SS_Fmt, SS_Arg(cwd), SS_Arg(TASKS_PATH), SS_Arg(*huid)) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to build task directory path for huid '" SS_Fmt "': %s", SS_Arg(*huid), aids_failure_reason());
            return_defer(AIDS_ERR);
        }
        Aids_String_Slice task_dir_path = {0};
        aids_string_builder_to_slice(&task_dir_sb, &task_dir_path);

        // Check that it's a directory and has a valid huid format
        if (!ishuid(huid) || !aids_io_isdir(&task_dir_path)) {
            continue; // Skip non-task directories
        }

        // Build task file path: cwd/tasks/huid/TASK.md
        aids_string_builder_clear(&task_file_sb);
        if (aids_string_builder_append(&task_file_sb, SS_Fmt "/" SS_Fmt, SS_Arg(task_dir_path), SS_Arg(TASK_FILE_NAME)) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to build task file path for huid '" SS_Fmt "': %s", SS_Arg(*huid), aids_failure_reason());
            return_defer(AIDS_ERR);
        }
        Aids_String_Slice task_file_path = {0};
        aids_string_builder_to_slice(&task_file_sb, &task_file_path);

        // Read task file
        Aids_String_Slice serialized_task = {0};
        if (aids_io_read(&task_file_path, &serialized_task, "r") != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to read task file '" SS_Fmt "': %s", SS_Arg(task_file_path), aids_failure_reason());
            return_defer(AIDS_ERR);
        }

        // Deserialize task
        Task task = {0};
        if (task_deserialize(serialized_task, &task) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to deserialize task from file '" SS_Fmt ": %s", SS_Arg(task_file_path), aids_failure_reason());
            return_defer(AIDS_ERR);
        }

        // Append task to array
        if (aids_array_append(&tasks, &task) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to append task to array: %s", aids_failure_reason());
            return_defer(AIDS_ERR);
        }

        // Append huid to array
        if (aids_array_append(&task_huids, huid) != AIDS_OK) {
            aids_log(AIDS_ERROR, "Failed to append huid to array: %s", aids_failure_reason());
            return_defer(AIDS_ERR);
        }
    }

    // ./tasks/20260330-202358/TASK.md: [PRIORITY: 100, TAGS: feature] Implement ls subcommand
    for (size_t i = 0; i < tasks.count; ++i) {
        Task *task = NULL;
        AIDS_ASSERT(aids_array_get(&tasks, i, (void **)&task) == AIDS_OK, "Failed to get task at index %zu: %s", i, aids_failure_reason());
        Aids_String_Slice *huid = NULL;
        AIDS_ASSERT(aids_array_get(&task_huids, i, (void **)&huid) == AIDS_OK, "Failed to get huid at index %zu: %s", i, aids_failure_reason());

        printf(SS_Fmt "/" SS_Fmt "/" SS_Fmt "/%s: [PRIORITY: %u, TAGS: ", SS_Arg(cwd), SS_Arg(TASKS_PATH), SS_Arg(*huid), TASK_FILE_NAME_CSTR, task->meta.priority);
        for (size_t j = 0; j < task->meta.tags.count; ++j) {
            Aids_String_Slice *tag = NULL;
            AIDS_ASSERT(aids_array_get(&task->meta.tags, j, (void **)&tag) == AIDS_OK, "Failed to get tag at index %zu for task %zu: %s", j, i, aids_failure_reason());
            printf(SS_Fmt, SS_Arg(*tag));
            if (j < task->meta.tags.count - 1) {
                printf(", ");
            }
        }
        printf("] " SS_Fmt "\n", SS_Arg(task->title));
    }

defer:
    if (cwd.str != NULL) {
        AIDS_FREE(cwd.str);
    }
    if (tasks_dir.str != NULL) {
        AIDS_FREE(tasks_dir.str);
    }
    aids_string_builder_free(&task_file_sb);
    aids_string_builder_free(&task_dir_sb);
    for (size_t i = 0; i < tasks.count; ++i) {
        Task *task = NULL;
        if (aids_array_get(&tasks, i, (void **)&task) == AIDS_OK) {
            task_cleanup(task);
        }
    }
    aids_array_free(&tasks);
    aids_array_free(&tasks_files);
    return result;
}

static void print_usage(const char *program) {
    fprintf(stderr, "Usage: %s <subcommand> [options]\n", program);
    fprintf(stderr, "\n");
    fprintf(stderr, "Subcommands:\n");
    fprintf(stderr, "  help       Show this help message\n");
    fprintf(stderr, "  version    Show version information\n");
    fprintf(stderr, "  new        Create a new task\n");
    fprintf(stderr, "  ls         List tasks\n");
    fprintf(stderr, "\n");
}

int main(int argc, char **argv) {
    // TODO(20260329-123700): Implement Task Tracker

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *subcommand = argv[1];

    if (strcmp(subcommand, "help") == 0) {
        print_usage(argv[0]);
        return 0;
    } else if (strcmp(subcommand, "version") == 0) {
        printf("%s version %s\n", argv[0], TATR_VERSION);
        return 0;
    } else if (strcmp(subcommand, "new") == 0) {
        return main_new(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "ls") == 0) {
        return main_ls(argc - 1, argv + 1);
    } else {
        fprintf(stderr, "Unknown subcommand: %s\n", subcommand);
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}

#define ARGPARSE_IMPLEMENTATION
#include "argparse.h"
#define AIDS_IMPLEMENTATION
#include "aids.h"
