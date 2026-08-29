#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "http.h"
#include "common.h"
#include "json.h"

void make_example_plaintext_request(void)
{
    printf("\n---------------------- example plaintext request ----------------------\n");
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

    // TODO: parse content-length and read body (probably need like a
    // read_to_end function on buffered reader?)
}

// makes a request to https://opencode.ai/zen/go/v1/models and prints the id of
// every model returned in the JSON response, demonstrating the funtionality of
// both the HTTPS and JSON libraries.
void make_example_ssl_json_request(void)
{
    printf("\n---------------------- example ssl request ----------------------\n");
    http_transport_t transport;
    http_init_ssl_transport(&transport, SV_LIT("opencode.ai"), NULL);

    char scratch_buf[2048];
    http_client_t client = {
        .transport = &transport,
        .head_buf = scratch_buf,
        .head_buf_len = sizeof(scratch_buf),
        .cursor = 0,
    };

    http_write_request_line(&client, SV_LIT("GET"), SV_LIT("/zen/go/v1/models"));
    http_write_header(&client, SV_LIT("Host"), SV_LIT("opencode.ai"));
    http_flush_request(&client);

    char headers_buf[4096];
    http_response_parser_t parser;
    http_init_response_parser(&client, &parser, headers_buf, sizeof(headers_buf));
    http_receive_head(&parser);

    http_header_t header;
    long content_length = -1;
    while (http_header_next(&parser, &header) != -1)
        if (str_view_cmp_ci(&header.key, &SV_LIT("content-length"))) {
            char cl_buf[32];
            SV_AS_C_STR(header.value, cl_buf);
            content_length = strtol(cl_buf, NULL, 10);
        }

    char body[1 << 20];
    assert(content_length >= 0 && (size_t)content_length <= sizeof(body) &&
           "response body exceeds stack buffer");
    size_t got = (size_t)buffered_reader_read_all(&parser.reader, body, (size_t)content_length);

    // tokenize the JSON, printing the value of every "id" key
    json_tok_t tok;
    char str_buf[1024];
    char *p = body;
    size_t remaining = got;
    bool in_key = true, is_id = false;
    while (remaining > 0) {
        size_t adv = json_parse(p, remaining, &tok, str_buf, sizeof(str_buf));
        if (adv == 0 || tok.tok_type == JSON_TOK_INVALID)
            break;

        if (tok.tok_type == JSON_TOK_STR) {
            if (in_key)
                is_id = str_view_cmp(&tok.tok, &SV_LIT("id"));
            else if (is_id)
                printf("model id: %.*s\n", SV_FMT(tok.tok));
        }

        in_key = tok.tok_type == JSON_TOK_OBJECT_OPEN || tok.tok_type == JSON_TOK_COMMA;
        if (in_key)
            is_id = false;

        p += adv;
        remaining -= adv;
    }

    http_close_ssl_transport(&transport);
}

void test_response_parser_bounds_chunk_size(void)
{
    // The response parser reuses the client's head buffer as its reader's
    // buffer.  Even if that buffer is smaller than the default chunk size, the
    // parser must never create a malformed reader (chunk_size > buf_len).
    char scratch[32];
    http_transport_t transport = { 0 };
    http_client_t client = {
        .transport = &transport,
        .head_buf = scratch,
        .head_buf_len = sizeof(scratch),
        .cursor = 0,
    };

    char headers_buf[128];
    http_response_parser_t parser;
    http_init_response_parser(&client, &parser, headers_buf, sizeof(headers_buf));

    assert(parser.reader.chunk_size <= parser.reader.buf_len &&
           "reader chunk_size must not exceed its buffer length");
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
    TEST(test_response_parser_bounds_chunk_size);
    make_example_plaintext_request();
    make_example_ssl_json_request();
}
