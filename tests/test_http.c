#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "http.h"

void test_http_client(void)
{
    http_transport_t transport;
    // surely google doesn't mind a little spam in my tests, right...right...
    init_plaintext_transport(&transport, "google.com");

    char header_buf[2048];
    http_buf_t request = {
        .head_buf = header_buf,
        .head_buf_len = sizeof(header_buf),
        .cursor = 0,
    };

    if (write_request_line(&request, "GET", "/") != 0) {
        assert(false && "expected to be able to write request line");
    }
    if (write_header(&request, "Host", "google.com") != 0) {
        assert(false && "expectd to be able to write a Host header");
    }

    ssize_t written = flush_request_to_transport(&transport, &request);
    if (written <= 0) {
        assert(false && "expected to be able to write out request head to socket");
    }

    printf("[HTTP]: successfully wrote %lu bytes to socket\n", (size_t)written);
}
