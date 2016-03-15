/* htable_bench.c
   Rémi Attab (remi.attab@gmail.com), 15 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "bench.h"


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

typedef char name_t[optics_name_max_len];

struct htable_bench
{
    struct htable *ht;

    size_t names_len;
    name_t *names;
};

name_t * make_names(size_t len)
{
    name_t *names = malloc(len * sizeof(name_t));

    for (size_t i = 0; i < len; ++i)
        snprintf(names[i], sizeof(names[i]), "key-%lu", i);

    return names;
}


// -----------------------------------------------------------------------------
// get
// -----------------------------------------------------------------------------

void run_get_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    (void) id;
    struct htable_bench *ctx = data;

    optics_bench_start(b);

    for (size_t i = 0; i < n; ++i)
        htable_get(ctx->ht, ctx->names[i % ctx->names_len]);
}


void get_bench_st(optics_unused void **state)
{
    enum { max_len = 100000 };
    name_t *names = make_names(max_len);

    for (size_t len = 1; len < max_len; len *= 10) {
        char title[256];
        snprintf(title, sizeof(title), "get_%lu_bench", len);

        struct htable ht = {0};
        for (size_t i = 0; i < len; ++i) htable_put(&ht, names[i], 1);

        struct htable_bench data = { &ht, len, names };
        optics_bench_st(title, run_get_bench, &data);
    }

    free(names);
}


// -----------------------------------------------------------------------------
// put
// -----------------------------------------------------------------------------

void run_put_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    (void) id;
    struct htable_bench *ctx = data;

    struct htable ht = {0};

    optics_bench_start(b);

    for (size_t i = 0; i < n; ++i)
        htable_put(&ht, ctx->names[i], 1);
}


void put_bench_st(optics_unused void **state)
{
    enum { max_len = 10000000 };
    name_t *names = make_names(max_len);

    for (size_t len = 1; len < max_len; len *= 10) {
        char title[256];
        snprintf(title, sizeof(title), "put_%lu_bench", len);

        struct htable_bench data = { .names = names };
        optics_bench_st(title, run_put_bench, &data);
    }

    free(names);
}

// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(get_bench_st),
        cmocka_unit_test(put_bench_st),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
