#include "tasks.h"
#include "logger.h"
#include "memory.h"
#include "scheduler.h"
#include <string.h>

Task *task_find(App *app, int pid) {
    size_t i;
    for (i = 0; i < app->task_count; ++i)
        if (app->tasks[i].pid == pid) return &app->tasks[i];
    return NULL;
}

bool task_create(App *app, const char *name, int burst, size_t bytes, bool session) {
    Task task = {0};
    char detail[160];
    if (!valid_name(name) || burst < 1 || burst > MAX_TICKS) {
        snprintf(app->error, ERROR_SIZE, "Name: 1..48 letters/digits/_/-. Burst: 1..1000000 ticks.");
        return false;
    }
    if (app->task_count == MAX_TASKS) {
        snprintf(app->error, ERROR_SIZE, "Task table full (20 records). Use reset for a new experiment.");
        return false;
    }
    task.pid = app->next_pid;
    task.payload = memory_allocate(&app->memory, task.pid, bytes, app->error);
    if (!task.payload) return false;
    /* Commit the task and PID only after allocation and queue insertion succeed. */
    if (!queue_push(&app->queue, task.pid)) {
        memory_release(&app->memory, task.pid);
        snprintf(app->error, ERROR_SIZE, "Ready queue full; creation rolled back.");
        return false;
    }
    strcpy(task.name, name);
    task.state = READY;
    task.burst = task.remaining = burst;
    task.arrival = app->now;
    task.first_run = task.completion = -1;
    task.payload_bytes = bytes;
    task.is_session = session;
    app->tasks[app->task_count++] = task;
    ++app->next_pid;
    snprintf(detail, sizeof detail, "state=READY burst=%d bytes=%zu used_bytes=%zu session=%d",
             burst, bytes, app->memory.used, session ? 1 : 0);
    logger_event(app, "CREATE", task.pid, detail);
    return true;
}

bool task_block(App *app, int pid) {
    Task *task = task_find(app, pid);
    if (!task || task->state != READY) {
        snprintf(app->error, ERROR_SIZE, "Only an existing READY task can be blocked.");
        return false;
    }
    if (!queue_remove(&app->queue, pid)) {
        snprintf(app->error, ERROR_SIZE, "Task missing from ready queue.");
        return false;
    }
    task->state = WAITING;
    logger_event(app, "BLOCK", pid, "READY->WAITING");
    return true;
}

bool task_wake(App *app, int pid) {
    Task *task = task_find(app, pid);
    if (!task || task->state != WAITING) {
        snprintf(app->error, ERROR_SIZE, "Only an existing WAITING task can be woken.");
        return false;
    }
    if (!queue_push(&app->queue, pid)) {
        snprintf(app->error, ERROR_SIZE, "Ready queue full.");
        return false;
    }
    task->state = READY;
    logger_event(app, "WAKE", pid, "WAITING->READY; appended to queue tail");
    return true;
}

void tasks_print(const App *app, FILE *out) {
    size_t i;
    fprintf(out, "Tasks | tick=%lld quantum=%d records=%zu/20\n", app->now, app->quantum, app->task_count);
    fprintf(out, "PID NAME                 STATE     BURST LEFT ARRIVAL BYTES\n");
    for (i = 0; i < app->task_count; ++i) {
        const Task *t = &app->tasks[i];
        fprintf(out, "%-3d %-20.20s %-9s %-5d %-4d %-7lld %zu\n", t->pid, t->name,
                state_name(t->state), t->burst, t->remaining, t->arrival, t->payload_bytes);
    }
    fprintf(out, "Ready queue:");
    for (i = 0; i < app->queue.count; ++i)
        fprintf(out, " P%d", app->queue.ids[(app->queue.head + i) % MAX_TASKS]);
    fprintf(out, "%s\n", app->queue.count ? "" : " (empty)");
}
