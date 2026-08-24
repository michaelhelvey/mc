#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>

#include "http.h"
#include "common.h"

char err_buf[256];

const char *http_get_error(void)
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

int http_init_plaintext_transport(http_transport_t *transport, const char *domain)
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

int http_close_plaintext_transport(http_transport_t *transport)
{
    assert(transport->ctx_type == HTTP_CTX_SOCKET);
    close(transport->ctx.socket);
    return 0;
}

int http_write_request_line(http_client_t *client, const char *method, const char *path)
{
    client->cursor = 0;

    size_t used = snprintf(client->head_buf + client->cursor, client->head_buf_len - client->cursor,
                           "%s %s HTTP/1.1\r\n", method, path);
    if (used >= client->head_buf_len - client->cursor) {
        snprintf(err_buf, sizeof err_buf,
                 "remaining head_buf_len of %lu was less than required for request line",
                 client->head_buf_len - client->cursor);
        return -1;
    }

    client->cursor += used;
    return 0;
}

int http_write_header(http_client_t *client, const char *key, const char *value)
{
    size_t used = snprintf(client->head_buf + client->cursor, client->head_buf_len - client->cursor,
                           "%s: %s\r\n", key, value);
    if (used >= client->head_buf_len - client->cursor) {
        snprintf(err_buf, sizeof err_buf,
                 "remaining head_buf_len of %lu was less than required for header line",
                 client->head_buf_len - client->cursor);
        return -1;
    }

    client->cursor += used;
    return 0;
}

ssize_t http_flush_request(http_client_t *client)
{
    size_t used =
        snprintf(client->head_buf + client->cursor, client->head_buf_len - client->cursor, "\r\n");

    if (used >= client->head_buf_len - client->cursor) {
        snprintf(err_buf, sizeof err_buf,
                 "remaining head_buf_len of %lu was less than required for final CRLF",
                 client->head_buf_len - client->cursor);
    }

    client->cursor += used;
    return client->transport->write(client->transport, client->head_buf, client->cursor);
}

// named of course after the perfectly named correllary in the zig std library.
// it would be criminal to name this function anything else
ssize_t http_receive_head(http_client_t *client, char *headers_buf, size_t headers_buf_len)
{
    buffered_reader_t reader = buffered_reader_defaults_from(
        client->transport, client->transport->read, client->head_buf, client->head_buf_len);

    ssize_t result =
        buffered_reader_read_until(&reader, &SV_LIT("\r\n\r\n"), headers_buf, headers_buf_len);

    return result;
}
