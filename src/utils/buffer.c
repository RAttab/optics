/* buffer.c
   Rémi Attab (remi.attab@gmail.com), 09 Jun 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "buffer.h"


// -----------------------------------------------------------------------------
// config
// -----------------------------------------------------------------------------

enum { buffer_min_cap = 128 };


// -----------------------------------------------------------------------------
// buffer
// -----------------------------------------------------------------------------

void buffer_reset(struct buffer *buffer)
{
    if (buffer->data) free(buffer->data);
    buffer->data = NULL;
    buffer->len = 0;
    buffer->cap = 0;
}

void buffer_reserve(struct buffer *buffer, size_t len)
{
    if (len < buffer->cap) return;

    if (!buffer->cap) buffer->cap = buffer_min_cap;
    while (len > buffer->cap) buffer->cap *= 2;

    buffer->data = buffer->data ?
        realloc(buffer->data, buffer->cap) :
        malloc(buffer->cap);
    optics_assert_alloc(buffer->data);
}

void buffer_put(struct buffer *buffer, char c)
{
    buffer_reserve(buffer, buffer->len + 1);
    buffer->data[buffer->len] = c;
    buffer->len++;
}

void buffer_write(struct buffer *buffer, const void *data, size_t len)
{
    buffer_reserve(buffer, buffer->len + len);
    memcpy(buffer->data + buffer->len, data, len);
    buffer->len += len;
}

void buffer_printf(struct buffer *buffer, const char *fmt, ...)
{
    int ret;
    size_t avail = buffer->cap - buffer->len;

    {
        va_list args;
        va_start(args, fmt);
        ret = vsnprintf(buffer->data + buffer->len, avail, fmt, args);
        va_end(args);
    }

    if (ret > 0 && (size_t) ret > avail) {
        buffer_reserve(buffer, buffer->len + ret);
        avail = buffer->cap - buffer->len;

        va_list args;
        va_start(args, fmt);
        ret = vsnprintf(buffer->data + buffer->len, avail, fmt, args);
        va_end(args);
    }

    if (ret < 0) {
        optics_fail("unable to print '%s' to buffer", fmt);
        optics_abort();
    }

    buffer->len += ret;
}
