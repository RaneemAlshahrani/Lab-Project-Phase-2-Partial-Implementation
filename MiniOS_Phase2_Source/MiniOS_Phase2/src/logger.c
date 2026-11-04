#include "logger.h"

bool logger_open(Logger *logger, const char *path) {
    logger->stream = fopen(path, "a");
    logger->failures = 0;
    return logger->stream != NULL;
}

void logger_event(App *app, const char *event, int pid, const char *detail) {
    Logger *logger = &app->logger;
    if (!logger->stream) return;
    if (fprintf(logger->stream, "experiment=%d tick=%lld event=%s pid=%d %s\n",
                app->experiment, app->now, event, pid, detail) < 0 ||
        fflush(logger->stream) != 0) {
        ++logger->failures;
        fprintf(stderr, "WARNING: event log write failed.\n");
    }
}

void logger_close(Logger *logger) {
    if (logger->stream && fclose(logger->stream) != 0) {
        ++logger->failures;
        fprintf(stderr, "WARNING: event log close failed.\n");
    }
    logger->stream = NULL;
}
