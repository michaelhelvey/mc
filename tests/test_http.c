#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "http.h"
#include "common.h"

void test_http_client(void)
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
        assert(false && "expectd to be able to write a Host header");
    }

    ssize_t written = http_flush_request(&client);
    if (written <= 0) {
        assert(false && "expected to be able to write out request head to socket");
    }

    printf("[HTTP]: successfully wrote %lu bytes to socket\n", (size_t)written);
    char headers_buf[2048];
    ssize_t header_byte_len = http_receive_head(&client, SV_FROM(headers_buf, sizeof(headers_buf)));

    if (header_byte_len <= 0) {
        assert(false && "expected to be able to read a response");
    }

    printf("[HTTP]: successfully read %lu bytes of headers\n", (size_t)header_byte_len);
    string_view_t headers = SV_FROM(headers_buf, header_byte_len);
    printf("[HTTP]: read headers:\n\n%.*s\n", SV_FMT(headers));
}
