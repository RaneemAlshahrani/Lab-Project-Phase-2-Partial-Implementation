#ifndef MINIOS_CORE_H
#define MINIOS_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define MAX_TASKS 20
#define MEMORY_BUDGET 65536u
#define NAME_SIZE 49
#define PATH_SIZE 512
#define ERROR_SIZE 256
#define MAX_TICKS 1000000

typedef enum { READY, RUNNING, WAITING, FINISHED } TaskState;
typedef struct {
    int pid;
    char name[NAME_SIZE];
    TaskState state;
    int burst, remaining;
    long long arrival, first_run, completion, ready_wait;
    void *payload;
    size_t payload_bytes;
    bool is_session, notified;
} Task;

typedef struct { int ids[MAX_TASKS]; size_t head, count; } ReadyQueue;
typedef struct { void *pointer; int owner; size_t bytes; } AllocationRecord;
typedef struct {
    AllocationRecord records[MAX_TASKS];
    size_t used, count;
    bool fail_next;
} Memory;
typedef struct { FILE *stream; unsigned failures; } Logger;

typedef struct {
    Task tasks[MAX_TASKS];
    size_t task_count;
    int next_pid, quantum, experiment;
    long long now;
    ReadyQueue queue;
    Memory memory;
    Logger logger;
    FILE *trace;
    char data_dir[PATH_SIZE], error[ERROR_SIZE];
} App;

bool app_init(App *app, const char *data_dir, int quantum);
void app_shutdown(App *app);
void app_reset(App *app);
bool app_check(const App *app);
const char *state_name(TaskState state);
bool valid_name(const char *name);

#endif
