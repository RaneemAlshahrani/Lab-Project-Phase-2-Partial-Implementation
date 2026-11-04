/**
 * @file logger.h
 * @brief Event stream ownership and nonfatal I/O-failure accounting.
 */

#ifndef MINIOS_LOGGER_H
#define MINIOS_LOGGER_H
#include "core.h"

/* Open an append stream owned by Logger; false means open failure. */
bool logger_open(Logger *logger, const char *path);

/* Append/flush an event; I/O errors warn and increment logger.failures. */
void logger_event(App *app, const char *event, int pid, const char *detail);

/* Close the owned stream and record close errors; clear stream afterward. */
void logger_close(Logger *logger);

#endif /* MINIOS_LOGGER_H */
