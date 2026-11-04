/**
 * @file main.c
 * @brief Program entry point.
 *
 * Reads launch options, creates the App model, and starts the command loop.
 * The shell owns cleanup after a successful initialization.
 */

#include "core.h"
#include "shell.h"

#include <string.h>

/**
 * Parse --batch and --data-dir, then initialize and run the application.
 * Return a nonzero status for invalid options, startup failures, or shell errors.
 */
int main(int argc, char **argv) {
    App app;
    const char *directory = "data";
    bool interactive = true;
    int i;
    /* Launch options belong to the host terminal, not the minios> prompt. */
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--batch"))
            interactive = false;
        else if (!strcmp(argv[i], "--data-dir") && i + 1 < argc)
            directory = argv[++i];
        else {
            fprintf(stderr, "Usage: %s [--batch] [--data-dir DIRECTORY]\n", argv[0]);
            return 1;
        }
    }
    /* Quantum 2 matches the reference experiment from the proposal. */
    if (!app_init(&app, directory, 2)) {
        fprintf(stderr, "ERROR: %s\n", app.error);
        return 1;
    }
    return shell_run(&app, interactive);
}
