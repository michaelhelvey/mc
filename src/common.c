#include "common.h"

ssize_t str_view_cmp(string_view_t *a, string_view_t *b)
{
    if (a->buf_len != b->buf_len) {
        return false;
    }

    for (size_t i = 0; i < a->buf_len; i++) {
        if (a->buf[i] != b->buf[i]) {
            return false;
        }
    }

    return true;
}
