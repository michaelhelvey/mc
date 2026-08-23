#include "json.h"

#define _JSON_SINGLE_CHAR_RETURN(typ)  \
    out_token->tok.buf = buf + cursor; \
    out_token->tok.buf_len = 1;        \
    out_token->tok_type = typ;         \
    return cursor + 1;

#define _JSON_CURRENT_CHAR (char)buf[cursor]
#define _JSON_ADVANCE_CURSOR_OR_THROW()         \
    if (cursor + 1 > buf_len) {                 \
        out_token->tok_type = JSON_TOK_INVALID; \
        return cursor;                          \
    }                                           \
    cursor++;

#define _JSON_EOF (cursor >= buf_len)

size_t json_parse(char *buf, size_t buf_len, json_tok_t *out_token, char *str_buf,
                  size_t str_buf_len)
{
    out_token->tok_type = JSON_TOK_INVALID;
    out_token->tok.buf = buf;
    out_token->tok.buf_len = 0;

    uintptr_t cursor = 0;
    while (isspace(_JSON_CURRENT_CHAR)) {
        _JSON_ADVANCE_CURSOR_OR_THROW();
    }

    switch (_JSON_CURRENT_CHAR) {
    case '{': {
        _JSON_SINGLE_CHAR_RETURN(JSON_TOK_OBJECT_OPEN)
        break;
    }
    case '}': {
        _JSON_SINGLE_CHAR_RETURN(JSON_TOK_OBJECT_CLOSE)
        break;
    }
    case '[': {
        _JSON_SINGLE_CHAR_RETURN(JSON_TOK_LIST_OPEN)
        break;
    }
    case ']': {
        _JSON_SINGLE_CHAR_RETURN(JSON_TOK_LIST_CLOSE)
        break;
    }
    case ':': {
        _JSON_SINGLE_CHAR_RETURN(JSON_TOK_COLON)
        break;
    }
    case ',': {
        _JSON_SINGLE_CHAR_RETURN(JSON_TOK_COMMA)
        break;
    }
    }

    if (_JSON_CURRENT_CHAR == '"') {
        _JSON_ADVANCE_CURSOR_OR_THROW();
        size_t str_len = 0;
        out_token->tok.buf = str_buf;
        out_token->tok_type = JSON_TOK_STR;

        while (_JSON_CURRENT_CHAR != '"' && str_len < str_buf_len) {
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

        out_token->tok.buf_len = str_len;
        return cursor + 1; // consume final quotation mark
    }

    if (_JSON_CURRENT_CHAR == '-' || isnumber(_JSON_CURRENT_CHAR)) {
        out_token->tok_type = JSON_TOK_NUMBER;
        out_token->tok.buf = buf + cursor;
        size_t num_len = 1;
        cursor++;

        // somewhat cheating here, because we're not actually interpreting the
        // numbers, just tokenizing them.  Any interpreter for the numbers
        // should handle the case of it not being a valid number.
        while ((ishexnumber(_JSON_CURRENT_CHAR) || _JSON_CURRENT_CHAR == 'b' ||
                _JSON_CURRENT_CHAR == 'x' || _JSON_CURRENT_CHAR == 'o' ||
                _JSON_CURRENT_CHAR == '.') &&
               !_JSON_EOF) {
            num_len++;
            cursor++;
        }

        out_token->tok.buf_len = num_len;
        return cursor;
    }

    // identifiers as a fallback case:
    size_t identifier_len = 0;
    while (isalnum(_JSON_CURRENT_CHAR) && !_JSON_EOF) {
        identifier_len++;
        cursor++;
    }

    // TODO: this could obviously be faster with interning
    out_token->tok.buf_len = identifier_len;
    if (str_view_cmp(&out_token->tok, &SV_LIT("null"))) {
        out_token->tok_type = JSON_TOK_NULL;
    } else if (str_view_cmp(&out_token->tok, &SV_LIT("true"))) {
        out_token->tok_type = JSON_TOK_TRUE;
    } else if (str_view_cmp(&out_token->tok, &SV_LIT("false"))) {
        out_token->tok_type = JSON_TOK_FALSE;
    }

    return cursor;
}

// Consume a string literal whose opening quote is at buf[0], honoring escape
// sequences (so a `\"` doesn't end the string).  Returns the number of bytes
// consumed including the closing quote, or 0 if the string is unterminated.
static size_t json_consume_string(char *buf, size_t buf_len)
{
    if (buf_len < 2 || buf[0] != '"') {
        return 0;
    }

    size_t cursor = 1;
    while (cursor < buf_len) {
        if (buf[cursor] == '\\') {
            cursor += 2;
        } else if (buf[cursor] == '"') {
            return cursor + 1;
        } else {
            cursor++;
        }
    }

    return 0;
}

// Consume a container value by simply finding the matching closing token
static size_t json_consume_container(char *buf, size_t buf_len)
{
    uintptr_t depth = 0;
    size_t cursor = 0;

    while (cursor < buf_len) {
        switch (buf[cursor]) {
        case '"': {
            size_t consumed = json_consume_string(buf + cursor, buf_len - cursor);
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
static size_t json_consume_scalar(char *buf, size_t buf_len)
{
    size_t cursor = 0;
    while (cursor < buf_len) {
        char c = (char)buf[cursor];
        if (isspace(c) || c == '\0' || c == ',' || c == ':' || c == ']' || c == '}' || c == '"' ||
            c == '{' || c == '[') {
            break;
        }
        cursor++;
    }
    return cursor;
}

size_t json_consume_value(char *buf, size_t buf_len)
{
    size_t cursor = 0;

    while (cursor < buf_len && isspace((char)buf[cursor])) {
        cursor++;
    }

    if (cursor >= buf_len) {
        return 0;
    }

    size_t value_len = 0;
    switch (buf[cursor]) {
    case '"':
        value_len = json_consume_string(buf + cursor, buf_len - cursor);
        break;
    case '{':
    case '[':
        value_len = json_consume_container(buf + cursor, buf_len - cursor);
        break;
    default:
        value_len = json_consume_scalar(buf + cursor, buf_len - cursor);
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
    while (cursor < buf_len && isspace((char)buf[cursor])) {
        cursor++;
    }

    if (cursor < buf_len && (buf[cursor] == ',' || buf[cursor] == ':')) {
        cursor++;
        while (cursor < buf_len && isspace((char)buf[cursor])) {
            cursor++;
        }
    }

    return cursor;
}
