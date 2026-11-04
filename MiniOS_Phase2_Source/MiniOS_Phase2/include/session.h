#ifndef MINIOS_SESSION_H
#define MINIOS_SESSION_H
#include "core.h"
bool session_start(App *app, const char *name, int ticks, size_t bytes);
void session_complete(App *app, Task *task, FILE *out);
#endif
