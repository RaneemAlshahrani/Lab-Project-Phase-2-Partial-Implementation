#include "shell.h"
#include "logger.h"
#include "notes.h"
#include "scheduler.h"
#include "session.h"
#include "tasks.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define LINE_SIZE 2048

/* Space-delimited command words; note text is the untouched rest of the line. */
static char *word(char **cursor) {
    char *start;
    while (**cursor == ' ' || **cursor == '\t') ++*cursor;
    if (!**cursor) return NULL;
    start = *cursor;
    while (**cursor && **cursor != ' ' && **cursor != '\t') ++*cursor;
    if (**cursor) *(*cursor)++ = '\0';
    return start;
}

static bool number(const char *text, long minimum, long maximum, long *result) {
    char *end;
    long value;
    size_t i;
    if (!text || !*text) return false;
    for (i = 0; text[i]; ++i) if (text[i] < '0' || text[i] > '9') return false;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno || *end || value < minimum || value > maximum) return false;
    *result = value;
    return true;
}

static void help(void) {
    puts("Commands (names use letters/digits/_/-; time is in virtual ticks):");
    puts("  task create NAME BURST BYTES   | tasks");
    puts("  session start NAME TICKS BYTES | block PID | wake PID");
    puts("  quantum N | step | run | metrics | export | memory");
    puts("  note create NAME TEXT | note append NAME TEXT");
    puts("  note read NAME | note delete NAME");
    puts("  check | reset | debug fail-next-allocation | help | quit");
}

/* Returns 0 for success, 1 for recoverable command errors, 2 for quit. */
static int execute(App *app, char *line) {
    char *cursor = line, *cmd = word(&cursor), *sub, *name, *a, *b;
    long value, bytes;
    bool okay = false;
    if (!cmd || cmd[0] == '#') return 0;
    app->error[0] = '\0';
    if (!strcmp(cmd, "task") || !strcmp(cmd, "session")) {
        bool session = !strcmp(cmd, "session");
        sub = word(&cursor); name = word(&cursor); a = word(&cursor); b = word(&cursor);
        if (!sub || strcmp(sub, session ? "start" : "create") || !name ||
            !number(a, 1, MAX_TICKS, &value) || !number(b, 1, MEMORY_BUDGET, &bytes) || word(&cursor))
            goto syntax;
        okay = session ? session_start(app, name, (int)value, (size_t)bytes) :
                        task_create(app, name, (int)value, (size_t)bytes, false);
        if (okay) printf("Created P%d %s: READY, burst=%ld, payload=%ld bytes\n",
                         app->next_pid - 1, name, value, bytes);
    } else if (!strcmp(cmd, "block") || !strcmp(cmd, "wake") || !strcmp(cmd, "quantum")) {
        a = word(&cursor);
        if (!number(a, 1, MAX_TICKS, &value) || word(&cursor)) goto syntax;
        if (!strcmp(cmd, "quantum")) {
            app->quantum = (int)value;
            logger_event(app, "QUANTUM", 0, a);
            okay = true;
        } else okay = !strcmp(cmd, "block") ? task_block(app, (int)value) : task_wake(app, (int)value);
        if (okay) printf("OK: %s %ld\n", cmd, value);
    } else if (!strcmp(cmd, "note")) {
        sub = word(&cursor); name = word(&cursor);
        if (!sub || !name) goto syntax;
        if (!strcmp(sub, "create") || !strcmp(sub, "append")) {
            while (*cursor == ' ' || *cursor == '\t') ++cursor;
            if (!*cursor) goto syntax;
            okay = !strcmp(sub, "create") ? note_create(app, name, cursor) : note_append(app, name, cursor);
        } else {
            if (word(&cursor)) goto syntax;
            if (!strcmp(sub, "read")) okay = note_read(app, name, stdout);
            else if (!strcmp(sub, "delete")) okay = note_delete(app, name);
            else goto syntax;
        }
        if (okay) printf("OK: note %s %s\n", sub, name);
    } else if (!strcmp(cmd, "debug")) {
        sub = word(&cursor);
        if (!sub || strcmp(sub, "fail-next-allocation") || word(&cursor)) goto syntax;
        app->memory.fail_next = true;
        logger_event(app, "DEBUG", 0, "next valid allocation will fail");
        puts("Debug hook armed: next valid allocation will fail.");
        okay = true;
    } else {
        if (word(&cursor)) goto syntax;
        okay = true;
        if (!strcmp(cmd, "help")) help();
        else if (!strcmp(cmd, "quit")) return 2;
        else if (!strcmp(cmd, "tasks")) tasks_print(app, stdout);
        else if (!strcmp(cmd, "step")) (void)scheduler_step(app, stdout);
        else if (!strcmp(cmd, "run")) scheduler_run(app, stdout);
        else if (!strcmp(cmd, "metrics")) metrics_print(app, stdout, false);
        else if (!strcmp(cmd, "export")) {
            okay = metrics_export(app);
            if (okay) puts("OK: metrics.csv exported (finished tasks only).");
        } else if (!strcmp(cmd, "memory"))
            printf("Memory: used=%zu/%u bytes free=%zu allocations=%zu\n", app->memory.used,
                   MEMORY_BUDGET, MEMORY_BUDGET - app->memory.used, app->memory.count);
        else if (!strcmp(cmd, "check")) {
            okay = app_check(app);
            if (okay) puts("CHECK PASS: queue, task states, and allocation ownership are consistent.");
            else snprintf(app->error, ERROR_SIZE, "Invariant check failed.");
        } else if (!strcmp(cmd, "reset")) {
            app_reset(app);
            puts("Reset: tasks cleared, clock=0, memory=0; notes preserved; quantum retained.");
        } else goto syntax;
    }
    if (okay) return 0;
    logger_event(app, "ERROR", 0, app->error);
    printf("ERROR: %s\n", app->error);
    return 1;
syntax:
    snprintf(app->error, ERROR_SIZE, "Invalid command or argument; enter help for syntax and limits.");
    logger_event(app, "ERROR", 0, app->error);
    printf("ERROR: %s\n", app->error);
    return 1;
}

int shell_run(App *app, bool interactive) {
    char line[LINE_SIZE];
    int result = 0;
    puts("MiniOS for a Study Room | C11 Phase 2 prototype");
    printf("One simulated CPU | quantum=%d | payload budget=%u bytes\n", app->quantum, MEMORY_BUDGET);
    puts("Type help for commands. Notes persist; task state starts fresh on restart.");
    while (true) {
        size_t length;
        int status;
        if (interactive) { fputs("minios> ", stdout); fflush(stdout); }
        if (!fgets(line, sizeof line, stdin)) break;
        length = strlen(line);
        /* Drain the WHOLE oversized command, so its tail cannot execute separately. */
        if (length == sizeof line - 1 && line[length - 1] != '\n') {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) { }
            puts("ERROR: command too long (maximum 2046 bytes); entire line discarded.");
            logger_event(app, "ERROR", 0, "oversized command discarded");
            continue;
        }
        while (length && (line[length - 1] == '\n' || line[length - 1] == '\r')) line[--length] = '\0';
        if (!interactive && length && line[0] != '#') printf("minios> %s\n", line);
        status = execute(app, line);
        if (status == 2) break;
        if (!app_check(app)) {
            fputs("FATAL: internal invariant failed.\n", stderr);
            result = 1;
            break;
        }
    }
    if (ferror(stdin)) { fputs("ERROR: standard input read failed.\n", stderr); result = 1; }
    app_shutdown(app);
    puts("Shutdown: used_bytes=0 allocations=0");
    return result || app->logger.failures ? 1 : 0;
}
