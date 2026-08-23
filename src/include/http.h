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
 * provided `transport4` with the given socket.  Can be called again if the
 * connection drops to re-initialize the connection.
 */
int init_plaintext_transport(http_transport_t *transport, const char *domain);

/**
 * Closes the socket associated with the given transport.  Panics with a runtime
 * assertion error if the transport is not a plaintext transport (ctx_type !=
 * HTTP_CTX_SOCKET).
 */
int close_plaintext_transport(http_transport_t *transport);

/**
 * Returns a string explanation of the last error encountered.
 */
const char *get_http_error(void);

/**
 * Buffer used for constructing and parsing request and response headers.  After
 * a request is sent off, this buffer can be safely re-used for the response
 * headers.
 */
typedef struct http_buf_t {
    char *head_buf;
    size_t head_buf_len;
    size_t cursor;
} http_buf_t;

/**
 * Writes the request line to the request's internal header buffer.   Does not perform
 * a syscall.  Returns -1 on error, or 0 on success.
 */
int write_request_line(http_buf_t *request, const char *method, const char *path);

/**
 * Writes the given key/value pair to the request's internal header buffer.
 * Does not perform a syscall.  Returns -1 on error or 0 on success.
 */
int write_header(http_buf_t *request, const char *key, const char *value);

/**
 * Writes the final CRLF to the end of the headers stored in the request buffer
 * and sends the request off through the transport's configured write function.
 * Returns -1 on error if there is not space for the final CRLF, or the result
 * of the transport's write function.
 */
ssize_t flush_request_to_transport(http_transport_t *transport, http_buf_t *request);

#ifdef __cplusplus
}
#endif

#endif // _MC_HTTP_H
