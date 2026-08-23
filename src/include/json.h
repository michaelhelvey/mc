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
#ifndef _MC_JSON_H
#define _MC_JSON_H

/**
 * MODULE DOCS: JSON
 *
 * Simple JSON parser implemented as a tokenizer.  Rather than recursively
 * parsing into a heap-allocated data structure, we simply expose an iterator
 * over a buffer that produces tokens.  The caller is intended to maintain state
 * on the stack in order to interpret this series of tokens, such as whether a
 * given JSON_TOK_STRING is in a key or value position within an object.
 *
 * Because containers (objects and arrays) are simply a series of these tokens,
 * skipping over a value that the caller doesn't care about -- e.g. a deeply
 * nested object -- would normally require walking every token inside it.  To
 * avoid that, `json_consume_value` will consume a whole value at once based
 * only on its structure.
 */

#include <stddef.h>
#include <ctype.h>
#include "common.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define _JSON_TOKS_FOR_EACH(ITER) \
    ITER(JSON_TOK_OBJECT_OPEN)    \
    ITER(JSON_TOK_OBJECT_CLOSE)   \
    ITER(JSON_TOK_LIST_OPEN)      \
    ITER(JSON_TOK_LIST_CLOSE)     \
    ITER(JSON_TOK_COLON)          \
    ITER(JSON_TOK_COMMA)          \
    ITER(JSON_TOK_STR)            \
    ITER(JSON_TOK_NUMBER)         \
    ITER(JSON_TOK_TRUE)           \
    ITER(JSON_TOK_FALSE)          \
    ITER(JSON_TOK_NULL)           \
    ITER(JSON_TOK_INVALID)

typedef enum json_tok_type_t {
#define _FOR(name) name,
    _JSON_TOKS_FOR_EACH(_FOR)
#undef _FOR
} json_tok_type_t;

typedef struct json_tok_t {
    string_view_t tok;
    json_tok_type_t tok_type;
} json_tok_t;

/**
 * Parse a single token out of the input buffer, returning the number of
 * characters consumed to produce the token (including whitespace, etc),
 * positioning buf + <return> at the position required to produce the following
 * token if called again when buf set to buf + <return>.  The actual token
 * produced is written to `out_token`.
 *
 * Because strings cannot be parsed as literal views into the buffer, because of
 * escape sequences, an additional buffer is provided in `str_buf` into which
 * strings will be decoded.  If not enough space in this buffer is provided,
 * then `json_parse` will return 0.  The memory in this buffer should be assumed
 * valid only until the next call to `json_parse`
 */
size_t json_parse(char *buf, size_t buf_len, json_tok_t *out_token, char *str_buf,
                  size_t str_buf_len);

/**
 * Consume a single JSON value from the input buffer, returning the number of
 * characters to advance so that `buf + <return>` is at the next token in the
 * stream.
 *
 * A "value" in considered to be any of:
 *   - a string literal
 *   - an object            ({ ... })
 *   - an array             ([ ... ])
 *   - a bare scalar token  (number, true, false, null, ...)
 * 
 * This function assumes that the buffer is positioned at the start of such a
 * value. If it is not, (e.g. the buffer is empty, whitespace only, or starts
 * with a leading delimiter), or the value is malformed or unterminated, then
 * this function returns 0. 
 */
size_t json_consume_value(char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif // _MC_JSON_H
