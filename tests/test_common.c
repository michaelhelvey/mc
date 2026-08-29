#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#define TEST(t) \
    t();        \
    printf("[COMMON]: Passed: %s\n", #t);

#define GIVEN_READER(name, data)                    \
    char name##_buf[sizeof(data) - 1] = data;       \
    buffered_reader_t name = {                      \
        .ctx = (void *)NULL,                        \
        .buf = name##_buf,                          \
        .buf_len = sizeof(name##_buf),              \
        .readable = true,                           \
        .read = (buffered_reader_read_fn)mock_read, \
        .chunk_size = sizeof(name##_buf),           \
        .read_cursor = 0,                           \
        .write_cursor = sizeof(name##_buf),         \
    };

ssize_t mock_read(char *buf, size_t len)
{
    return 0;
}

static char mock_source[] = "abcdefghijklmnopqrstuvwxyz";
static size_t mock_source_len;
static size_t mock_source_off;

// Mock that hands out `mock_source` a chunk at a time, up to the requested len.
ssize_t mock_source_read(void *ctx, char *buf, size_t len)
{
    size_t avail = mock_source_len - mock_source_off;
    if (avail > len)
        avail = len;
    memcpy(buf, mock_source + mock_source_off, avail);
    mock_source_off += avail;
    return (ssize_t)avail;
}

void test_reader_small_buffer_read_all(void)
{
    // A small well-formed reader (chunk_size == buf_len).  Pulling more data
    // than one buffer fill holds exercises the fill-and-copy loop repeatedly,
    // which is the path that used to overflow when the chunk exceeded the buf.
    char buf[8];
    buffered_reader_t reader = {
        .ctx = NULL,
        .buf = buf,
        .buf_len = sizeof(buf),
        .readable = true,
        .read = (buffered_reader_read_fn)mock_source_read,
        .chunk_size = sizeof(buf), // == buf_len, so well-formed
        .read_cursor = 0,
        .write_cursor = 0,
    };

    mock_source_len = 26;
    mock_source_off = 0;

    char out[26];
    ssize_t n = buffered_reader_read_all(&reader, out, sizeof(out));
    assert(n == 26 && "expected to read all source bytes");
    assert(memcmp(out, mock_source, 26) == 0 && "expected all source bytes in order");
}

void test_reader_read_until(void)
{
    GIVEN_READER(reader, "Hello, world");

    char result[256];
    ssize_t r = buffered_reader_read_until(&reader, SV_LIT(", "), result, sizeof(result));
    assert(r == 7 && "expected to consume 7 bytes including the needle");

    // we should expect the reader to be at "world"
    r = buffered_reader_read_nbytes(&reader, result, sizeof(result), 5);
    assert(r == 5 && "expected to be able to read 5 bytes");
    assert(memcmp(result, "world", 5) == 0 && "expected reader to be left at 'world'");
}

void test_reader_unread(void)
{
    GIVEN_READER(reader, "Hello, world");

    char result[256];

    // Consume 5 bytes ("Hello"), then push 3 back.  The next read of 5 bytes
    // should give us the 2 bytes that weren't unread plus the 3 we pushed
    // back: "llo, ".
    assert(buffered_reader_read_nbytes(&reader, result, sizeof(result), 5) == 5);
    assert(memcmp(result, "Hello", 5) == 0);
    assert(buffered_reader_unread(&reader, 3) == 0);

    assert(buffered_reader_read_nbytes(&reader, result, sizeof(result), 5) == 5);
    assert(memcmp(result, "llo, ", 5) == 0);

    // you can't unread more than is already in the reader
    size_t saved_cursor = reader.read_cursor;
    assert(buffered_reader_unread(&reader, 9999) == -1);
    assert(reader.read_cursor == saved_cursor && "failed unread must not move the cursor");
}

void test_common(void)
{
    TEST(test_reader_read_until);
    TEST(test_reader_unread);
    TEST(test_reader_small_buffer_read_all);
}
