#ifndef MINIOS_LOGGER_H
#define MINIOS_LOGGER_H
#include "core.h"
bool logger_open(Logger *logger, const char *path);
void logger_event(App *app, const char *event, int pid, const char *detail);
void logger_close(Logger *logger);
#endif
