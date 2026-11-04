#include "scheduler.h"
#include "tasks.h"
#include "logger.h"
#include "memory.h"
#include "session.h"

bool queue_push(ReadyQueue *queue, int pid) {
    size_t i;
    if (queue->count == MAX_TASKS) return false;
    for (i = 0; i < queue->count; ++i)
        if (queue->ids[(queue->head + i) % MAX_TASKS] == pid) return false;
    queue->ids[(queue->head + queue->count) % MAX_TASKS] = pid;
    ++queue->count;
    return true;
}

bool queue_pop(ReadyQueue *queue, int *pid) {
    if (queue->count == 0) return false;
    *pid = queue->ids[queue->head];
    queue->head = (queue->head + 1) % MAX_TASKS;
    --queue->count;
    return true;
}

bool queue_remove(ReadyQueue *queue, int pid) {
    size_t i, j;
    for (i = 0; i < queue->count; ++i) {
        if (queue->ids[(queue->head + i) % MAX_TASKS] != pid) continue;
        for (j = i; j + 1 < queue->count; ++j)
            queue->ids[(queue->head + j) % MAX_TASKS] =
                queue->ids[(queue->head + j + 1) % MAX_TASKS];
        --queue->count;
        return true;
    }
    return false;
}

bool scheduler_step(App *app, FILE *out) {
    int pid, slice;
    size_t i;
    long long start;
    Task *task;
    char detail[160];
    if (!queue_pop(&app->queue, &pid)) {
        fprintf(out, "IDLE: no READY tasks; virtual clock remains %lld.\n", app->now);
        logger_event(app, "IDLE", 0, "no READY tasks");
        return false;
    }
    task = task_find(app, pid);
    if (!task || task->state != READY) {
        snprintf(app->error, ERROR_SIZE, "Invalid scheduler queue entry.");
        return false;
    }
    start = app->now;
    task->state = RUNNING;
    logger_event(app, "DISPATCH", pid, "READY->RUNNING");
    if (task->first_run < 0) task->first_run = start;
    slice = task->remaining < app->quantum ? task->remaining : app->quantum;
    /* WAITING tasks receive no ready-wait ticks. The running task is not queued. */
    for (i = 0; i < app->task_count; ++i)
        if (app->tasks[i].state == READY) app->tasks[i].ready_wait += slice;
    app->now += slice;
    task->remaining -= slice;
    if (task->remaining == 0) {
        task->state = FINISHED;
        task->completion = app->now;
        memory_release(&app->memory, pid);
        task->payload = NULL;
    } else {
        task->state = READY;
        /* The dequeue created a free slot and pid is absent, so push cannot fail. */
        (void)queue_push(&app->queue, pid);
    }
    fprintf(out, "[%lld,%lld) P%d %-12.12s RUNNING -> %-8s remaining=%d\n",
            start, app->now, pid, task->name, state_name(task->state), task->remaining);
    snprintf(detail, sizeof detail, "start=%lld end=%lld remaining=%d state=%s used_bytes=%zu",
             start, app->now, task->remaining, state_name(task->state), app->memory.used);
    logger_event(app, "SLICE", pid, detail);
    if (app->trace &&
        (fprintf(app->trace, "%d,%d,%lld,%lld,%d,%s\n", app->experiment, pid, start,
                 app->now, task->remaining, state_name(task->state)) < 0 ||
         fflush(app->trace) != 0)) {
        ++app->logger.failures;
        fprintf(stderr, "WARNING: scheduling trace write failed.\n");
    }
    session_complete(app, task, out);
    return true;
}

void scheduler_run(App *app, FILE *out) {
    if (app->queue.count == 0) {
        (void)scheduler_step(app, out);
        return;
    }
    while (app->queue.count > 0)
        if (!scheduler_step(app, out)) break;
    fprintf(out, "Run paused: ready queue empty; WAITING tasks need wake.\n");
}

void metrics_print(const App *app, FILE *out, bool csv) {
    size_t i, completed = 0;
    double turnaround_sum = 0, waiting_sum = 0, response_sum = 0;
    fputs(csv ? "pid,name,arrival,burst,first_run,completion,turnaround,waiting,response\n" :
          "PID NAME             ARR BURST FIRST DONE TURN WAIT RESP\n", out);
    for (i = 0; i < app->task_count; ++i) {
        const Task *t = &app->tasks[i];
        long long turnaround, response;
        if (t->state != FINISHED) continue;
        turnaround = t->completion - t->arrival;
        response = t->first_run - t->arrival;
        if (csv)
            fprintf(out, "%d,%s,%lld,%d,%lld,%lld,%lld,%lld,%lld\n", t->pid, t->name,
                    t->arrival, t->burst, t->first_run, t->completion, turnaround, t->ready_wait, response);
        else
            fprintf(out, "%-3d %-16.16s %-3lld %-5d %-5lld %-4lld %-4lld %-4lld %lld\n",
                    t->pid, t->name, t->arrival, t->burst, t->first_run, t->completion,
                    turnaround, t->ready_wait, response);
        ++completed;
        turnaround_sum += (double)turnaround;
        waiting_sum += (double)t->ready_wait;
        response_sum += (double)response;
    }
    if (!csv) {
        if (completed)
            fprintf(out, "Means (%zu finished): turnaround=%.2f waiting=%.2f response=%.2f ticks\n",
                    completed, turnaround_sum / completed, waiting_sum / completed, response_sum / completed);
        else fputs("No finished tasks; means are unavailable.\n", out);
    }
}

bool metrics_export(App *app) {
    char path[PATH_SIZE + 20];
    FILE *stream;
    bool okay;
    snprintf(path, sizeof path, "%s/metrics.csv", app->data_dir);
    stream = fopen(path, "w");
    if (!stream) {
        snprintf(app->error, ERROR_SIZE, "Cannot open metrics.csv.");
        return false;
    }
    metrics_print(app, stream, true);
    okay = !ferror(stream);
    if (fclose(stream) != 0) okay = false;
    if (!okay) {
        snprintf(app->error, ERROR_SIZE, "Metrics write/close failed.");
        return false;
    }
    logger_event(app, "METRICS", 0, "metrics.csv exported; finished tasks only");
    return true;
}
