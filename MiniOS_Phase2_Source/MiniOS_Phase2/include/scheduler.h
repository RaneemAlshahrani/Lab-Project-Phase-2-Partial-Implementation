#ifndef MINIOS_SCHEDULER_H
#define MINIOS_SCHEDULER_H
#include "core.h"
bool queue_push(ReadyQueue *queue, int pid);
bool queue_pop(ReadyQueue *queue, int *pid);
bool queue_remove(ReadyQueue *queue, int pid);
bool scheduler_step(App *app, FILE *out);
void scheduler_run(App *app, FILE *out);
void metrics_print(const App *app, FILE *out, bool csv);
bool metrics_export(App *app);
#endif
