/**
 * @file core.h
 * @brief Shared model, limits, and application lifecycle interfaces.
 */

#ifndef MINIOS_CORE_H
#define MINIOS_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* Limits for the educational simulation, independent of host OS resources. */
#define MAX_TASKS 20
#define MEMORY_BUDGET 65536u
#define NAME_SIZE 49        /* Up to 48 name characters plus the null terminator. */
#define PATH_SIZE 512
#define ERROR_SIZE 256
#define MAX_TICKS 1000000    /* Upper bound for one burst or time quantum. */

/* RUNNING exists only during a dispatch; commands observe the other states. */
typedef enum {
    READY,
    RUNNING,
    WAITING,
    FINISHED
} TaskState;
/* Retained record for a simulated task, not a real host process. */
typedef struct {
    int pid;
    char name[NAME_SIZE];
    TaskState state;
    /* Original requested work and work still to execute, in virtual ticks. */
    int burst, remaining;
    /* first_run/completion are -1 until their events occur. */
    /* ready_wait counts only READY time and excludes WAITING intervals. */
    long long arrival, first_run, completion, ready_wait;
    void *payload;         /* Borrowed alias of the registry-owned allocation. */
    size_t payload_bytes;  /* Original size; retained after the payload is freed. */
    bool is_session, notified; /* Enables one completion notice per session. */
} Task;

/* Logical FIFO position i is stored at ids[(head + i) % MAX_TASKS]. */
typedef struct {
    int ids[MAX_TASKS];
    size_t head, count;
} ReadyQueue;
/* A NULL pointer marks a free slot; owner is a simulated PID. */
typedef struct {
    void *pointer;
    int owner;
    size_t bytes;
} AllocationRecord;
/* The registry is the only owner that calls free on task payloads. */
typedef struct {
    AllocationRecord records[MAX_TASKS];
    size_t used, count;   /* Live payload bytes and live allocation records. */
    bool fail_next;      /* Test hook: fail one otherwise valid allocation. */
} Memory;
/* Owns the log stream; failures contributes to the application exit status. */
typedef struct {
    FILE *stream;
    unsigned failures;
} Logger;

/* Shared state passed explicitly to services; avoids mutable global state. */
typedef struct {
    Task tasks[MAX_TASKS];
    size_t task_count;    /* Includes FINISHED records until reset. */
    /* PID assigned on the next success, future slice limit, reset/run label. */
    int next_pid, quantum, experiment;
    long long now;       /* Virtual clock; not wall-clock time. */
    ReadyQueue queue;
    Memory memory;
    Logger logger;
    FILE *trace;         /* Owned scheduling CSV stream, closed at shutdown. */
    /* Runtime directory and the most recent service error shown by the shell. */
    char data_dir[PATH_SIZE], error[ERROR_SIZE];
} App;

/* Initialize empty state and open output streams; false sets app->error. */
bool app_init(App *app, const char *data_dir, int quantum);

/* Free registered payloads and close files after successful initialization. */
void app_shutdown(App *app);

/* Release payloads and clear task state; preserve notes, quantum, and streams. */
void app_reset(App *app);

/* Check queue membership and payload ownership between commands. */
bool app_check(const App *app);

/* Return a static display label; the caller must not free it. */
const char *state_name(TaskState state);

/* Accept 1..48 ASCII letters, digits, underscores, or hyphens. */
bool valid_name(const char *name);

#endif /* MINIOS_CORE_H */
