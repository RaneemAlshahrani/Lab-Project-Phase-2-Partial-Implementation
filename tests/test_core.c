/**
 * @file test_core.c
 * @brief Direct C assertions for scheduling, queue order, ownership, and cleanup.
 *
 * The Python integration suite checks CLI/file behavior. These tests inspect the
 * internal model directly and use a temporary output stream instead of user data.
 */

#include "core.h"
#include "memory.h"
#include "scheduler.h"
#include "session.h"
#include "tasks.h"
#include <assert.h>
#include <string.h>

/* Initialize a file-free App fixture for tests of the in-memory services. */
static void fresh(App *a) {
    memset(a, 0, sizeof *a);
    a->next_pid = 1;
    a->quantum = 2;
    a->experiment = 1;
    assert(app_check(a));
}

/* Check every expected dispatch time and the proposal's per-task metrics. */
static void reference(App *a, FILE *sink) {
    const int order[] = {1, 2, 3, 1, 2};
    const long long ends[] = {2, 4, 6, 8, 9};
    const long long first[] = {0, 2, 4}, done[] = {8, 9, 6}, wait[] = {4, 6, 4};
    size_t i;
    fresh(a);
    assert(session_start(a, "Study", 4, 256));
    assert(task_create(a, "Review", 3, 128, false));
    assert(task_create(a, "Practice", 2, 64, false));
    for (i = 0; i < 5; ++i) {
        assert(a->queue.ids[a->queue.head] == order[i]);
        assert(scheduler_step(a, sink));
        assert(a->now == ends[i]);
        assert(app_check(a));
    }
    for (i = 0; i < 3; ++i) {
        assert(a->tasks[i].first_run == first[i]);
        assert(a->tasks[i].completion == done[i]);
        assert(a->tasks[i].ready_wait == wait[i]);
    }
    assert(a->tasks[0].notified);
    assert(a->memory.used == 0 && a->memory.count == 0);
    assert(!scheduler_step(a, sink));
    assert(a->now == 9);
    app_shutdown(a);
}

/* Check that blocked time is excluded from READY waiting and later arrivals are recorded. */
static void blocking_and_arrival(App *a, FILE *sink) {
    Task *task;
    fresh(a);
    assert(task_create(a, "Blocked", 4, 64, false));
    assert(task_create(a, "Other", 2, 64, false));
    assert(task_block(a, 1));
    assert(!task_block(a, 1));
    assert(scheduler_step(a, sink));
    assert(a->now == 2 && a->tasks[0].ready_wait == 0);
    assert(task_create(a, "Late", 1, 64, false));
    assert(a->tasks[2].arrival == 2);
    assert(task_wake(a, 1));
    assert(!task_wake(a, 1));
    assert(!task_wake(a, 999));
    assert(a->queue.count == 2);
    assert(a->queue.ids[a->queue.head] == 3); /* wake goes behind Late */
    scheduler_run(a, sink);
    task = task_find(a, 1);
    assert(task->completion == 7 && task->ready_wait == 1 && task->first_run == 3);
    /* Turnaround-burst=3 includes blocked ticks; READY-only waiting is 1. */
    assert(a->tasks[2].first_run - a->tasks[2].arrival == 0);
    assert(!task_block(a, 1));
    assert(app_check(a));
    app_shutdown(a);
}

/* Check budget/capacity failures, PID rollback, reset, short bursts, and shutdown cleanup. */
static void limits_and_rollback(App *a, FILE *sink) {
    size_t i;
    char name[NAME_SIZE + 2];
    fresh(a);
    assert(task_create(a, "Full", 1, MEMORY_BUDGET, false));
    assert(!task_create(a, "Overflow", 1, 1, false));
    assert(a->task_count == 1 && a->next_pid == 2 && a->queue.count == 1);
    assert(a->memory.used == MEMORY_BUDGET);
    assert(scheduler_step(a, sink));
    assert(!memory_release(&a->memory, 1)); /* repeat release is harmless */
    assert(task_create(a, "Reused", 1, 1, false));
    a->memory.fail_next = true;
    assert(!task_create(a, "Failed", 2, 100, false));
    assert(a->memory.used == 1 && a->memory.count == 1 && a->next_pid == 3);
    assert(task_create(a, "AfterFailure", 2, 100, false));
    assert(task_find(a, 3) != NULL);
    assert(!task_create(a, "Bad", -1, 1, false));
    assert(!task_create(a, "Bad", 0, 1, false));
    assert(!task_create(a, "Bad", 1, 0, false));
    memset(name, 'A', sizeof name - 1); name[sizeof name - 1] = '\0';
    assert(!task_create(a, name, 1, 1, false));
    for (i = a->task_count; i < MAX_TASKS; ++i)
        assert(task_create(a, "Capacity", 1, 1, false));
    assert(!task_create(a, "TwentyFirst", 1, 1, false));
    assert(a->task_count == 20 && app_check(a));
    assert(task_block(a, 2));
    app_reset(a);
    assert(a->memory.used == 0 && a->memory.count == 0 && a->queue.count == 0);
    assert(a->task_count == 0 && a->now == 0 && app_check(a));
    assert(!scheduler_step(a, sink));
    a->quantum = 10;
    assert(session_start(a, "Short", 1, 1));
    assert(scheduler_step(a, sink));
    assert(a->now == 1 && a->tasks[0].notified);
    assert(!scheduler_step(a, sink));
    assert(task_create(a, "ExitReady", 3, 200, false));
    assert(task_create(a, "ExitWaiting", 3, 300, false));
    assert(task_block(a, 3));
    app_shutdown(a);
    assert(a->memory.used == 0 && a->memory.count == 0 && memory_check(&a->memory));
}

/* Check FIFO order and targeted removal after the physical queue wraps around. */
static void queue_wraparound(void) {
    ReadyQueue queue = {0};
    int pid, i;
    for (i = 1; i <= 20; ++i) assert(queue_push(&queue, i));
    assert(!queue_push(&queue, 21));
    for (i = 1; i <= 15; ++i) { assert(queue_pop(&queue, &pid)); assert(pid == i); }
    for (i = 21; i <= 35; ++i) assert(queue_push(&queue, i));
    assert(!queue_push(&queue, 16));
    assert(queue_remove(&queue, 22));
    for (i = 16; i <= 35; ++i) {
        if (i == 22) continue;
        assert(queue_pop(&queue, &pid)); assert(pid == i);
    }
    assert(!queue_pop(&queue, &pid));
}

/* Run all direct assertions and return success only if no assertion fails. */
int main(void) {
    App app;
    FILE *sink = tmpfile();
    assert(sink);
    reference(&app, sink);
    blocking_and_arrival(&app, sink);
    limits_and_rollback(&app, sink);
    queue_wraparound();
    assert(fclose(sink) == 0);
    puts("CORE ASSERTIONS PASS: exact metrics, blocked wait, late arrivals, rollback, capacity, queue wrap, cleanup.");
    return 0;
}
