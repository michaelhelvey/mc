#include <stdio.h>
#include <assert.h>

#include "arena.h"

static void test_arena_alloc_basic(void)
{
    arena_t arena;
    arena_init(&arena);

    int *p = arena_alloc(&arena, int);
    assert(p != NULL && "expected non-null allocation");
    *p = 42;
    assert(*p == 42 && "expected value to survive");

    arena_release(&arena);
}

static void test_arena_allocn_array(void)
{
    arena_t arena;
    arena_init(&arena);

    int *arr = arena_allocn(&arena, int, 10);
    assert(arr != NULL && "expected non-null array allocation");
    for (int i = 0; i < 10; i++) {
        arr[i] = i;
    }
    for (int i = 0; i < 10; i++) {
        assert(arr[i] == i && "expected array values to survive");
    }

    arena_release(&arena);
}

static void test_arena_multiple_regions(void)
{
    arena_t arena;
    arena_init(&arena);

    // Allocate enough to force a second region
    size_t page_size = arena.page_size;
    char *big = arena_allocn(&arena, char, page_size - sizeof(region_t) - 256);
    assert(big != NULL && "expected large allocation to succeed");

    char *more = arena_allocn(&arena, char, 128);
    assert(more != NULL && "expected subsequent allocation to succeed (new region)");

    arena_release(&arena);
}

#define TEST(t)                          \
    t();                                 \
    printf("[ARENA]: Passed: %s\n", #t); \
    ;

void test_arena(void)
{
    TEST(test_arena_alloc_basic);
    TEST(test_arena_allocn_array);
    TEST(test_arena_multiple_regions);
}
