/* htable.h
   Rémi Attab (remi.attab@gmail.com), 10 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

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
struct htable_ret htable_del(struct htable *, const char *key);
