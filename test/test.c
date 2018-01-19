/* test.c
   Rémi Attab (remi.attab@gmail.com), 03 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"


// -----------------------------------------------------------------------------
// impl
// -----------------------------------------------------------------------------

#include "test_http.c"


// -----------------------------------------------------------------------------
// assert - mt
// -----------------------------------------------------------------------------

void assert_mt()
{
    if (cpus() < 2) skip();
}


// -----------------------------------------------------------------------------
// assert - float
// -----------------------------------------------------------------------------

bool assert_float_equal_impl(double a, double b, double epsilon)
{
    if (fabs(a - b) <= epsilon) return true;

    printf("fabs(%g - %g) <= %g\n", a, b, epsilon);
    return false;
}


// -----------------------------------------------------------------------------
// assert - htable
// -----------------------------------------------------------------------------

static size_t htable_print(struct htable *ht, char *dest, size_t max)
{
    size_t dest_i = snprintf(dest, max, "%lu:[ ", ht->len);

    struct htable_bucket *it;
    for (it = htable_next(ht, NULL); it; it = htable_next(ht, it)) {
        dest_i += snprintf(dest + dest_i, max - dest_i, "{%s, %f} ",
                it->key, pun_itod(it->value));
    }

    return dest_i + snprintf(dest + dest_i, max - dest_i, "]");
}

bool assert_htable_equal_impl(
        struct htable *a,
        const struct htable_bucket *buckets,
        size_t buckets_len,
        double epsilon)
{
    struct htable b = {0};
    for (size_t i = 0; i < buckets_len; ++i)
        htable_put(&b, buckets[i].key, buckets[i].value);

    char buffer[1024];
    bool result = true;

    struct htable diff = {0};
    htable_diff(a, &b, &diff);
    if (diff.len) {
        result = false;
        htable_print(&diff, buffer, sizeof(buffer));
        printf("extra: %s\n", buffer);
    }

    htable_reset(&diff);
    htable_diff(&b, a, &diff);
    if (diff.len) {
        result = false;
        htable_print(&diff, buffer, sizeof(buffer));
        printf("missing: %s\n", buffer);
    }

    struct htable_bucket *it;
    for (it = htable_next(a, NULL); it; it = htable_next(a, it)) {
        struct htable_ret ret = htable_get(&b, it->key);
        if (!ret.ok) continue;

        double a_value = pun_itod(it->value);
        double b_value = pun_itod(ret.value);

        if (!assert_float_equal_impl(a_value, b_value, epsilon)) {
            printf("  -> %s\n", it->key);
            result = false;
        }
    }

    htable_reset(&b);
    return result;
}


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

struct optics *optics_create_idx_at(
        const char *base, size_t index, optics_ts_t ts)
{
    char name[256];
    snprintf(name, sizeof(name), "%s_%zu", base, index);
    return optics_create_at(name, ts);
}

