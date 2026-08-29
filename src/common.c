#include <assert.h>
#include <string.h>
#include <ctype.h>

#include "common.h"

bool str_view_cmp(const string_view_t *a, const string_view_t *b)
{
    return a->len == b->len && memcmp(a->buf, b->buf, a->len) == 0;
}

bool str_view_cmp_ci(const string_view_t *a, const string_view_t *b)
{
    if (a->len != b->len)
        return false;

    for (size_t i = 0; i < a->len; i++) {
        if (tolower((unsigned char)a->buf[i]) != tolower((unsigned char)b->buf[i]))
            return false;
    }

    return true;
}

bool sv_to_c_str(string_view_t sv, char *out, size_t out_cap)
{
    if (out_cap == 0) {
        return false;
    }

    // An empty view is always representable as a NUL-terminated empty string,
    // regardless of whether sv.buf is NULL.
    if (sv.len == 0) {
        out[0] = '\0';
        return true;
    }

    assert(sv.buf != NULL && "string_view_t with non-zero length must have a non-NULL buf");

    size_t copy_len = sv.len < out_cap - 1 ? sv.len : out_cap - 1;
    memcpy(out, sv.buf, copy_len);
    out[copy_len] = '\0';

    return sv.len <= out_cap - 1;
}

ssize_t buffered_reader_read_nbytes(buffered_reader_t *reader, char *out, size_t out_cap, size_t n)
{
    assert(reader != NULL && out != NULL && reader->read != NULL &&
           "buffered_reader_t must be properly initialized before attempting to read out of it");
    assert(reader->chunk_size <= reader->buf_len &&
           "a well-formed reader's chunk_size must not exceed its buffer length");
    assert(out_cap >= n && "destination out must have capacity of at least n bytes");

    if (n > reader->chunk_size) {
        return -1;
    }

    for (;;) {
        size_t available_bytes_count = reader->write_cursor - reader->read_cursor;

        // 1) We already have all the data we need in our internal buffer, so
        // we can just return it:
        if (available_bytes_count >= n) {
            memcpy(out, reader->buf + reader->read_cursor, n);
            reader->read_cursor += n;
            return (ssize_t)n;
        }

        // 2) We don't have all the data we need, but our underlying source is
        // not readable anymore, so we just return what we have (possibly 0):
        if (reader->readable == false) {
            if (available_bytes_count == 0) {
                return 0;
            }
            memcpy(out, reader->buf + reader->read_cursor, available_bytes_count);
            reader->read_cursor += available_bytes_count;
            return (ssize_t)available_bytes_count;
        }

        // 3) We don't have all the data that we need, but we can read more.
        // first compact the buffer
        if (reader->write_cursor + reader->chunk_size > reader->buf_len) {
            memmove(reader->buf, reader->buf + reader->read_cursor, available_bytes_count);
            reader->read_cursor = 0;
            reader->write_cursor = available_bytes_count;
        }

        // write_cursor can be non-zero after compaction (leftover bytes we
        // haven't yet consumed), so even when chunk_size <= buf_len, a full
        // chunk might not fit.  so clamp the read len to whatever we actually
        // have (up to chunk_size as a default).
        size_t read_len = reader->chunk_size;
        size_t free_room = reader->buf_len - reader->write_cursor;
        if (read_len > free_room)
            read_len = free_room;

        ssize_t result = reader->read(reader->ctx, reader->buf + reader->write_cursor, read_len);

        if (result < 0) {
            return -1;
        } else if (result == 0) {
            // EOF.  Mark as unreadable and try again above
            reader->readable = false;
            continue;
        }

        // we read at least 1 byte, so advance the write_cursor
        reader->write_cursor += (size_t)result;
    }
}

ssize_t buffered_reader_read_all(buffered_reader_t *reader, char *out, size_t out_cap)
{
    assert(reader != NULL && out != NULL && reader->read != NULL &&
           "buffered_reader_t must be properly initialized before attempting to read out of it");

    size_t total = 0;
    while (total < out_cap) {
        size_t want = reader->chunk_size;
        if (want > out_cap - total)
            want = out_cap - total;

        ssize_t n = buffered_reader_read_nbytes(reader, out + total, out_cap - total, want);
        if (n < 0)
            return -1;
        if (n == 0)
            break; // EOF, nothing more to read

        total += (size_t)n;
    }

    return (ssize_t)total;
}

int buffered_reader_unread(buffered_reader_t *reader, size_t n)
{
    size_t available = reader->write_cursor - reader->read_cursor;
    if (n > available) {
        return -1;
    }
    reader->read_cursor -= n;
    return 0;
}

ssize_t buffered_reader_read_until(buffered_reader_t *reader, string_view_t needle, char *out,
                                   size_t out_cap)
{
    size_t total = 0;
    size_t check_pos = 0;

    for (;;) {
        if (total + needle.len > out_cap) {
            return -1;
        }

        ssize_t result =
            buffered_reader_read_nbytes(reader, out + total, out_cap - total, needle.len);
        if (result <= 0) {
            return result;
        }

        total += (size_t)result;

        if (total < needle.len) {
            continue;
        }

        size_t end = total - needle.len;
        for (; check_pos <= end; check_pos++) {
            if (memcmp(out + check_pos, needle.buf, needle.len) == 0) {
                // push back any extra bytes that we read past the match and
                // return our total
                size_t overread = total - (check_pos + needle.len);
                buffered_reader_unread(reader, overread);
                return (ssize_t)(check_pos + needle.len);
            }
        }
    }
}
