/* htable.h
   Rémi Attab (remi.attab@gmail.com), 10 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

#include <string.h>

// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

enum { htable_key_max_len = 1024 };

struct htable_bucket
{
    const char *key;
    uint64_t value;
};

struct htable
{
    size_t len;
    size_t cap;
    struct htable_bucket *table;
};


struct htable_ret
{
    bool ok;
    uint64_t value;
};

// -----------------------------------------------------------------------------
// basics
// -----------------------------------------------------------------------------

void htable_reset(struct htable *);
void htable_reserve(struct htable *, size_t items);


// -----------------------------------------------------------------------------
// ops
// -----------------------------------------------------------------------------

struct htable_ret htable_get(struct htable *, const char *key);
struct htable_ret htable_put(struct htable *, const char *key, uint64_t value);
struct htable_ret htable_xchg(struct htable *, const char *key, uint64_t value);
struct htable_ret htable_del(struct htable *, const char *key);
struct htable_bucket * htable_next(struct htable *, struct htable_bucket *bucket);

void htable_diff(struct htable *a, struct htable *b, struct htable *result);


// -----------------------------------------------------------------------------
// hash
// -----------------------------------------------------------------------------

// FNV-1a hash implementation: http://isthe.com/chongo/tech/comp/fnv/
inline uint64_t htable_hash(const char *key)
{
    uint64_t hash = 0xcbf29ce484222325;
    for (size_t i = 0; i < strnlen(key, htable_key_max_len); ++i)
        hash = (hash ^ key[i]) * 0x100000001b3;

    return hash;
}
