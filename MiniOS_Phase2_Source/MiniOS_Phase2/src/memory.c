#include "memory.h"
#include <stdlib.h>
#include <string.h>

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
    for (slot = 0; slot < MAX_TASKS; ++slot) {
        if (m->records[slot].pointer && m->records[slot].owner == owner) {
            snprintf(error, ERROR_SIZE, "PID already owns an allocation.");
            return NULL;
        }
    }
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
    memset(pointer, 0, bytes);
    m->records[slot] = (AllocationRecord){pointer, owner, bytes};
    m->used += bytes;
    ++m->count;
    return pointer;
}

bool memory_release(Memory *m, int owner) {
    size_t i;
    for (i = 0; i < MAX_TASKS; ++i) {
        AllocationRecord *r = &m->records[i];
        if (r->pointer && r->owner == owner) {
            free(r->pointer);
            m->used -= r->bytes;
            --m->count;
            *r = (AllocationRecord){0};
            return true;
        }
    }
    return false; /* A second release never calls free again. */
}

void memory_clear(Memory *m) {
    size_t i;
    for (i = 0; i < MAX_TASKS; ++i) {
        if (m->records[i].pointer) free(m->records[i].pointer);
    }
    memset(m, 0, sizeof *m);
}

bool memory_check(const Memory *m) {
    size_t i, j, bytes = 0, count = 0;
    for (i = 0; i < MAX_TASKS; ++i) {
        const AllocationRecord *r = &m->records[i];
        if (!r->pointer) continue;
        if (r->owner < 1 || r->bytes == 0) return false;
        for (j = 0; j < i; ++j)
            if (m->records[j].pointer &&
                (m->records[j].owner == r->owner || m->records[j].pointer == r->pointer))
                return false;
        bytes += r->bytes;
        ++count;
    }
    return bytes == m->used && count == m->count && bytes <= MEMORY_BUDGET;
}
