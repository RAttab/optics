/* key.c
   Rémi Attab (remi.attab@gmail.com), 26 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "key.h"

#include <assert.h>
#include <bsd/string.h>

// -----------------------------------------------------------------------------
// key
// -----------------------------------------------------------------------------

size_t key_push(struct key *key, const char *suffix)
{
    size_t old = key->pos;

    if (key->pos) {
        key->buffer[key->pos] = '.';
        key->pos++;
    }
    strlcpy(key->buffer + key->pos, suffix, sizeof(key->buffer) - key->pos);

    key->pos += strnlen(suffix, optics_name_max_len);
    assert(key->buffer[key->pos] == '\0');

    return old;
}

void key_pop(struct key *key, size_t pos)
{
    key->pos = pos;
    key->buffer[pos] = '\0';
}
