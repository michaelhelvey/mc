#include "json.h"

#define _JSON_SINGLE_CHAR_RETURN(typ)    \
    out_token->tok.buf = input + cursor; \
    out_token->tok.len = 1;              \
    out_token->tok_type = typ;           \
    return cursor + 1;

#define _JSON_CURRENT_CHAR (char)input[cursor]
#define _JSON_ADVANCE_CURSOR_OR_THROW()         \
    if (cursor + 1 > input_len) {               \
        out_token->tok_type = JSON_TOK_INVALID; \
        return cursor;                          \
    }                                           \
    cursor++;

#define _JSON_EOF (cursor >= input_len)

size_t json_parse(char *input, size_t input_len, json_tok_t *out_token, char *str_buf,
                  size_t str_buf_cap)
{
    out_token->tok_type = JSON_TOK_INVALID;
    out_token->tok.buf = input;
    out_token->tok.len = 0;

    uintptr_t cursor = 0;
    while (cursor < input_len && isspace(_JSON_CURRENT_CHAR)) {
        _JSON_ADVANCE_CURSOR_OR_THROW();
    }

    if (cursor >= input_len) {
        return 0;
    }

    switch (_JSON_CURRENT_CHAR) {
    case '{': {
        _JSON_SINGLE_CHAR_RETURN(JSON_TOK_OBJECT_OPEN)
    }
    case '}': {
        _JSON_SINGLE_CHAR_RETURN(JSON_TOK_OBJECT_CLOSE)
    }
    case '[': {
        _JSON_SINGLE_CHAR_RETURN(JSON_TOK_LIST_OPEN)
    }
    case ']': {
        _JSON_SINGLE_CHAR_RETURN(JSON_TOK_LIST_CLOSE)
    }
    case ':': {
        _JSON_SINGLE_CHAR_RETURN(JSON_TOK_COLON)
    }
    case ',': {
        _JSON_SINGLE_CHAR_RETURN(JSON_TOK_COMMA)
    }
    default:
        break;
    }

    if (_JSON_CURRENT_CHAR == '"') {
        _JSON_ADVANCE_CURSOR_OR_THROW();
        size_t str_len = 0;
        out_token->tok.buf = str_buf;
        out_token->tok_type = JSON_TOK_STR;

        while (cursor < input_len && _JSON_CURRENT_CHAR != '"' && str_len < str_buf_cap) {
            if (_JSON_CURRENT_CHAR == '\\') {
                _JSON_ADVANCE_CURSOR_OR_THROW();
                str_buf[str_len] = _JSON_CURRENT_CHAR;
                str_len++;
                _JSON_ADVANCE_CURSOR_OR_THROW();
                continue;
            }

            str_buf[str_len] = _JSON_CURRENT_CHAR;
            str_len++;
            cursor++;
        }

        if (cursor >= input_len) {
            // ran off the end without finding a closing quote -- treat the
            // token as invalid rather than reporting a bogus advance past EOF.
            out_token->tok_type = JSON_TOK_INVALID;
            return 0;
        }

        out_token->tok.len = str_len;
        return cursor + 1; // consume final quotation mark
    }

    if (_JSON_CURRENT_CHAR == '-' || isnumber(_JSON_CURRENT_CHAR)) {
        out_token->tok_type = JSON_TOK_NUMBER;
        out_token->tok.buf = input + cursor;
        size_t num_len = 1;
        cursor++;

        // somewhat cheating here, because we're not actually interpreting the
        // numbers, just tokenizing them.  Any interpreter for the numbers
        // should handle the case of it not being a valid number.
        while (cursor < input_len && (ishexnumber(_JSON_CURRENT_CHAR) ||
                                      _JSON_CURRENT_CHAR == 'b' || _JSON_CURRENT_CHAR == 'x' ||
                                      _JSON_CURRENT_CHAR == 'o' || _JSON_CURRENT_CHAR == '.')) {
            num_len++;
            cursor++;
        }

        out_token->tok.len = num_len;
        return cursor;
    }

    // identifiers as a fallback case:
    size_t identifier_len = 0;
    while (cursor < input_len && isalnum(_JSON_CURRENT_CHAR)) {
        identifier_len++;
        cursor++;
    }

    // TODO: this could obviously be faster with interning
    out_token->tok.len = identifier_len;
    if (str_view_cmp(&out_token->tok, &SV_LIT("null"))) {
        out_token->tok_type = JSON_TOK_NULL;
    } else if (str_view_cmp(&out_token->tok, &SV_LIT("true"))) {
        out_token->tok_type = JSON_TOK_TRUE;
    } else if (str_view_cmp(&out_token->tok, &SV_LIT("false"))) {
        out_token->tok_type = JSON_TOK_FALSE;
    }

    return cursor;
}

