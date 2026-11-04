/**
 * @file memory.h
 * @brief PID-owned allocation registry and 64 KiB payload-budget interfaces.
 */

#ifndef MINIOS_MEMORY_H
#define MINIOS_MEMORY_H
#include "core.h"

/* Return a registry-owned block or NULL; error must hold ERROR_SIZE bytes. */
void *memory_allocate(Memory *memory, int owner, size_t bytes, char *error);

/* Release one owner PID once; false means there is no matching allocation. */
bool memory_release(Memory *memory, int owner);

/* Free all registered blocks and reset counters and the failure hook. */
void memory_clear(Memory *memory);

/* Validate registry uniqueness, byte/count totals, and the payload budget. */
bool memory_check(const Memory *memory);

#endif /* MINIOS_MEMORY_H */
