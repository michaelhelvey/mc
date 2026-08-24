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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE. 
 */
#ifndef _MC_HTTP_H
#define _MC_HTTP_H

#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdbool.h>

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

// declare a flag that the user can use to exclude OpenSSL from the build, if
// they only require the plaintext functions that depend on only libc.
#ifndef MC_OPENSSL_SUPPORT
#define MC_OPENSSL_SUPPORT 1
#endif

// forward declaration
struct http_transport_t;

/**
 * Writes size_t bytes from the const buffer provided into the transport,
 * returning either the number of bytes written, or -1 on error.
 */
typedef ssize_t (*http_write_fn)(struct http_transport_t *transport, const char *, size_t);

/**
 * Reads size_t bytes from the transport into the buffer, returning either the
 * number of bytes read, or -1 on error.
 */
typedef ssize_t (*http_read_fn)(struct http_transport_t *transport, char *, size_t);

#define HTTP_CTX_SOCKET 0
#define HTTP_CTX_SSL 1

/* 
 * Represents a generic readable / writable transport over some protocol.
 * allows the client to be agnostic to whether it is operating over plaintext or
 * TLS.
*/
typedef struct http_transport_t {
    /* selects the union member in `ctx`.  must be one of HTTP_CTX_SOCKET, HTTP_CTX_SSL */
    uint8_t ctx_type;
    union {
        int socket;
        void *ctx;
    } ctx;
    http_read_fn read;
    http_write_fn write;
} http_transport_t;

/**
 * Performs DNS resolution and connects to a given domain, initializing the
 * provided `transport` with the given socket.  Can be called again if the
 * connection drops to re-initialize the connection.
 */
int http_init_plaintext_transport(http_transport_t *transport, string_view_t domain);

/**
 * Closes the socket associated with the given transport.  Panics with a runtime
 * assertion error if the transport is not a plaintext transport (ctx_type !=
 * HTTP_CTX_SOCKET).
 */
int http_close_plaintext_transport(http_transport_t *transport);

/**
 * Returns a string explanation of the last error encountered.
 */
const char *http_get_error(void);

/**
 * Buffer used for constructing and parsing request and response headers.  After
 * a request is sent off, this buffer can be safely re-used for the response
 * headers.
 */
typedef struct http_client_t {
    http_transport_t *transport;
    char *head_buf;
    size_t head_buf_len;
    size_t cursor;
} http_client_t;

/**
 * Writes the request line to the client's internal header buffer.   Does not perform
 * a syscall.  Returns -1 on error, or 0 on success.
 */
int http_write_request_line(http_client_t *client, string_view_t method, string_view_t path);

/**
 * Writes the given key/value pair to the request's internal header buffer.
 * Does not perform a syscall.  Returns -1 on error or 0 on success.
 */
int http_write_header(http_client_t *client, string_view_t key, string_view_t value);

/**
 * Writes the final CRLF to the end of the headers stored in the request buffer
 * and sends the request off through the transport's configured write function.
 * Returns -1 on error if there is not space for the final CRLF, or the result
 * of the transport's write function.
 */
ssize_t http_flush_request(http_client_t *client);

typedef struct http_response_parser_t {
    size_t headers_cursor;
    char *head_buf;
    size_t head_buf_len;
    string_view_t head_buf_view;
    bool headers_complete;
    /* really the only "public" member -- you can use `reader` to read the body
     * after parsing the head, if you want */
    buffered_reader_t reader;
} http_response_parser_t;

/**
 * Reads the entirety of the response head into the parser's internal buffer.
 * Returns the number of bytes read, or -1 in the case of an error.
 */
ssize_t http_receive_head(http_response_parser_t *response_parser);

/**
 * Initializes `in_parser` from the given client, with caller-owned space for
 * reading the headers & statusline.
 */
void http_init_response_parser(http_client_t *client, http_response_parser_t *in_parser,
                               char *head_buf, size_t head_buf_len);

typedef struct http_header_t {
    string_view_t key;
    string_view_t value;
} http_header_t;

int http_header_next(http_response_parser_t *response_parser, http_header_t *out_header);

/**
 * Given a view of the entire HTTP response head, reads the statusline and
 * returns a view into the original buffer representing the statusline of the
 * request, sans the trailing CRLF.
 * 
 * This function currently doesn't have a failure case -- we just read until we
 * either run out room in the buffer or hit a \r\n.  If we hit the end of the
 * buffer then the returned string_view_t will just be the response head.
 */
string_view_t http_response_read_statusline(http_response_parser_t *response_parser);

#ifdef __cplusplus
}
#endif

#endif // _MC_HTTP_H
