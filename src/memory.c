/**
 * @file memory.c
 * @brief Task-payload memory ownership.
 *
 * Wraps malloc/free with a PID ownership registry and a 64 KiB budget.
 * Only payload bytes count toward the budget, not static task records or logs.
 */

#include "memory.h"

#include <stdlib.h>
#include <string.h>

/**
 * Allocate one nonzero payload for an owner PID within the shared budget.
 * Check capacity and the optional failure hook before malloc; publish a registry
 * record and increase counters only after allocation succeeds. Return NULL and
 * fill the caller error buffer (ERROR_SIZE bytes) when the request fails.
 */
void *memory_allocate(Memory *m, int owner, size_t bytes, char *error) {
    size_t slot;
    void *pointer;
    if (bytes == 0 || bytes > MEMORY_BUDGET) {
        snprintf(error, ERROR_SIZE, "Payload must be 1..%u bytes.", MEMORY_BUDGET);
        return NULL;
    }
    if (bytes > MEMORY_BUDGET - m->used) {
        snprintf(error, ERROR_SIZE, "Memory budget exceeded: requested=%zu free=%zu bytes.",
                 bytes, MEMORY_BUDGET - m->used);
        return NULL;
    }
    /* One unfinished task may own only one payload at a time. */
    for (slot = 0; slot < MAX_TASKS; ++slot) {
        if (m->records[slot].pointer && m->records[slot].owner == owner) {
            snprintf(error, ERROR_SIZE, "PID already owns an allocation.");
            return NULL;
        }
    }
    /* A NULL registry pointer denotes a free slot. */
    for (slot = 0; slot < MAX_TASKS && m->records[slot].pointer; ++slot) { }
    if (slot == MAX_TASKS) {
        snprintf(error, ERROR_SIZE, "Allocation registry is full.");
        return NULL;
    }
    /* The hook exercises the same rollback path as malloc returning NULL. */
    if (m->fail_next) {
        m->fail_next = false;
        snprintf(error, ERROR_SIZE, "Forced allocation failure; no task was created.");
        return NULL;
    }
    pointer = malloc(bytes);
    if (!pointer) {
        snprintf(error, ERROR_SIZE, "Host allocation failed; no task was created.");
        return NULL;
    }
    /* Initialize the payload before making its allocation visible. */
    memset(pointer, 0, bytes);
    m->records[slot] = (AllocationRecord){pointer, owner, bytes};
    m->used += bytes;
    ++m->count;
    return pointer;
}

/**
 * Find and free the allocation owned by one PID, then clear its registry slot.
 * Return true if a block was released. A missing owner returns false, so a
 * repeated release never frees the same pointer twice.
 */
bool memory_release(Memory *m, int owner) {
    size_t i;
    for (i = 0; i < MAX_TASKS; ++i) {
        AllocationRecord *r = &m->records[i];
        if (r->pointer && r->owner == owner) {
            /* Clear the record after free so future lookup cannot reuse it. */
            free(r->pointer);
            m->used -= r->bytes;
            --m->count;
            *r = (AllocationRecord){0};
            return true;
        }
    }
    return false; /* A second release never calls free again. */
}

/**
 * Free all remaining registered payloads and reset accounting/failure state.
 * Used by reset and shutdown, including when tasks are unfinished or WAITING.
 */
void memory_clear(Memory *m) {
    size_t i;
    for (i = 0; i < MAX_TASKS; ++i) {
        if (m->records[i].pointer)
            free(m->records[i].pointer);
    }
    memset(m, 0, sizeof *m);
}

/**
 * Recalculate live bytes/count and check for duplicate owners or pointers.
 * Compare calculated totals with stored counters and the configured budget.
 */
bool memory_check(const Memory *m) {
    size_t i, j, bytes = 0, count = 0;
    for (i = 0; i < MAX_TASKS; ++i) {
        const AllocationRecord *r = &m->records[i];
        if (!r->pointer)
            continue;
        if (r->owner < 1 || r->bytes == 0)
            return false;
        /* Distinct live records must not share either an owner or pointer. */
        for (j = 0; j < i; ++j)
            if (m->records[j].pointer &&
                (m->records[j].owner == r->owner || m->records[j].pointer == r->pointer))
                return false;
        bytes += r->bytes;
        ++count;
    }
    return bytes == m->used && count == m->count && bytes <= MEMORY_BUDGET;
}
