/* key.c
   Rémi Attab (remi.attab@gmail.com), 26 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "optics.h"

#include <assert.h>
#include <bsd/string.h>

// -----------------------------------------------------------------------------
// key
// -----------------------------------------------------------------------------

size_t optics_key_push(struct optics_key *key, const char *suffix)
{
    size_t old = key->len;
    if (key->len + 1 == sizeof(key->data)) return old;

    if (key->len) {
        key->data[key->len] = '.';
        key->len++;
    }

    key->len += strlcpy(key->data + key->len, suffix, sizeof(key->data) - key->len);
    if (key->len >= sizeof(key->data)) key->len = sizeof(key->data) - 1;

    assert(key->data[key->len] == '\0');
    return old;
}

void optics_key_pop(struct optics_key *key, size_t pos)
{
    assert(pos <= key->len);

    key->len = pos;
    key->data[pos] = '\0';
}
