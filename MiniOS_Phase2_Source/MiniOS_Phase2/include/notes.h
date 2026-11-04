#ifndef MINIOS_NOTES_H
#define MINIOS_NOTES_H
#include "core.h"
bool note_create(App *app, const char *name, const char *text);
bool note_append(App *app, const char *name, const char *text);
bool note_read(App *app, const char *name, FILE *out);
bool note_delete(App *app, const char *name);
#endif
