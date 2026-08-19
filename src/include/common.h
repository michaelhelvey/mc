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
#ifndef _MC_COMMON_H
#define _MC_COMMON_H

/*
 * MODULE DOCS: Common
 *
 * This module contains helper functions and types for common operations (e.g. a string view type)
 * that are lacking in the C std library, but are common enough patterns in practice that defining
 * them as-needed elsewhere would be needlessly repetitive.
 */

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A non-null-terminated view into a character buffer owned elsewhere.
 */
typedef struct string_view_t {
    char *buf;
    size_t buf_len;
} string_view_t;

/**
 * Helper to print a pointer to a string_view_t: `printf("sv: %.*s\n", SV_FMT_PTR(sv));`
 */
#define SV_FMT_PTR(sv) (int)(sv->buf_len), (sv)->buf

/**
 * Helper to print a string_view_t (not a pointer): `printf("sv: %.*s\n", SV_FMT(sv))`;
 */
#define SV_FMT(sv) (int)(sv.buf_len), (sv).buf

/**
 * Helper to create a string_view_t from a string literal: `string_view_t str = SV_LIT("literal");`
 */
#define SV_LIT(s) ((string_view_t){ .buf = (s), .buf_len = sizeof(s) - 1 })

/**
 * Compares two string views. Logically similar to `strcmp` from the C standard
 * library, except that it's safe because we know the lengths of the strings
 * going in. Returns `false` from stdbool if the strings are not equal, and
 * `true` if they are.
 *
 * Performance is O(n) in the worst case, and O(1) if the strings are different lengths.
 */
extern ssize_t str_view_cmp(string_view_t *a, string_view_t *b);

#ifdef __cplusplus
}
#endif

#endif // _MC_COMMON_H
