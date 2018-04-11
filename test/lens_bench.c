/* lens_bench.c
   Rémi Attab (remi.attab@gmail.com), 08 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "bench.h"


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

struct bench_lens
{
    struct optics_lens *lens;
    char name[optics_name_max_len];
};

struct bench_lens *make_names(size_t len, size_t id)
{
    struct bench_lens *list = calloc(len, sizeof(*list));

    for (size_t i = 0; i < len; ++i) {
        snprintf(list[i].name, sizeof(list[i].name),
                "prefix_%lu_lens_%lu", id, i);
    }

    return list;
}


struct bench_lens *make_lenses(struct optics *optics, size_t len, size_t id)
{
    struct bench_lens *list = calloc(len, sizeof(*list));

    for (size_t i = 0; i < len; ++i) {
        snprintf(list[i].name, sizeof(list[i].name), "prefix_%lu_lens_%lu", id, i);
        list[i].lens = optics_counter_alloc(optics, list[i].name);
    }

    return list;
}

void close_lenses(struct bench_lens *list, size_t len)
{
    for (size_t i = 0; i < len; ++i)
        optics_lens_close(list[i].lens);

    free(list);
}

// -----------------------------------------------------------------------------
// get bench
// -----------------------------------------------------------------------------

struct get_bench
{
    struct optics *optics;
    struct bench_lens *list;
    size_t list_len;
};

void run_get_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    (void) id;

    struct get_bench *bench = data;
    optics_bench_start(b);

    for (size_t i = 0; i < n; ++i)
        optics_lens_get(bench->optics, bench->list[i % bench->list_len].name);
}

optics_test_head(lens_get_bench)
{
    for (size_t count = 1; count <= 100; count *= 2) {
        struct optics *optics = optics_create(test_name);

        struct bench_lens *list = make_lenses(optics, count, 0);
        struct get_bench bench = {
            .optics = optics,
            .list = list,
            .list_len = count,
        };

        char buffer[256];

        snprintf(buffer, sizeof(buffer), "%s_%lu_st", test_name, count);
        optics_bench_st(buffer, run_get_bench, &bench);

        if (cpus() >= 2) {
            snprintf(buffer, sizeof(buffer), "%s_%lu_mt", test_name, count);
            optics_bench_mt(buffer, run_get_bench, &bench);
        }

        close_lenses(list, count);
        optics_close(optics);
    }
}
optics_test_tail()


// -----------------------------------------------------------------------------
// alloc bench
// -----------------------------------------------------------------------------

void run_alloc_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    struct optics *optics = NULL;
    if (!id) optics = optics_create(data);
    optics = optics_bench_setup(b, optics);

    struct bench_lens *list = make_names(n, id);

    {
        optics_bench_start(b);

        for (size_t i = 0; i < n; ++i)
            list[i].lens = optics_gauge_alloc(optics, list[i].name);

        optics_bench_stop(b);
    }

    close_lenses(list, n);
    if (!id) optics_close(optics);
}

optics_test_head(lens_alloc_bench_st)
{
    optics_bench_st(test_name, run_alloc_bench, (void *) test_name);
}
optics_test_tail()

optics_test_head(lens_alloc_bench_mt)
{
    assert_mt();
    optics_bench_mt(test_name, run_alloc_bench, (void *) test_name);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// free bench
// -----------------------------------------------------------------------------

void run_free_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    struct optics *optics = NULL;
    if (!id) optics = optics_create(data);
    optics = optics_bench_setup(b, optics);

    struct bench_lens *list = make_lenses(optics, n, id);

    {
        optics_bench_start(b);

        for (size_t i = 0; i < n; ++i)
            optics_lens_free(list[i].lens);

        optics_bench_stop(b);
    }

    free(list);
    if (!id) optics_close(optics);
}

optics_test_head(lens_free_bench_st)
{
    optics_bench_st(test_name, run_free_bench, (void *) test_name);
}
optics_test_tail()

optics_test_head(lens_free_bench_mt)
{
    assert_mt();
    optics_bench_mt(test_name, run_free_bench, (void *) test_name);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// foreach bench
// -----------------------------------------------------------------------------

enum optics_ret foreach_cb(void *ctx, struct optics_lens *lens)
{
    (void) ctx, (void) lens;
    return optics_ok;
}

void run_foreach_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    (void) id;

    struct optics *optics = optics_create(data);
    struct bench_lens *list = make_lenses(optics, n, 0);

    {
        optics_bench_start(b);

        optics_foreach_lens(optics, NULL, foreach_cb);

        optics_bench_stop(b);
    }

    close_lenses(list, n);
    optics_close(optics);
}

optics_test_head(lens_foreach_bench_st)
{
    optics_bench_st(test_name, run_foreach_bench, (void *) test_name);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(lens_get_bench),
        cmocka_unit_test(lens_alloc_bench_st),
        cmocka_unit_test(lens_alloc_bench_mt),

        // Setup time for these benches is too long for the number of runs.
        /* cmocka_unit_test(lens_free_bench_st), */
        /* cmocka_unit_test(lens_free_bench_mt), */
        /* cmocka_unit_test(lens_foreach_bench_st), */
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
