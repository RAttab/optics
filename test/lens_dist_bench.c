/* lens_dist_bench.c
   Rémi Attab (remi.attab@gmail.com), 07 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "bench.h"


struct dist_bench
{
    struct optics *optics;
    struct optics_lens *lens;
};


// -----------------------------------------------------------------------------
// record bench
// -----------------------------------------------------------------------------

void run_record_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    (void) id;
    struct dist_bench *bench = data;
    optics_bench_start(b);

    for (size_t i = 0; i < n; ++i)
        optics_dist_record(bench->lens, 1);
}


optics_test_head(lens_dist_record_bench_st)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_dist_alloc(optics, "my_dist");

    struct dist_bench bench = { optics, lens };
    optics_bench_st(test_name, run_record_bench, &bench);

    optics_close(optics);
}
optics_test_tail()


optics_test_head(lens_dist_record_bench_mt)
{
    assert_mt();
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_dist_alloc(optics, "my_dist");

    struct dist_bench bench = { optics, lens };
    optics_bench_mt(test_name, run_record_bench, &bench);

    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// read bench
// -----------------------------------------------------------------------------

void run_read_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    (void) id;
    struct dist_bench *bench = data;
    optics_epoch_t epoch = optics_epoch(bench->optics);

    optics_bench_start(b);

    struct optics_dist value;
    for (size_t i = 0; i < n; ++i)
        optics_dist_read(bench->lens, epoch, &value);
}


optics_test_head(lens_dist_read_bench_st)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_dist_alloc(optics, "my_dist");

    struct dist_bench bench = { optics, lens };
    optics_bench_st(test_name, run_read_bench, &bench);

    optics_close(optics);
}
optics_test_tail()


optics_test_head(lens_dist_read_bench_mt)
{
    assert_mt();
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_dist_alloc(optics, "my_dist");

    struct dist_bench bench = { optics, lens };
    optics_bench_mt(test_name, run_read_bench, &bench);

    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// mixed bench
// -----------------------------------------------------------------------------

void run_mixed_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    struct dist_bench *bench = data;
    optics_epoch_t epoch = optics_epoch(bench->optics);

    optics_bench_start(b);

    if (!id) {
        struct optics_dist value;
        for (size_t i = 0; i < n; ++i)
            optics_dist_read(bench->lens, epoch, &value);
    }
    else {
        for (size_t i = 0; i < n; ++i)
            optics_dist_record(bench->lens, 1);
    }
}


optics_test_head(lens_dist_mixed_bench_mt)
{
    assert_mt();
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_dist_alloc(optics, "my_dist");

    struct dist_bench bench = { optics, lens };
    optics_bench_mt(test_name, run_mixed_bench, &bench);

    optics_close(optics);
}
optics_test_tail()



// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(lens_dist_record_bench_st),
        cmocka_unit_test(lens_dist_record_bench_mt),
        cmocka_unit_test(lens_dist_read_bench_st),
        cmocka_unit_test(lens_dist_read_bench_mt),
        cmocka_unit_test(lens_dist_mixed_bench_mt),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
