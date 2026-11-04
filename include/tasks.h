/**
 * @file tasks.h
 * @brief Task lookup, creation, state transitions, and display interfaces.
 */

#ifndef MINIOS_TASKS_H
#define MINIOS_TASKS_H
#include "core.h"

/* Return a borrowed task-record pointer, or NULL; invalidated by reset. */
Task *task_find(App *app, int pid);

/* Commit a READY task only after allocation/enqueue; false sets app->error. */
bool task_create(App *app, const char *name, int burst, size_t bytes, bool session);

/* Move only an existing READY task to WAITING; false sets app->error. */
bool task_block(App *app, int pid);

/* Append only an existing WAITING task to READY; false sets app->error. */
bool task_wake(App *app, int pid);

/* Print retained records and FIFO order; do not change application state. */
void tasks_print(const App *app, FILE *out);

#endif /* MINIOS_TASKS_H */
