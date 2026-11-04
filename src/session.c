/**
 * @file session.c
 * @brief Simulated study sessions.
 *
 * Reuses normal task scheduling for a session duration in virtual ticks.
 * Sends one completion notice when the scheduler finishes the session task.
 */

#include "session.h"
#include "tasks.h"
#include "logger.h"

/**
 * Create an ordinary task marked as a study session.
 * ticks is simulated CPU work, not wall-clock seconds or minutes.
 */
bool session_start(App *app, const char *name, int ticks, size_t bytes) {
    return task_create(app, name, ticks, bytes, true);
}

/**
 * Emit a console notice and log event once for a finished session.
 * Normal tasks and already-notified sessions produce no additional notice.
 */
void session_complete(App *app, Task *task, FILE *out) {
    if (task->is_session && task->state == FINISHED && !task->notified) {
        /* The guard makes later idle runs harmless: no duplicate completion. */
        task->notified = true;
        fprintf(out, "SESSION COMPLETE: P%d %s at tick %lld\n", task->pid, task->name, app->now);
        logger_event(app, "SESSION_COMPLETE", task->pid, task->name);
    }
}
