/**
 * @file notes.h
 * @brief Persistent note operations; false results explain the error in app->error.
 */

#ifndef MINIOS_NOTES_H
#define MINIOS_NOTES_H
#include "core.h"

/* Create NAME.txt with one text line; never replace an existing note. */
bool note_create(App *app, const char *name, const char *text);

/* Append one text line to an existing note; never silently create a new one. */
bool note_append(App *app, const char *name, const char *text);

/* Copy note text to out; check read, output, and close errors. */
bool note_read(App *app, const char *name, FILE *out);

/* Remove a validated note file; false sets app->error. */
bool note_delete(App *app, const char *name);

#endif /* MINIOS_NOTES_H */
