#ifndef MINIOS_MEMORY_H
#define MINIOS_MEMORY_H
#include "core.h"
void *memory_allocate(Memory *memory, int owner, size_t bytes, char *error);
bool memory_release(Memory *memory, int owner);
void memory_clear(Memory *memory);
bool memory_check(const Memory *memory);
#endif
