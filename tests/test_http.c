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
    buffered_reader_t response_reader = http_create_response_reader(&client);
    ssize_t header_byte_len = http_receive_head(&response_reader, headers_buf, sizeof(headers_buf));

    if (header_byte_len <= 0) {
        assert(false && "expected to be able to read a response");
    }

    // http_receive_head only guarantees the first <header_byte_len> bytes of
    // headers_buf are valid; a caller that wants a view over the parsed head
    // constructs one from (headers_buf, <return value>).
    printf("[HTTP]: successfully read %lu bytes of headers\n", (size_t)header_byte_len);
    printf("[HTTP]: read headers:\n\n%.*s\n", (int)header_byte_len, headers_buf);
}

void test_header_iterator_simple(void)
{
    string_view_t example = SV_LIT("host: google.com\r\ncontent-length: 269\r\n\r\n");
    http_header_iterator_t iter = { .complete = false, .header_buf = example, .cursor = 0 };
    http_header_t header;

    assert(http_header_next(&iter, &header) == 0 && "expected to read one header");
    assert(str_view_cmp(&header.key, &SV_LIT("host")) && "expected key to equal 'host'");
    assert(str_view_cmp(&header.value, &SV_LIT("google.com")) &&
           "expected value to equal 'google.com'");

    assert(http_header_next(&iter, &header) == 0 && "expected to read second header");
    assert(str_view_cmp(&header.key, &SV_LIT("content-length")) &&
           "expected key to equal 'content-length'");
    assert(str_view_cmp(&header.value, &SV_LIT("269")) && "expected value to equal '269'");

    assert(http_header_next(&iter, &header) == -1 && "expected to reach end of headers");
}

void test_header_iterator_end(void)
{
    string_view_t example = SV_LIT("\r\n\r\n");
    http_header_iterator_t iter = { .complete = false, .header_buf = example, .cursor = 0 };
    http_header_t header;

    assert(http_header_next(&iter, &header) == -1 && "expected to not read a header");
    assert(iter.complete && "expected iterator to be in complete state");
}

#define TEST(t) \
    t();        \
    printf("[HTTP]: Passed: %s\n", #t);

void test_http_client(void)
{
    make_example_plaintext_request();
    TEST(test_header_iterator_simple);
    TEST(test_header_iterator_end);
}
