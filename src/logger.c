/**
 * @file logger.c
 * @brief Shared event logging.
 *
 * Appends events with an experiment number, virtual tick, PID, and detail.
 * Write failures are reported to stderr and remembered for the exit status.
 */

#include "logger.h"

/**
 * Open the append-only event stream and reset its detected-failure counter.
 * Return false when the path cannot be opened; the logger owns this stream.
 */
bool logger_open(Logger *logger, const char *path) {
    logger->stream = fopen(path, "a");
    logger->failures = 0;
    return logger->stream != NULL;
}

/**
 * Append and flush one event using the current experiment and virtual clock.
 * PID 0 denotes an application/file event rather than a particular task.
 * This void function warns and records I/O failure for the shell exit status.
 */
void logger_event(App *app, const char *event, int pid, const char *detail) {
    Logger *logger = &app->logger;
    if (!logger->stream)
        return;
    /* Flush per event so a later error is not reported as a successful write. */
    if (fprintf(logger->stream, "experiment=%d tick=%lld event=%s pid=%d %s\n",
                app->experiment, app->now, event, pid, detail) < 0 ||
        fflush(logger->stream) != 0) {
        ++logger->failures;
        fprintf(stderr, "WARNING: event log write failed.\n");
    }
}

/**
 * Close the owned stream once and clear its pointer.
 * Preserve a detected close failure in logger->failures for the caller to see.
 */
void logger_close(Logger *logger) {
    if (logger->stream && fclose(logger->stream) != 0) {
        ++logger->failures;
        fprintf(stderr, "WARNING: event log close failed.\n");
    }
    logger->stream = NULL;
}
