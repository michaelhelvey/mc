#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "json.h"

#define GIVEN(buf)                                              \
    json_tok_t out_token;                                       \
    char *buffer = buf;                                         \
    char str_buf[1024];                                         \
    size_t r_value = json_parse(buffer, sizeof(buf), &out_token, str_buf, sizeof(str_buf));

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

static void test_tokenize_comma(void)
{
    GIVEN(",");
    assert(out_token.tok_type == JSON_TOK_COMMA && "expected token type to be JSON_TOK_COMMA");
    assert(r_value == 1 && "expected to consume 1 character for ','");
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

static void test_consume_scalar_value(void)
{
    assert(json_consume_value((char *)"123", 3) == 3 && "expected to consume '123'");
}

static void test_consume_string_skips_delimiter(void)
{
    char *buf = "\"first\", \"second\"";
    size_t offset = json_consume_value(buf, strlen(buf));
    assert(offset == 9 && "expected to skip string, comma, and whitespace");
    assert(buf[offset] == '"' && "expected to land on the start of the second string");
}

static void test_consume_walks_key_value_pairs(void)
{
    // positioned at the first key, like the caller would be after consuming '{'
    char *buf = "\"a\": 1, \"b\": [2,3]";

    // each call consumes a key + ':' or value + ','
    size_t offset = json_consume_value(buf, strlen(buf));
    assert(buf[offset] == '1' && "key + ':' should land at the value");
    offset += json_consume_value(buf + offset, strlen(buf) - offset);
    assert(buf[offset] == '"' && "value + ',' should land at the next key");
    offset += json_consume_value(buf + offset, strlen(buf) - offset);
    assert(buf[offset] == '[' && "key + ':' should land at the array value");
}

static void test_consume_to_third_item_in_array(void)
{
    char *buf = "[  1, {\"a\":[true,false], \"b\":{\"c\":null}},  \"third\" ]";
    size_t buf_len = strlen(buf);
    json_tok_t out_token;
    char str_buf[1024];

    size_t offset = json_parse(buf, buf_len, &out_token, str_buf, 1024); // consume '['
    assert(out_token.tok_type == JSON_TOK_LIST_OPEN && "expected to open the array");

    offset += json_consume_value(buf + offset, buf_len - offset); // skip item 1
    offset += json_consume_value(buf + offset, buf_len - offset); // skip item 2 (nested)

    size_t r = json_parse(buf + offset, buf_len - offset, &out_token, str_buf, 1024);
    assert(r > 0 && "expected to read a token at the 3rd item");
    assert(out_token.tok_type == JSON_TOK_STR && "expected the 3rd item to be a string");
    assert(str_view_cmp(&out_token.tok, &SV_LIT("third")) && "expected the 3rd item to be 'third'");
}

static void test_consume_ignores_brackets_in_strings(void)
{
    char *buf = "{\"k\": \"[not, an {item]\"}, true";
    json_tok_t out_token;
    char str_buf[1024];

    size_t offset = json_consume_value(buf, strlen(buf));
    assert(buf[offset] == 't' && "expected to land on 'true' despite brackets in the string");
    size_t r = json_parse(buf + offset, strlen(buf) - offset, &out_token, str_buf, 1024);
    assert(r > 0 && "expected to parse a token after the skip");
    assert(out_token.tok_type == JSON_TOK_TRUE && "expected 'true' token after the skip");
}

static void test_consume_errors(void)
{
    assert(json_consume_value((char *)"", 0) == 0 && "empty buffer returns 0");
    assert(json_consume_value((char *)"   ", 3) == 0 && "whitespace-only buffer returns 0");
    assert(json_consume_value((char *)"]", 1) == 0 && "leading delimiter returns 0");
    assert(json_consume_value((char *)"{ \"a\": 1", 8) == 0 && "unterminated object returns 0");
    assert(json_consume_value((char *)"\"abc", 4) == 0 && "unterminated string returns 0");
}

// no NUL-terminated string tests:

static void test_parse_empty_input(void)
{
    json_tok_t tok;
    char str_buf[16];
    assert(json_parse((char *)"", 0, &tok, str_buf, sizeof(str_buf)) == 0);
    assert(tok.tok_type == JSON_TOK_INVALID && "empty input produces INVALID token");
}

static void test_parse_whitespace_only(void)
{
    json_tok_t tok;
    char str_buf[16];
    assert(json_parse((char *)"   ", 3, &tok, str_buf, sizeof(str_buf)) == 0);
    assert(tok.tok_type == JSON_TOK_INVALID && "whitespace-only input produces INVALID token");
}

static void test_parse_unterminated_string(void)
{
    json_tok_t tok;
    char str_buf[16];
    assert(json_parse((char *)"\"abc", 4, &tok, str_buf, sizeof(str_buf)) == 0);
    assert(tok.tok_type == JSON_TOK_INVALID && "unterminated string produces INVALID token");
}

static void test_parse_identifier_at_eob(void)
{
    json_tok_t tok;
    char str_buf[16];
    assert(json_parse((char *)"null", 4, &tok, str_buf, sizeof(str_buf)) == 4);
    assert(tok.tok_type == JSON_TOK_NULL && "identifier at end-of-buffer is recognized");
}

static void test_parse_number_at_eob(void)
{
    json_tok_t tok;
    char str_buf[16];
    assert(json_parse((char *)"123", 3, &tok, str_buf, sizeof(str_buf)) == 3);
    assert(tok.tok_type == JSON_TOK_NUMBER && "number at end-of-buffer is recognized");
}

static void test_parse_subview_of_larger_buffer(void)
{
    // test that the tokenizer doesn't read past the input view's length by
    // parsing a "true" in the midst of a bunch of other non-NUL characters
    char big[64];
    memset(big, 'X', sizeof(big));
    static const char needle[] = "true";
    size_t offset = 17; // arbitrary position inside the 'X' padding
    memcpy(big + offset, needle, sizeof(needle));

    json_tok_t tok;
    char str_buf[16];
    assert(json_parse(big + offset, 4, &tok, str_buf, sizeof(str_buf)) == 4);
    assert(tok.tok_type == JSON_TOK_TRUE && "true within non-NUL-terminated buffer is recognized");
    // if we returned successfully, that means we didn't read into the X's or
    // else we'd have gotten an invalid token error
}

#define TEST(t) \
    t();        \
    printf("[JSON]: Passed: %s\n", #t);

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

    // delimiters:
    TEST(test_tokenize_comma);

    // value skipping:
    TEST(test_consume_scalar_value);
    TEST(test_consume_string_skips_delimiter);
    TEST(test_consume_walks_key_value_pairs);
    TEST(test_consume_to_third_item_in_array);
    TEST(test_consume_ignores_brackets_in_strings);
    TEST(test_consume_errors);

    // non-NUL-terminated inputs (these used to pass by accident because of an
    // implicit trailing NUL; these tests pass an explicit length along with the
    // buffer):
    TEST(test_parse_empty_input);
    TEST(test_parse_whitespace_only);
    TEST(test_parse_unterminated_string);
    TEST(test_parse_identifier_at_eob);
    TEST(test_parse_number_at_eob);
    TEST(test_parse_subview_of_larger_buffer);
}
