#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "http.h"
#include "common.h"

void make_example_plaintext_request(void)
{
    http_transport_t transport;
    // surely google doesn't mind a little spam in my tests, right...right...
    http_init_plaintext_transport(&transport, SV_LIT("google.com"));

    char scratch_buf[2048];
    http_client_t client = {
        .transport = &transport,
        .head_buf = scratch_buf,
        .head_buf_len = sizeof(scratch_buf),
        .cursor = 0,
    };

    if (http_write_request_line(&client, SV_LIT("GET"), SV_LIT("/")) != 0) {
        assert(false && "expected to be able to write request line");
    }
    if (http_write_header(&client, SV_LIT("Host"), SV_LIT("google.com")) != 0) {
        assert(false && "expected to be able to write a Host header");
    }

    ssize_t written = http_flush_request(&client);
    if (written <= 0) {
        assert(false && "expected to be able to write out request head to socket");
    }

    printf("[HTTP]: successfully wrote %lu bytes to socket\n", (size_t)written);

    char headers_buf[2048];
    http_response_parser_t response_parser;
    http_init_response_parser(&client, &response_parser, headers_buf, sizeof(headers_buf));

    ssize_t head_bytes = http_receive_head(&response_parser);
    if (head_bytes <= 0) {
        assert(false && "could not read response head");
    }

    string_view_t statusline = http_response_read_statusline(&response_parser);
    printf("[HTTP]: statusline: %.*s\n", SV_FMT(statusline));

    http_header_t header;
    while (http_header_next(&response_parser, &header) != -1) {
        printf("[HTTP]: read header: key = %.*s, value = %.*s\n", SV_FMT(header.key),
               SV_FMT(header.value));
    }

    // TODO: parse content-length & parse some json or something
}

void test_header_iterator_simple(void)
{
    string_view_t example =
        SV_LIT("HTTP/1.1 200 OK\r\nhost: google.com\r\ncontent-length: 269\r\n\r\n");

    // doesn't matter, not used
    buffered_reader_t mock_reader =
        buffered_reader_defaults_from(NULL, NULL, (char *)example.buf, example.len);
    http_response_parser_t parser = {
        .headers_cursor = 0,
        .head_buf = (char *)example.buf,
        .head_buf_len = example.len,
        .head_buf_view = example,
        .headers_complete = false,
        .reader = mock_reader,
    };

    string_view_t statusline = http_response_read_statusline(&parser);
    assert(str_view_cmp(&statusline, &SV_LIT("HTTP/1.1 200 OK")) && "expected to read stausline");

    http_header_t header;
    assert(http_header_next(&parser, &header) == 0 && "expected to read one header");
    assert(str_view_cmp(&header.key, &SV_LIT("host")) && "expected key to equal 'host'");
    assert(str_view_cmp(&header.value, &SV_LIT("google.com")) &&
           "expected value to equal 'google.com'");

    assert(http_header_next(&parser, &header) == 0 && "expected to read second header");
    assert(str_view_cmp(&header.key, &SV_LIT("content-length")) &&
           "expected key to equal 'content-length'");
    assert(str_view_cmp(&header.value, &SV_LIT("269")) && "expected value to equal '269'");

    assert(http_header_next(&parser, &header) == -1 && "expected to reach end of headers");
}

#define TEST(t) \
    t();        \
    printf("[HTTP]: Passed: %s\n", #t);

void test_http_client(void)
{
    TEST(test_header_iterator_simple);
    make_example_plaintext_request();
}
