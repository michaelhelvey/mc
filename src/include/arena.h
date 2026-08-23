/*
 * MIT License
 *
 * Copyright (c) 2026 Michael Helvey
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef _MC_ARENA_H
#define _MC_ARENA_H

/*
 * MODULE DOCS: Arena
 *
 * A simple bump-pointer arena allocator.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct region_t {
    uintptr_t start;
    size_t len;
    uintptr_t free_cursor;
    struct region_t *next;
} region_t;

typedef struct arena_t {
    region_t *head;
    region_t *current;
    size_t region_count;
    size_t page_size;
} arena_t;

/**
 * Rounds `addr` up to the nearest multiple of `align`. `align` must be a non-zero power of two.
 */
uintptr_t arena_align_up(uintptr_t addr, size_t align);

/**
 * Creates a new memory region of the given size. Returns NULL on failure.
 */
region_t *region_create(size_t size);

/**
 * Initializes an arena_t. Must be called before any allocation.
 */
void arena_init(arena_t *arena);

/**
 * Allocates `size` bytes aligned to `align` from the arena. Returns NULL on
 * failure or if the arena is NULL.
 */
void *arena_alloc_aligned(arena_t *arena, size_t size, size_t align);

/**
 * Releases all memory held by the arena. The arena_t struct itself is not freed.
 */
void arena_release(arena_t *arena);

#define arena_alloc(arena, type) \
    ((type *)arena_alloc_aligned((arena), sizeof(type), _Alignof(type)))

#define arena_allocn(arena, type, count) \
    ((type *)arena_alloc_aligned((arena), sizeof(type) * (count), _Alignof(type)))

#ifdef __cplusplus
}
#endif

#endif // _MC_ARENA_H
