/**
 * @file notes.c
 * @brief Persistent study notes.
 *
 * Provides create, append, read, and delete using standard C file I/O.
 * Notes live in a trusted local directory and survive simulator restarts.
 */

#include "notes.h"
#include "logger.h"

/**
 * Validate the note name and build DATA_DIR/notes/NAME.txt safely.
 * Report path truncation instead of using a partial path. The directory itself
 * is trusted local storage; this service is not a security sandbox.
 */
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

/**
 * Share create/append handling and append one newline-terminated text line.
 * Create preserves existing files; append requires an existing readable file.
 * Report write/close failures; content may be partial because writes are not
 * transactional. The public wrappers select which operation is requested.
 */
static bool write_note(App *app, const char *name, const char *text, bool append) {
    char path[PATH_SIZE + NAME_SIZE + 20];
    FILE *stream;
    bool okay;
    if (!note_path(app, name, path, sizeof path))
        return false;
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
    /* One command writes one text line. fclose also flushes buffered output. */
    okay = fputs(text, stream) != EOF;
    if (okay)
        okay = fputc('\n', stream) != EOF;
    if (fclose(stream) != 0)
        okay = false;
    if (!okay) {
        snprintf(app->error, ERROR_SIZE, "Note write/close failed; content may be partial.");
        return false;
    }
    logger_event(app, append ? "NOTE_APPEND" : "NOTE_CREATE", 0, name);
    return true;
}

/**
 * Create a new note without overwriting an existing one.
 * Return true only after the text and newline are written and fclose succeeds.
 */
bool note_create(App *app, const char *name, const char *text) {
    return write_note(app, name, text, false);
}

/**
 * Append one text line to an existing note and preserve its previous content.
 * Return false with app->error for validation, open, write, or close failures.
 */
bool note_append(App *app, const char *name, const char *text) {
    return write_note(app, name, text, true);
}

/**
 * Stream a note to the caller output without loading the whole file.
 * Check read, output, and close outcomes; chunked fgets calls also support
 * note lines longer than the temporary buffer.
 */
bool note_read(App *app, const char *name, FILE *out) {
    char path[PATH_SIZE + NAME_SIZE + 20], buffer[512];
    FILE *stream;
    bool okay = true;
    if (!note_path(app, name, path, sizeof path))
        return false;
    stream = fopen(path, "r");
    if (!stream) {
        snprintf(app->error, ERROR_SIZE, "Cannot read note: missing file or read failure.");
        return false;
    }
    /* Read in bounded chunks; a long note line may need several iterations. */
    while (fgets(buffer, sizeof buffer, stream))
        if (fputs(buffer, out) == EOF) {
            okay = false;
            break;
        }
    if (ferror(stream))
        okay = false;
    if (fclose(stream) != 0)
        okay = false;
    if (!okay) {
        snprintf(app->error, ERROR_SIZE, "Note read/output/close failed.");
        return false;
    }
    logger_event(app, "NOTE_READ", 0, name);
    return true;
}

/**
 * Delete a validated note path and record success only if remove succeeds.
 */
bool note_delete(App *app, const char *name) {
    char path[PATH_SIZE + NAME_SIZE + 20];
    if (!note_path(app, name, path, sizeof path))
        return false;
    if (remove(path) != 0) {
        snprintf(app->error, ERROR_SIZE, "Cannot delete note: missing file or remove failure.");
        return false;
    }
    logger_event(app, "NOTE_DELETE", 0, name);
    return true;
}
