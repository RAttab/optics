/* lens_streaming_bench.c
   Marina C., Jan 25, 20186
   FreeBSD-style copyright and disclaimer apply
*/

#include "bench.h"


struct streaming_bench
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

    struct streaming_bench *bench = data;
    optics_bench_start(b);

    for (size_t i = 0; i < n; ++i)
        optics_streaming_update(bench->lens, 1);
}


optics_test_head(lens_streaming_record_bench_st)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_streaming_alloc(optics, "my_streaming", 50, 0.99, 0.05);

    struct streaming_bench bench = { optics, lens };
    optics_bench_st(test_name, run_record_bench, &bench);

    optics_close(optics);
}
optics_test_tail()

/*
optics_test_head(lens_gauge_record_bench_mt)
{
    assert_mt();
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_gauge_alloc(optics, "my_gauge");

    struct gauge_bench bench = { optics, lens };
    optics_bench_mt(test_name, run_record_bench, &bench);

    optics_close(optics);
}
optics_test_tail()
*/

// -----------------------------------------------------------------------------
// read bench
// -----------------------------------------------------------------------------

void run_read_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    (void) id;
    struct streaming_bench *bench = data;
    optics_epoch_t epoch = optics_epoch(bench->optics);

    optics_bench_start(b);

    double value;
    for (size_t i = 0; i < n; ++i)
        optics_streaming_read(bench->lens, epoch, &value);
}


optics_test_head(lens_streaming_read_bench_st)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_streaming_alloc(optics, "my_streaming", 50, 0.99, 0.05);

    struct streaming_bench bench = { optics, lens };
    optics_bench_st(test_name, run_read_bench, &bench);

    optics_close(optics);
}
optics_test_tail()

/*
optics_test_head(lens_gauge_read_bench_mt)
{
    assert_mt();
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_gauge_alloc(optics, "my_gauge");

    struct gauge_bench bench = { optics, lens };
    optics_bench_mt(test_name, run_read_bench, &bench);

    optics_close(optics);
}
optics_test_tail()

*/
// -----------------------------------------------------------------------------
// mixed bench
// -----------------------------------------------------------------------------

void run_mixed_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    struct streaming_bench *bench = data;
    optics_epoch_t epoch = optics_epoch(bench->optics);

    optics_bench_start(b);

    if (!id) {
        double value;
        for (size_t i = 0; i < n; ++i)
            optics_streaming_read(bench->lens, epoch, &value);
    }
    else {
        for (size_t i = 0; i < n; ++i)
            optics_streaming_update(bench->lens, id);
    }
}

/*
optics_test_head(lens_gauge_mixed_bench_mt)
{
    assert_mt();
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_gauge_alloc(optics, "my_gauge");

    struct gauge_bench bench = { optics, lens };
    optics_bench_mt(test_name, run_mixed_bench, &bench);

    optics_close(optics);
}
optics_test_tail()

*/

// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(lens_streaming_record_bench_st),
        cmocka_unit_test(lens_streaming_read_bench_st),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