// Consume a string literal whose opening quote is at input[0], honoring
// escape sequences (so a `\"` doesn't end the string).  Returns the number of
// bytes consumed including the closing quote, or 0 if the string is
// unterminated.
static size_t json_consume_string(char *input, size_t input_len)
{
    if (input_len < 2 || input[0] != '"') {
        return 0;
    }

    size_t cursor = 1;
    while (cursor < input_len) {
        if (input[cursor] == '\\') {
            cursor += 2;
        } else if (input[cursor] == '"') {
            return cursor + 1;
        } else {
            cursor++;
        }
    }

    return 0;
}

// Consume a container value by simply finding the matching closing token
static size_t json_consume_container(char *input, size_t input_len)
{
    uintptr_t depth = 0;
    size_t cursor = 0;

    while (cursor < input_len) {
        switch (input[cursor]) {
        case '"': {
            size_t consumed = json_consume_string(input + cursor, input_len - cursor);
            if (consumed == 0) {
                return 0;
            }
            cursor += consumed;
            continue;
        }
        case '{':
        case '[':
            depth++;
            break;
        case '}':
        case ']':
            if (depth == 0) {
                return 0;
            }
            depth--;
            if (depth == 0) {
                return cursor + 1;
            }
            break;
        default:
            break;
        }
        cursor++;
    }

    return 0;
}

// A scalar value ends as soon as we get to something that has semantic meaning
// in JSON -- comma, colon, list/object open close, etc.  So to consume a scalar
// value, we just have to scan until we hit one of those.
static size_t json_consume_scalar(char *input, size_t input_len)
{
    size_t cursor = 0;
    while (cursor < input_len) {
        char c = (char)input[cursor];
        if (isspace(c) || c == '\0' || c == ',' || c == ':' || c == ']' || c == '}' || c == '"' ||
            c == '{' || c == '[') {
            break;
        }
        cursor++;
    }
    return cursor;
}

size_t json_consume_value(char *input, size_t input_len)
{
    size_t cursor = 0;

    while (cursor < input_len && isspace((char)input[cursor])) {
        cursor++;
    }

    if (cursor >= input_len) {
        return 0;
    }

    char *rest = input + cursor;
    size_t rest_len = input_len - cursor;

    size_t value_len = 0;
    switch (rest[0]) {
    case '"':
        value_len = json_consume_string(rest, rest_len);
        break;
    case '{':
    case '[':
        value_len = json_consume_container(rest, rest_len);
        break;
    default:
        value_len = json_consume_scalar(rest, rest_len);
        break;
    }

    if (value_len == 0) {
        // we didn't find a value at all, such as if the buffer starts with a
        // comma or something
        return 0;
    }

    cursor += value_len;

    // consume trailing whitespace and/or delimiters so that the _next_ call to
    // `json_consume_value` starts at a valid value
    while (cursor < input_len && isspace((char)input[cursor])) {
        cursor++;
    }

    if (cursor < input_len && (input[cursor] == ',' || input[cursor] == ':')) {
        cursor++;
        while (cursor < input_len && isspace((char)input[cursor])) {
            cursor++;
        }
    }

    return cursor;
}
