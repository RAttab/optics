/* lens_histo_bench.c
   Rémi Attab (remi.attab@gmail.com), 07 Jul 2017
   FreeBSD-style copyright and disclaimer apply
*/

#include "bench.h"


struct histo_bench
{
    struct optics *optics;
    struct optics_lens *lens;

    double value;
};


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

#define calc_len(buckets) (sizeof(buckets) / sizeof(typeof((buckets)[0])))

static struct optics_lens *make_basic_lens(struct optics * optics)
{
    double buckets[] = {1, 2, 3, 4, 5, 6, 7, 8};
    return optics_histo_alloc(optics, "my_histo", buckets, calc_len(buckets));
}


// -----------------------------------------------------------------------------
// record value bench
// -----------------------------------------------------------------------------

void run_record_value_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    (void) id;

    struct histo_bench *bench = data;
    optics_bench_start(b);

    for (size_t i = 0; i < n; ++i)
        optics_histo_inc(bench->lens, bench->value);
}

static void record_value_bench(const char *test_name, double value)
{
    {
        struct optics *optics = optics_create(test_name);
        struct optics_lens *lens = make_basic_lens(optics);

        struct histo_bench bench = { optics, lens, value };

        char name[256];
        snprintf(name, sizeof(name), "%s_st", test_name);
        optics_bench_st(name, run_record_value_bench, &bench);

        optics_lens_close(lens);
        optics_close(optics);
    }

    {
        struct optics *optics = optics_create(test_name);
        struct optics_lens *lens = make_basic_lens(optics);

        struct histo_bench bench = { optics, lens, value };

        char name[256];
        snprintf(name, sizeof(name), "%s_mt", test_name);
        optics_bench_mt(name, run_record_value_bench, &bench);

        optics_lens_close(lens);
        optics_close(optics);
    }

}

optics_test_head(lens_histo_record_near_bench)
{
    record_value_bench(test_name, 1);
}
optics_test_tail()


optics_test_head(lens_histo_record_far_bench)
{
    record_value_bench(test_name, 7);
}
optics_test_tail()


optics_test_head(lens_histo_record_bound_bench)
{
    record_value_bench(test_name, 8);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// record spread
// -----------------------------------------------------------------------------

void run_record_spread_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    struct histo_bench *bench = data;
    optics_bench_start(b);

    size_t value = id;
    for (size_t i = 0; i < n; ++i)
        optics_histo_inc(bench->lens, ++value % 9);
}

optics_test_head(lens_histo_record_spread_bench_st)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = make_basic_lens(optics);

    struct histo_bench bench = { optics, lens };
    optics_bench_st(test_name, run_record_spread_bench, &bench);

    optics_lens_close(lens);
    optics_close(optics);
}
optics_test_tail()


optics_test_head(lens_histo_record_spread_bench_mt)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = make_basic_lens(optics);

    struct histo_bench bench = { optics, lens };
    optics_bench_mt(test_name, run_record_spread_bench, &bench);

    optics_lens_close(lens);
    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// read
// -----------------------------------------------------------------------------

void run_read_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    (void) id;

    struct histo_bench *bench = data;
    optics_epoch_t epoch = optics_epoch(bench->optics);

    optics_bench_start(b);

    struct optics_histo value;
    for (size_t i = 0; i < n; ++i)
        optics_histo_read(bench->lens, epoch, &value);
}

optics_test_head(lens_histo_read_bench_st)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = make_basic_lens(optics);

    struct histo_bench bench = { optics, lens };
    optics_bench_st(test_name, run_read_bench, &bench);

    optics_lens_close(lens);
    optics_close(optics);
}
optics_test_tail()


optics_test_head(lens_histo_read_bench_mt)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = make_basic_lens(optics);

    struct histo_bench bench = { optics, lens };
    optics_bench_mt(test_name, run_read_bench, &bench);

    optics_lens_close(lens);
    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// mixed
// -----------------------------------------------------------------------------

void run_mixed_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    struct histo_bench *bench = data;
    optics_epoch_t epoch = optics_epoch(bench->optics);

    optics_bench_start(b);

    if (!id) {
        struct optics_histo value;
        for (size_t i = 0; i < n; ++i)
            optics_histo_read(bench->lens, epoch, &value);
    }
    else {
        size_t value = id;
        for (size_t i = 0; i < n; ++i)
            optics_histo_inc(bench->lens, ++value % 9);
    }
}

optics_test_head(lens_histo_mixed_bench_st)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = make_basic_lens(optics);

    struct histo_bench bench = { optics, lens };
    optics_bench_st(test_name, run_mixed_bench, &bench);

    optics_lens_close(lens);
    optics_close(optics);
}
optics_test_tail()


optics_test_head(lens_histo_mixed_bench_mt)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = make_basic_lens(optics);

    struct histo_bench bench = { optics, lens };
    optics_bench_mt(test_name, run_mixed_bench, &bench);

    optics_lens_close(lens);
    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(lens_histo_record_near_bench),
        cmocka_unit_test(lens_histo_record_far_bench),
        cmocka_unit_test(lens_histo_record_bound_bench),
        cmocka_unit_test(lens_histo_record_spread_bench_st),
        cmocka_unit_test(lens_histo_record_spread_bench_mt),
        cmocka_unit_test(lens_histo_read_bench_st),
        cmocka_unit_test(lens_histo_read_bench_mt),
        cmocka_unit_test(lens_histo_mixed_bench_st),
        cmocka_unit_test(lens_histo_mixed_bench_mt),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
