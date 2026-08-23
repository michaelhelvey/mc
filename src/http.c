#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>

#include "http.h"

char err_buf[256];

const char *get_http_error(void)
{
    return (const char *)&err_buf;
}

ssize_t plaintext_transport_read(http_transport_t *transport, char *buf, size_t len)
{
    assert(transport->ctx_type == HTTP_CTX_SOCKET);
    return read(transport->ctx.socket, (void *)buf, len);
}

ssize_t plaintext_transport_write(http_transport_t *transport, const char *buf, size_t len)
{
    assert(transport->ctx_type == HTTP_CTX_SOCKET);

    size_t total = 0;
    const char *p = buf;

    while (total < len) {
        ssize_t n = write(transport->ctx.socket, p, len - total);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0) {
            errno = EPIPE;
            return -1;
        }
        total += (size_t)n;
        p += n;
    }

    return (ssize_t)total;
}

int init_plaintext_transport(http_transport_t *transport, const char *domain)
{
    memset(transport, 0, sizeof(http_transport_t));

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = PF_UNSPEC;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_DEFAULT;

    struct addrinfo *head0;
    int result = getaddrinfo(domain, "http", &hints, &head0);
    if (result != 0) {
        snprintf(err_buf, sizeof(err_buf), "%s", gai_strerror(result));
        return -1;
    }

    int s;
    size_t addr_count = 0;
    char hbuf[INET6_ADDRSTRLEN];
    struct addrinfo *head;

    for (head = head0; head; head = head->ai_next) {
        addr_count++;
        if (getnameinfo(head->ai_addr, head->ai_addrlen, hbuf, sizeof(hbuf), NULL, 0,
                        NI_NUMERICHOST) == 0) {
        }

        if ((s = socket(head->ai_family, head->ai_socktype, head->ai_protocol)) < 0) {
            continue;
        }

        if (connect(s, head->ai_addr, head->ai_addrlen) < 0) {
            close(s);
            s = -1;
            continue;
        }
    }

    freeaddrinfo(head0);
    if (s < 0) {
        snprintf(err_buf, sizeof(err_buf), "could not connect to any of %lu addreses for %s",
                 addr_count, domain);
        return -1;
    }

    transport->ctx.socket = s;
    transport->ctx_type = HTTP_CTX_SOCKET;
    transport->read = plaintext_transport_read;
    transport->write = plaintext_transport_write;

    return 0;
}

int close_plaintext_transport(http_transport_t *transport)
{
    assert(transport->ctx_type == HTTP_CTX_SOCKET);
    close(transport->ctx.socket);
    return 0;
}

int write_request_line(http_buf_t *request, const char *method, const char *path)
{
    request->cursor = 0;

    size_t used = snprintf(request->head_buf + request->cursor,
                           request->head_buf_len - request->cursor, "%s %s HTTP/1.1\r\n", method,
                           path);
    if (used >= request->head_buf_len - request->cursor) {
        snprintf(err_buf, sizeof err_buf,
                 "remaining head_buf_len of %lu was less than required for request line",
                 request->head_buf_len - request->cursor);
        return -1;
    }

    request->cursor += used;
    return 0;
}

int write_header(http_buf_t *request, const char *key, const char *value)
{
    size_t used = snprintf(request->head_buf + request->cursor,
                           request->head_buf_len - request->cursor, "%s: %s\r\n", key, value);
    if (used >= request->head_buf_len - request->cursor) {
        snprintf(err_buf, sizeof err_buf,
                 "remaining head_buf_len of %lu was less than required for header line",
                 request->head_buf_len - request->cursor);
        return -1;
    }

    request->cursor += used;
    return 0;
}

ssize_t flush_request_to_transport(http_transport_t *transport, http_buf_t *request)
{
    size_t used = snprintf(request->head_buf + request->cursor,
                           request->head_buf_len - request->cursor, "\r\n");

    if (used >= request->head_buf_len - request->cursor) {
        snprintf(err_buf, sizeof err_buf,
                 "remaining head_buf_len of %lu was less than required for final CRLF",
                 request->head_buf_len - request->cursor);
    }

    request->cursor += used;
    return transport->write(transport, request->head_buf, request->cursor);
}
