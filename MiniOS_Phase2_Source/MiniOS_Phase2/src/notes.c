#include "notes.h"
#include "logger.h"

static bool note_path(App *app, const char *name, char *path, size_t size) {
    int length;
    if (!valid_name(name)) {
        snprintf(app->error, ERROR_SIZE, "Note name must use 1..48 letters/digits/_/- (no path separators).");
        return false;
    }
    length = snprintf(path, size, "%s/notes/%s.txt", app->data_dir, name);
    if (length < 0 || (size_t)length >= size) {
        snprintf(app->error, ERROR_SIZE, "Note path is too long.");
        return false;
    }
    return true;
}

static bool write_note(App *app, const char *name, const char *text, bool append) {
    char path[PATH_SIZE + NAME_SIZE + 20];
    FILE *stream;
    bool okay;
    if (!note_path(app, name, path, sizeof path)) return false;
    if (append) {
        /* Append must not silently create a previously missing note. */
        stream = fopen(path, "r");
        if (!stream) {
            snprintf(app->error, ERROR_SIZE, "Cannot append: note is missing or unreadable.");
            return false;
        }
        if (fclose(stream) != 0) {
            snprintf(app->error, ERROR_SIZE, "Note close failed before append.");
            return false;
        }
    }
    /* C11 exclusive create preserves an existing note instead of truncating it. */
    stream = fopen(path, append ? "a" : "wx");
    if (!stream) {
        snprintf(app->error, ERROR_SIZE, "Cannot %s note (check name, directory, permissions, or existing file).",
                 append ? "append to" : "create");
        return false;
    }
    okay = fputs(text, stream) != EOF;
    if (okay) okay = fputc('\n', stream) != EOF;
    if (fclose(stream) != 0) okay = false;
    if (!okay) {
        snprintf(app->error, ERROR_SIZE, "Note write/close failed; content may be partial.");
        return false;
    }
    logger_event(app, append ? "NOTE_APPEND" : "NOTE_CREATE", 0, name);
    return true;
}

bool note_create(App *app, const char *name, const char *text) {
    return write_note(app, name, text, false);
}

bool note_append(App *app, const char *name, const char *text) {
    return write_note(app, name, text, true);
}

bool note_read(App *app, const char *name, FILE *out) {
    char path[PATH_SIZE + NAME_SIZE + 20], buffer[512];
    FILE *stream;
    bool okay = true;
    if (!note_path(app, name, path, sizeof path)) return false;
    stream = fopen(path, "r");
    if (!stream) {
        snprintf(app->error, ERROR_SIZE, "Cannot read note: missing file or read failure.");
        return false;
    }
    while (fgets(buffer, sizeof buffer, stream))
        if (fputs(buffer, out) == EOF) { okay = false; break; }
    if (ferror(stream)) okay = false;
    if (fclose(stream) != 0) okay = false;
    if (!okay) {
        snprintf(app->error, ERROR_SIZE, "Note read/output/close failed.");
        return false;
    }
    logger_event(app, "NOTE_READ", 0, name);
    return true;
}

bool note_delete(App *app, const char *name) {
    char path[PATH_SIZE + NAME_SIZE + 20];
    if (!note_path(app, name, path, sizeof path)) return false;
    if (remove(path) != 0) {
        snprintf(app->error, ERROR_SIZE, "Cannot delete note: missing file or remove failure.");
        return false;
    }
    logger_event(app, "NOTE_DELETE", 0, name);
    return true;
}
