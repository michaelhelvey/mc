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

void http_init_response_parser(http_client_t *client, http_response_parser_t *in_parser,
                               char *head_buf, size_t head_buf_len)
{
    buffered_reader_t reader = buffered_reader_defaults_from(
        client->transport, client->transport->read, client->head_buf, client->head_buf_len);

    in_parser->reader = reader;
    in_parser->head_buf = head_buf;
    in_parser->head_buf_len = head_buf_len;
    in_parser->headers_complete = false;
    in_parser->headers_cursor = 0;
    in_parser->head_buf_view = SV_FROM(NULL, 0);
}

ssize_t http_receive_head(http_response_parser_t *response_parser)
{
    ssize_t nread = buffered_reader_read_until(&response_parser->reader, SV_LIT("\r\n\r\n"),
                                               response_parser->head_buf,
                                               response_parser->head_buf_len);
    if (nread < 0) {
        return nread;
    }

    response_parser->head_buf_view = SV_FROM(response_parser->head_buf, nread);
    return nread;
}

int http_header_next(http_response_parser_t *iter, http_header_t *out_header)
{
    if (iter->head_buf_view.buf == NULL) {
        return -1;
    }

    if (iter->headers_complete) {
        return -1;
    }

    if (iter->headers_cursor + 2 > iter->head_buf_view.len) {
        iter->headers_complete = true;
        return -1;
    }

    string_view_t window = SV_FROM(iter->head_buf_view.buf + iter->headers_cursor, 2);
    if (str_view_cmp(&window, &SV_LIT("\r\n"))) {
        iter->headers_complete = true;
        return -1;
    }

    size_t key_start = iter->headers_cursor;
    out_header->key.buf = iter->head_buf_view.buf + iter->headers_cursor;

    // find end of key
    while (iter->headers_cursor < iter->head_buf_view.len &&
           iter->head_buf_view.buf[iter->headers_cursor] != ':') {
        iter->headers_cursor++;
    }

    // if we ran off the end looking for a colon, then we have a malformed
    // headers section and there's no point continuing
    if (iter->headers_cursor >= iter->head_buf_view.len) {
        iter->headers_complete = true;
        return -1;
    }

    out_header->key.len = iter->headers_cursor - key_start;
    iter->headers_cursor++; // consume ':'

    // skip whitespace between key and value
    while (iter->headers_cursor < iter->head_buf_view.len &&
           (iter->head_buf_view.buf[iter->headers_cursor] == ' ' ||
            iter->head_buf_view.buf[iter->headers_cursor] == '\t')) {
        iter->headers_cursor++;
    }

    // scan for final CRLF
    size_t value_start = iter->headers_cursor;
    out_header->value.buf = iter->head_buf_view.buf + iter->headers_cursor;
    while (iter->headers_cursor < iter->head_buf_view.len &&
           iter->head_buf_view.buf[iter->headers_cursor] != '\r') {
        iter->headers_cursor++;
    }
    out_header->value.len = iter->headers_cursor - value_start;

    // consume final CRLF
    if (iter->headers_cursor + 2 <= iter->head_buf_view.len) {
        iter->headers_cursor += 2;
    } else {
        iter->headers_cursor = iter->head_buf_view.len;
    }

    return 0;
}

string_view_t http_response_read_statusline(http_response_parser_t *response_parser)
{
    size_t cursor = 0;
    size_t len = response_parser->head_buf_view.len;
    while (cursor + 1 < len &&
           !(response_parser->head_buf_view.buf[cursor] == '\r' &&
             response_parser->head_buf_view.buf[cursor + 1] == '\n')) {
        cursor++;
    }

    string_view_t result = SV_FROM(response_parser->head_buf_view.buf, cursor);

    // advance our internal view of the head buffer to the start of the first
    // header so that when the user requests the next header, we're ready to go.
    // guard against degenerate cases where the statusline is missing its CRLF
    // terminator so we never run off the end of the internal buffer.
    if (cursor < len && response_parser->head_buf_view.buf[cursor] == '\r') {
        cursor += 2; // consume the '\r\n' terminator
        if (cursor > len) {
            cursor = len;
        }
    }

    response_parser->head_buf_view.buf += cursor;
    response_parser->head_buf_view.len -= cursor;

    return result;
}
