/**
 * @file core.c
 * @brief Shared data and application lifecycle.
 *
 * Defines the simulator model, validates names, opens output files, and
 * handles reset, consistency checks, and shutdown.
 */

#include "core.h"
#include "logger.h"
#include "memory.h"

#include <string.h>

/**
 * Accept the shared task/note name alphabet and length limit.
 * Rejecting separators also prevents note names from becoming filesystem paths.
 */
bool valid_name(const char *name) {
    size_t i, length = strlen(name);
    if (length == 0 || length >= NAME_SIZE)
        return false;
    for (i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return true;
}

/**
 * Convert an enum value to a label for tables, traces, and log messages.
 */
const char *state_name(TaskState state) {
    switch (state) {
        case READY:
            return "READY";
        case RUNNING:
            return "RUNNING";
        case WAITING:
            return "WAITING";
        case FINISHED:
            return "FINISHED";
    }
    return "INVALID";
}

/**
 * Initialize an empty experiment and open the event log and trace.
 * The caller must provide an existing data directory. On failure, close any
 * file already opened and explain the error in app->error.
 */
bool app_init(App *app, const char *data_dir, int quantum) {
    char path[PATH_SIZE];
    /* Zero initialization gives empty queues, counters, and NULL file pointers. */
    memset(app, 0, sizeof *app);
    if (quantum < 1 || quantum > MAX_TICKS || strlen(data_dir) > PATH_SIZE - 80) {
        snprintf(app->error, ERROR_SIZE, "Invalid quantum or data directory path.");
        return false;
    }
    strcpy(app->data_dir, data_dir);
    app->next_pid = 1;
    app->quantum = quantum;
    app->experiment = 1;
    /* Events append across runs; the slice trace starts fresh for this run. */
    snprintf(path, sizeof path, "%s/events.log", data_dir);
    if (!logger_open(&app->logger, path)) {
        snprintf(app->error, ERROR_SIZE, "Cannot open event log. Create a writable data directory first.");
        return false;
    }
    snprintf(path, sizeof path, "%s/trace.csv", data_dir);
    app->trace = fopen(path, "w");
    if (!app->trace) {
        snprintf(app->error, ERROR_SIZE, "Cannot open scheduling trace.");
        logger_close(&app->logger);
        return false;
    }
    if (fputs("experiment,pid,start,end,remaining,state\n", app->trace) == EOF ||
        fflush(app->trace) != 0) {
        snprintf(app->error, ERROR_SIZE, "Cannot write scheduling trace.");
        fclose(app->trace);
        app->trace = NULL;
        logger_close(&app->logger);
        return false;
    }
    logger_event(app, "START", 0, "virtual clock initialized");
    return true;
}

/**
 * Start a new experiment after freeing every outstanding payload.
 * Keep note files, the chosen quantum, and open output streams. Reset task
 * records, the ready queue, PID numbering, and virtual time.
 */
void app_reset(App *app) {
    memory_clear(&app->memory);
    memset(app->tasks, 0, sizeof app->tasks);
    app->task_count = 0;
    app->queue = (ReadyQueue){0};
    app->next_pid = 1;
    app->now = 0;
    ++app->experiment;
    logger_event(app, "RESET", 0, "all task payloads released; notes preserved");
}

/**
 * Release remaining payloads, log zero tracked allocations, and close files.
 * Use only after successful initialization. Task records are not reused after
 * shutdown; their pointers are no longer valid for further scheduling.
 */
void app_shutdown(App *app) {
    memory_clear(&app->memory);
    logger_event(app, "SHUTDOWN", 0, "used_bytes=0 allocations=0");
    if (app->trace && fclose(app->trace) != 0) {
        ++app->logger.failures;
        fprintf(stderr, "WARNING: trace close failed.\n");
    }
    app->trace = NULL;
    logger_close(&app->logger);
}

/**
 * Check consistency at a command boundary, when no task is RUNNING.
 * Cross-check READY queue membership and payload ownership against task records.
 * Return false if a checked relationship or accounting total is inconsistent.
 */
bool app_check(const App *app) {
    size_t i, j, active = 0;
    if (!memory_check(&app->memory) || app->task_count > MAX_TASKS ||
        app->queue.count > MAX_TASKS || app->queue.head >= MAX_TASKS) return false;
    /* Each retained record must agree with both the queue and registry. */
    for (i = 0; i < app->task_count; ++i) {
        const Task *t = &app->tasks[i];
        size_t queued = 0, owned = 0;
        if (t->state == RUNNING || t->remaining < 0 || t->remaining > t->burst)
            return false;
        for (j = 0; j < app->queue.count; ++j)
            if (app->queue.ids[(app->queue.head + j) % MAX_TASKS] == t->pid)
                ++queued;
        if (queued != (t->state == READY ? 1u : 0u))
            return false;
        if (t->state == READY)
            ++active;
        for (j = 0; j < MAX_TASKS; ++j) {
            const AllocationRecord *r = &app->memory.records[j];
            if (r->pointer && r->owner == t->pid) {
                if (r->pointer != t->payload || r->bytes != t->payload_bytes)
                    return false;
                ++owned;
            }
        }
        if (t->state == FINISHED) {
            if (t->payload || t->remaining != 0 || owned != 0)
                return false;
        } else if (!t->payload || owned != 1)
            return false;
    }
    /* No extra queue entries may refer to missing or non-READY tasks. */
    if (active != app->queue.count)
        return false;
    /* Every unfinished task owns exactly one live registered payload. */
    active = 0;
    for (i = 0; i < app->task_count; ++i)
        if (app->tasks[i].state != FINISHED)
            ++active;
    return active == app->memory.count;
}
