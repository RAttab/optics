/* buffer.h
   Rémi Attab (remi.attab@gmail.com), 09 Jun 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

// -----------------------------------------------------------------------------
// buffer
// -----------------------------------------------------------------------------

struct buffer
{
    size_t len;
    size_t cap;
    char *data;
};

void buffer_reset(struct buffer *);
void buffer_reserve(struct buffer *, size_t len);

void buffer_put(struct buffer *, char c);
void buffer_write(struct buffer *, const void *data, size_t len);

// Does not write out null char at the end of the formatted string
void buffer_printf(struct buffer *, const char *fmt, ...) optics_printf(2, 3);

