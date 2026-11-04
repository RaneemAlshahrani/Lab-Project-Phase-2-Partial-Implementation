/**
 * @file session.h
 * @brief Study-session task creation and one-time completion notification.
 */

#ifndef MINIOS_SESSION_H
#define MINIOS_SESSION_H
#include "core.h"

/* Create a session task with a duration measured in virtual ticks. */
bool session_start(App *app, const char *name, int ticks, size_t bytes);

/* Notify only a FINISHED session whose notified flag is still false. */
void session_complete(App *app, Task *task, FILE *out);

#endif /* MINIOS_SESSION_H */
