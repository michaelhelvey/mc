#include "arena.h"

#include <sys/mman.h>
#include <unistd.h>

static void *arena_default_alloc(size_t size)
{
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}

static void arena_default_free(void *ptr, size_t size)
{
    munmap(ptr, size);
}

uintptr_t arena_align_up(uintptr_t addr, size_t align)
{
    return (addr + (align - 1)) & ~(align - 1);
}

region_t *region_create(size_t size)
{
    void *buf = arena_default_alloc(size);
    if (buf == NULL) {
        return NULL;
    }

    region_t *region = (region_t *)buf;
    region->start = (uintptr_t)buf + sizeof(region_t);
    region->len = size;
    // set the cursor to the largest alignment that any type could require so that we are guaranteed
    // to not have to re-align it when we first call `arena_alloc_aligned` on this region
    region->free_cursor = arena_align_up(region->start, _Alignof(max_align_t));
    region->next = NULL;
    return region;
}

void arena_init(arena_t *arena)
{
    arena->head = NULL;
    arena->current = NULL;
    arena->region_count = 0;
    long ps = sysconf(_SC_PAGESIZE);
    arena->page_size = (ps > 0) ? (size_t)ps : 0;
}

void *arena_alloc_aligned(arena_t *arena, size_t size, size_t align)
{
    if (arena == NULL || size == 0) {
        return NULL;
    }
    if (arena->page_size <= sizeof(region_t) || size > arena->page_size - sizeof(region_t)) {
        return NULL;
    }

    if (arena->current == NULL) {
        region_t *first = region_create(arena->page_size);
        if (first == NULL) {
            return NULL;
        }
        arena->head = first;
        arena->current = first;
        arena->region_count = 1;
    }

    uintptr_t aligned_cursor = arena_align_up(arena->current->free_cursor, align);
    size_t bytes_from_base = aligned_cursor - (uintptr_t)arena->current;
    size_t current_free = arena->current->len - bytes_from_base;

    if (size > current_free) {
        region_t *new_region = region_create(arena->current->len);
        if (new_region == NULL) {
            return NULL;
        }
        arena->current->next = new_region;
        arena->current = new_region;
        arena->region_count++;
        aligned_cursor = arena_align_up(arena->current->free_cursor, align);
    }

    arena->current->free_cursor = aligned_cursor + size;
    return (void *)aligned_cursor;
}

void arena_release(arena_t *arena)
{
    if (arena == NULL) {
        return;
    }

    region_t *current = arena->head;
    while (current != NULL) {
        region_t *next = current->next;
        arena_default_free((void *)current, current->len);
        current = next;
    }
    arena->head = NULL;
    arena->current = NULL;
    arena->region_count = 0;
}
