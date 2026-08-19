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

    // FIXME: this could obviously be faster with interning
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
