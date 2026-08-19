#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "json.h"

#define GIVEN(buf)                \
    json_tok_t out_token;         \
    char *buffer = buf;           \
    size_t buf_len = sizeof(buf); \
    size_t str_buf_len = 1024;    \
    char str_buf[str_buf_len];    \
    size_t r_value = json_parse(buffer, buf_len, &out_token, (char *)str_buf, str_buf_len);

static void test_tokenize_true(void)
{
    GIVEN("true");
    assert(str_view_cmp(&out_token.tok, &SV_LIT("true")) == true &&
           "expected identifier to be 'true'");
    assert(out_token.tok_type == JSON_TOK_TRUE && "expected type of token to be JSON_TOK_TRUE");
    assert(out_token.tok.buf_len == 4 && "expectd length of 'true' to be 4");
    assert(r_value == 4 && "expected to consume 4 characters for 'true'");
}

static void test_tokenize_false(void)
{
    GIVEN("false");
    assert(str_view_cmp(&out_token.tok, &SV_LIT("false")) && "expected identifier to be false");
    assert(out_token.tok_type == JSON_TOK_FALSE && "expected type of token to be JSON_TOK_FALSE");
    assert(out_token.tok.buf_len == 5 && "expected length of 'false' to be 5");
    assert(r_value = 4 && "expected to consume 4 characters for 'false'");
}

static void test_tokenize_null(void)
{
    GIVEN("null");
    assert(str_view_cmp(&out_token.tok, &SV_LIT("null")) == true &&
           "expected identifier to be 'null'");
    assert(out_token.tok_type == JSON_TOK_NULL && "expected type of token to be JSON_TOK_null");
    assert(out_token.tok.buf_len == 4 && "expectd length of 'null' to be 4");
    assert(r_value == 4 && "expected to consume 4 characters for 'null'");
}

static void test_simple_number(void)
{
    GIVEN("123");
    assert(str_view_cmp(&out_token.tok, &SV_LIT("123")) == true && "expected number to be 123");
    assert(out_token.tok.buf_len == 3 && "expected number to be 3 bytes long");
    assert(out_token.tok_type == JSON_TOK_NUMBER && "expected output type to be a number");
    assert(r_value == 3 && "expected to tokenize 3 byte number");
}

static void test_negative_hex_number(void)
{
    GIVEN("-0x123");
    assert(str_view_cmp(&out_token.tok, &SV_LIT("-0x123")) == true &&
           "expected number to be -0x123");
    assert(out_token.tok.buf_len == 6 && "expected number to be 3 bytes long");
    assert(out_token.tok_type == JSON_TOK_NUMBER && "expected output type to be a number");
    assert(r_value == 6 && "expected to tokenize 6 byte number");
}

static void test_skips_whitespace(void)
{
    GIVEN("  {");
    assert(r_value == 3 && "expected to skip whitespace and return opening bracket");
}

static void test_tokenizes_string(void)
{
    GIVEN("\"some_string\"");
    assert(r_value == 13 && "expected string + quotation marks to be 13 bytes");
    assert(str_view_cmp(&out_token.tok, &SV_LIT("some_string")) == true &&
           "expected to parse some_string");
}

static void test_honors_escape_characters(void)
{
    GIVEN("\"some\\\"_string\"");
    assert(r_value == 15 && "expected string + quotation marks to be 15 bytes with escapes");
    assert(out_token.tok.buf_len == 12 && "expected actual parsed string length to be 12");
    assert(str_view_cmp(&out_token.tok, &SV_LIT("some\"_string")) == true &&
           "expected to parse some\"_string");
}

static void test_silly_escapes(void)
{
    // a json string containing 3 quotation marks
    GIVEN("\"\\\"\\\"\\\"\"");
    assert(r_value == 8 && "consumed should be 8");
    assert(out_token.tok.buf_len == 3 && "buf_len = 3");
    assert(str_view_cmp(&out_token.tok, &SV_LIT("\"\"\"")) &&
           "should have parsed 3 quotation marks");
}

#define TEST(t) \
    t();        \
    printf("Passed: %s\n", #t);

void test_json_parser(void)
{
    // strings:
    TEST(test_skips_whitespace);
    TEST(test_tokenizes_string);
    TEST(test_honors_escape_characters);
    TEST(test_silly_escapes);

    // numbers:
    TEST(test_simple_number);
    TEST(test_negative_hex_number);

    // identifiers
    TEST(test_tokenize_true);
    TEST(test_tokenize_false);
    TEST(test_tokenize_null);
}
