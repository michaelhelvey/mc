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
 * A non-null-terminated, read-only immutable view into a character buffer owned
 * elsewhere.
 * 
 * Usage rule in this codebase: string_view_t is used whenever you're
 * representing an *immutable string value*.  When you're mutating a buffer of
 * memory, we represent this with a standard char*, size_t pair. It should be
 * obvious at the callsite which is which -- string_view_t is never an output
 * parameter that we write into, and we enforce with this a `const` annotation
 * on `*buf`.
 */
typedef struct string_view_t {
    const char *buf;
    size_t len;
} string_view_t;

/**
 * Helper to print a pointer to a string_view_t: `printf("sv: %.*s\n", SV_FMT_PTR(sv));`
 */
#define SV_FMT_PTR(sv) (int)(sv->len), (sv)->buf

/**
 * Helper to print a string_view_t (not a pointer): `printf("sv: %.*s\n", SV_FMT(sv))`;
 */
#define SV_FMT(sv) (int)(sv.len), (sv).buf

/**
 * Helper to create a string_view_t from a string literal: `string_view_t str =
 * SV_LIT("literal");`
 */
#define SV_LIT(s) ((const string_view_t){ .buf = (s), .len = sizeof(s) - 1 })

/**
 * Helper to create a string_view_t from an arbitrary buffer and a length.
 */
#define SV_FROM(s, sl) ((string_view_t){ .buf = (s), .len = sl })

/**
 * Compares two string views. Logically similar to `strcmp` from the C standard
 * library, except that it's safe because we know the lengths of the strings
 * going in. Returns `false` from stdbool if the strings are not equal, and
 * `true` if they are.
 *
 * Performance is O(n) in the worst case, and O(1) if the strings are different lengths.
 */
bool str_view_cmp(const string_view_t *a, const string_view_t *b);

/**
 * Case-insensitive variant of `str_view_cmp`.  Returns `true` if both views
 * have the same length and agree on every byte when compared case-insensitively
 * (e.g. useful for matching HTTP header field names, which are case-insensitive
 * by spec).  Returns `false` otherwise.
 */
bool str_view_cmp_ci(const string_view_t *a, const string_view_t *b);

/**
 * Copies the bytes of `sv` into `out` as a NUL-terminated C string. Useful
 * when bridging into a libc / POSIX API that requires a `const char *` (e.g.
 * `getaddrinfo`).
 * 
 * Returns whether or not the NUL-terminated bytes fit into `out`.
 */
bool sv_to_c_str(string_view_t sv, char *out, size_t out_cap);

/**
 * Convenience macro: `sv_to_c_str((sv), (buf), sizeof(buf))`.
 */
#define SV_AS_C_STR(sv, buf) sv_to_c_str((sv), (buf), sizeof(buf))

typedef ssize_t (*buffered_reader_read_fn)(void *, char *, size_t);

/**
 * A generic buffered reader implementation that provides the usual facilities
 * for reading up to particular characters and patterns while limiting syscalls.
 */
typedef struct buffered_reader_t {
    /* A pointer to some context that will be passed to our read function. e.g. a FILE* */
    void *ctx;
    /* generic blocking read function.  a negative return is assumed to be an
     * error, and a read of 0 bytes is assumed to represent EOF */
    buffered_reader_read_fn read;
    /* whether it is possible to read more from the underlying IO source.  once
     * set to false due to a blocking read of 0 bytes, stays false forever */
    bool readable;
    /* buffer and length used to hold bytes before they are consumed */
    char *buf;
    size_t buf_len;
    /* cursor into buf representing where we should start reading for callers*/
    size_t read_cursor;
    /* cursor into buf representing where we should start writing when reading
     * more from the underlying IO source */
    size_t write_cursor;
    /* how many bytes at a time to attempt to read out of the underlying source.
     * must be <= buf_len -- a reader whose chunk_size exceeds its buffer length
     * is malformed and is rejected by the read functions. */
    size_t chunk_size;
} buffered_reader_t;

#define BUFFERED_READER_DEFAULT_CHUNK_SIZE 4096
#define buffered_reader_defaults_from(ctxarg, read_fn, bufarg, buf_lenarg) \
    ((buffered_reader_t){ .ctx = (void *)ctxarg,                           \
                          .read = (buffered_reader_read_fn)read_fn,        \
                          .buf = bufarg,                                   \
                          .buf_len = buf_lenarg,                           \
                          .readable = true,                                \
                          .read_cursor = 0,                                \
                          .write_cursor = 0,                               \
                          .chunk_size = BUFFERED_READER_DEFAULT_CHUNK_SIZE })

/**
 * Reads up to `n` bytes out of the underlying buffer into `out`. Returns
 * either the number of bytes read or -1 in the case of an error.
 *
 * `n` is the number of bytes the caller wishes to consume; the destination
 * `out` must have capacity of at least `out_cap` bytes, and `out_cap` must be
 * at least `n`. It is also considered an error to request more bytes than the
 * reader's chunk size.
 */
ssize_t buffered_reader_read_nbytes(buffered_reader_t *reader, char *out, size_t out_cap, size_t n);

/**
 * Reads up to `out_cap` bytes out of the underlying buffer into `out`,
 * continuing until `out` is entirely filled or the underlying source reports
 * EOF (whichever comes first).  Returns the number of bytes read, or -1 in the
 * case of an error.
 *
 * `out` must have capacity of at least `out_cap` bytes.  Internally this reads
 * in `chunk_size`-sized pieces and copies them out, so there is no limit to the
 * number of bytes that can be requested.
 */
ssize_t buffered_reader_read_all(buffered_reader_t *reader, char *out, size_t out_cap);

/**
 * Pushes back `n` previously-consumed bytes so they will be returned by
 * subsequent reads.  The bytes must still be present in the reader's buffer
 * (i.e. between `read_cursor` and `write_cursor`); asking to unread more
 * bytes than are currently buffered is an error.  Returns 0 on success, or
 * -1 if `n` exceeds the number of bytes available to unread.
 */
int buffered_reader_unread(buffered_reader_t *reader, size_t n);

/**
 * Consumes characters from their reader, copying them into `out` (with capacity
 * `out_cap`), until the first instance of `needle` is encountered. `needle` is
 * a read-only string value and is itself copied into `out`. The number of bytes
 * consumed is returned, or -1 in the case of an error (including the case where
 * the buffer is too small to accommodate the needle).
 */
ssize_t buffered_reader_read_until(buffered_reader_t *reader, string_view_t needle, char *out,
                                   size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif // _MC_COMMON_H
