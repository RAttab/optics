/* lens_bench.c
   Rémi Attab (remi.attab@gmail.com), 08 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "bench.h"


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

void make_lens(struct optics *optics, size_t count, struct optics_lens ***lens, char ***names)
{
    char buffer[256];

    *lens = calloc(count, sizeof(**lens));
    *names = calloc(count, sizeof(**names));

    for (size_t i = 0; i < count; ++i) {
        snprintf(buffer, sizeof(buffer), "lens_%lu", i);

        (*lens)[i] = optics_counter_alloc(optics, buffer);
        (*names)[i] = strndup(buffer, sizeof(buffer));
    }
}

void close_lens(size_t count, struct optics_lens **lens, char **names)
{
    for (size_t i = 0; i < count; ++i) {
        optics_lens_close(lens[i]);
        free(names[i]);
    }

    free(lens);
    free(names);
}


// -----------------------------------------------------------------------------
// foreach bench
// -----------------------------------------------------------------------------

bool foreach_cb(void *ctx, struct optics_lens *lens)
{
    (void) ctx, (void) lens;
    return true;
}

void run_foreach_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    (void) id;

    struct optics *optics = optics_create(data);

    char **names;
    struct optics_lens **lens;
    make_lens(optics, n, &lens, &names);
    {
        optics_bench_start(b);

        optics_foreach_lens(optics, NULL, foreach_cb);

        optics_bench_stop(b);
    }

    close_lens(n, lens, names);
    optics_close(optics);
}

optics_test_head(lens_foreach_bench_st)
{
    optics_bench_st(test_name, run_foreach_bench, (void *) test_name);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// get bench
// -----------------------------------------------------------------------------

struct get_bench
{
    struct optics *optics;

    char **names;
    size_t names_len;
};

void run_get_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    (void) id;

    struct get_bench *bench = data;
    optics_bench_start(b);

    for (size_t i = 0; i < n; ++i)
        optics_lens_get(bench->optics, bench->names[i % bench->names_len]);
}

optics_test_head(lens_get_bench)
{
    for (size_t count = 1; count <= 1000; count *= 10) {
        struct optics *optics = optics_create(test_name);

        char **names;
        struct optics_lens **lens;
        make_lens(optics, count, &lens, &names);

        struct get_bench bench = {
            .optics = optics,
            .names = names,
            .names_len = count
        };

        char buffer[256];

        snprintf(buffer, sizeof(buffer), "%s_%lu_st", test_name, count);
        optics_bench_st(buffer, run_get_bench, &bench);

        snprintf(buffer, sizeof(buffer), "%s_%lu_mt", test_name, count);
        optics_bench_mt(buffer, run_get_bench, &bench);

        close_lens(count, lens, names);
        optics_close(optics);
    }
}
optics_test_tail()



// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    // This bench's setup is unbearably slow. To re-enable once the get_bench
    // scales better.
    (void) lens_foreach_bench_st;

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(lens_get_bench),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
