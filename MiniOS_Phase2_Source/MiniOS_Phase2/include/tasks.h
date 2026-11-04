#ifndef MINIOS_TASKS_H
#define MINIOS_TASKS_H
#include "core.h"
Task *task_find(App *app, int pid);
bool task_create(App *app, const char *name, int burst, size_t bytes, bool session);
bool task_block(App *app, int pid);
bool task_wake(App *app, int pid);
void tasks_print(const App *app, FILE *out);
#endif
