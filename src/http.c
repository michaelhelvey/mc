#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <ctype.h>

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

#define MC_MAX_DOMAIN_LEN 256

int http_init_plaintext_transport(http_transport_t *transport, string_view_t domain)
{
    memset(transport, 0, sizeof(http_transport_t));

    char domain_buf[MC_MAX_DOMAIN_LEN];
    if (!SV_AS_C_STR(domain, domain_buf)) {
        snprintf(err_buf, sizeof(err_buf), "domain of length %zu exceeds maximum of %zu",
                 domain.len, sizeof(domain_buf) - 1);
        return -1;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = PF_UNSPEC;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_DEFAULT;

    struct addrinfo *head0;
    int result = getaddrinfo(domain_buf, "http", &hints, &head0);
    if (result != 0) {
        snprintf(err_buf, sizeof(err_buf), "%s", gai_strerror(result));
        return -1;
    }

    int s = -1;
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
                 addr_count, domain_buf);
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

int http_write_request_line(http_client_t *client, string_view_t method, string_view_t path)
{
    client->cursor = 0;

    size_t used = snprintf(client->head_buf + client->cursor, client->head_buf_len - client->cursor,
                           "%.*s %.*s HTTP/1.1\r\n", SV_FMT(method), SV_FMT(path));
    if (used >= client->head_buf_len - client->cursor) {
        snprintf(err_buf, sizeof err_buf,
                 "remaining head_buf_len of %lu was less than required for request line",
                 client->head_buf_len - client->cursor);
        return -1;
    }

    client->cursor += used;
    return 0;
}

int http_write_header(http_client_t *client, string_view_t key, string_view_t value)
{
    size_t used = snprintf(client->head_buf + client->cursor, client->head_buf_len - client->cursor,
                           "%.*s: %.*s\r\n", SV_FMT(key), SV_FMT(value));
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
        return -1;
    }

    client->cursor += used;
    return client->transport->write(client->transport, client->head_buf, client->cursor);
}

buffered_reader_t http_create_response_reader(http_client_t *client)
{
    buffered_reader_t reader = buffered_reader_defaults_from(
        client->transport, client->transport->read, client->head_buf, client->head_buf_len);

    return reader;
}

int http_header_next(http_header_iterator_t *iter, http_header_t *out_header)
{
    if (iter->complete) {
        return -1;
    }

    if (iter->cursor + 2 > iter->header_buf.len) {
        iter->complete = true;
        return -1;
    }

    string_view_t window = SV_FROM(iter->header_buf.buf + iter->cursor, 2);
    if (str_view_cmp(&window, &SV_LIT("\r\n"))) {
        iter->complete = true;
        return -1;
    }

    size_t key_start = iter->cursor;
    out_header->key.buf = iter->header_buf.buf + iter->cursor;

    // find end of key
    while (iter->cursor < iter->header_buf.len && iter->header_buf.buf[iter->cursor] != ':') {
        iter->cursor++;
    }

    // if we ran off the end looking for a colon, then we have a malformed
    // headers section and there's no point continuing
    if (iter->cursor >= iter->header_buf.len) {
        iter->complete = true;
        return -1;
    }

    out_header->key.len = iter->cursor - key_start;
    iter->cursor++; // consume ':'

    // skip whitespace between key and value
    while (
        iter->cursor < iter->header_buf.len &&
        (iter->header_buf.buf[iter->cursor] == ' ' || iter->header_buf.buf[iter->cursor] == '\t')) {
        iter->cursor++;
    }

    // scan for final CRLF
    size_t value_start = iter->cursor;
    out_header->value.buf = iter->header_buf.buf + iter->cursor;
    while (iter->cursor < iter->header_buf.len && iter->header_buf.buf[iter->cursor] != '\r') {
        iter->cursor++;
    }
    out_header->value.len = iter->cursor - value_start;

    // consume final CRLF
    if (iter->cursor + 2 <= iter->header_buf.len) {
        iter->cursor += 2;
    } else {
        iter->cursor = iter->header_buf.len;
    }

    return 0;
}

ssize_t http_receive_head(buffered_reader_t *reader, char *head_buf, size_t head_buf_cap)
{
    ssize_t nread =
        buffered_reader_read_until(reader, SV_LIT("\r\n\r\n"), head_buf, head_buf_cap);
    if (nread < 0) {
        return nread;
    }

    return nread;
}
