/* key.c
   Rémi Attab (remi.attab@gmail.com), 26 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "key.h"


// -----------------------------------------------------------------------------
// key
// -----------------------------------------------------------------------------


size_t key_push(struct key *key, const char *suffix)
{
    key->buffer[key->pos] = '.';
    strcpy(key->buffer + key->pos + 1, suffix);

    size_t old = key->pos;
    key->pos += strlen(suffix) + 1;

    key->buffer[key->pos] = '\0';
    return old;
}

void key_pop(struct key *key, size_t pos)
{
    key->pos = pos;
    key->buffer[pos] = '\0';
}
