/* key.c
   Rémi Attab (remi.attab@gmail.com), 26 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "optics.h"

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

    optics_assert(key->data[key->len] == '\0',
            "key doesn't teminate with nil byte: len=%lu, char=%d",
            key->len, key->data[key->len]);
    return old;
}

size_t optics_key_pushf(struct optics_key *key, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char suffix[optics_name_max_len];
    (void) vsnprintf(suffix, optics_name_max_len, fmt, args);

    return optics_key_push(key, suffix);
}

void optics_key_pop(struct optics_key *key, size_t pos)
{
    optics_assert(pos <= key->len, "invalid key pop: %lu > %lu", pos, key->len);

    key->len = pos;
    key->data[pos] = '\0';
}
