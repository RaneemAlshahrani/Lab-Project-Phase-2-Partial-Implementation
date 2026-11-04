/**
 * @file scheduler.h
 * @brief Bounded ready-queue operations, scheduling, and completed-task metrics.
 */

#ifndef MINIOS_SCHEDULER_H
#define MINIOS_SCHEDULER_H
#include "core.h"

/* Append a unique PID; false means the queue is full or PID is duplicated. */
bool queue_push(ReadyQueue *queue, int pid);

/* Write the oldest PID to *pid; false means the queue is empty. */
bool queue_pop(ReadyQueue *queue, int *pid);

/* Remove a PID without changing the order of others; false means absent. */
bool queue_remove(ReadyQueue *queue, int pid);

/* Run one slice; false means no READY task or an invalid queue entry. */
bool scheduler_step(App *app, FILE *out);

/* Run until no READY task remains or a dispatch cannot proceed. */
void scheduler_run(App *app, FILE *out);

/* Print FINISHED tasks only; csv selects machine-readable output. */
void metrics_print(const App *app, FILE *out, bool csv);

/* Write DATA_DIR/metrics.csv; false sets app->error. */
bool metrics_export(App *app);

#endif /* MINIOS_SCHEDULER_H */
