#include "session.h"
#include "tasks.h"
#include "logger.h"

bool session_start(App *app, const char *name, int ticks, size_t bytes) {
    return task_create(app, name, ticks, bytes, true);
}

void session_complete(App *app, Task *task, FILE *out) {
    if (task->is_session && task->state == FINISHED && !task->notified) {
        task->notified = true;
        fprintf(out, "SESSION COMPLETE: P%d %s at tick %lld\n", task->pid, task->name, app->now);
        logger_event(app, "SESSION_COMPLETE", task->pid, task->name);
    }
}
