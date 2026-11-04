#include "core.h"
#include "shell.h"
#include <string.h>

int main(int argc, char **argv) {
    App app;
    const char *directory = "data";
    bool interactive = true;
    int i;
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--batch")) interactive = false;
        else if (!strcmp(argv[i], "--data-dir") && i + 1 < argc) directory = argv[++i];
        else {
            fprintf(stderr, "Usage: %s [--batch] [--data-dir DIRECTORY]\n", argv[0]);
            return 1;
        }
    }
    if (!app_init(&app, directory, 2)) {
        fprintf(stderr, "ERROR: %s\n", app.error);
        return 1;
    }
    return shell_run(&app, interactive);
}
