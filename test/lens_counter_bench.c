/* lens_counter_bench.c
   Rémi Attab (remi.attab@gmail.com), 04 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "bench.h"


struct counter_bench
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
    struct counter_bench *bench = data;
    optics_bench_start(b);

    for (size_t i = 0; i < n; ++i)
        optics_counter_inc(bench->lens, 1);
}


optics_test_head(lens_counter_record_bench_st)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_counter_alloc(optics, "my_counter");

    struct counter_bench bench = { optics, lens };
    optics_bench_st(test_name, run_record_bench, &bench);

    optics_close(optics);
}
optics_test_tail()


optics_test_head(lens_counter_record_bench_mt)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_counter_alloc(optics, "my_counter");

    struct counter_bench bench = { optics, lens };
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
    struct counter_bench *bench = data;
    optics_epoch_t epoch = optics_epoch(bench->optics);

    optics_bench_start(b);

    int64_t value;
    for (size_t i = 0; i < n; ++i)
        optics_counter_read(bench->lens, epoch, &value);
}


optics_test_head(lens_counter_read_bench_st)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_counter_alloc(optics, "my_counter");

    struct counter_bench bench = { optics, lens };
    optics_bench_st(test_name, run_read_bench, &bench);

    optics_close(optics);
}
optics_test_tail()


optics_test_head(lens_counter_read_bench_mt)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_counter_alloc(optics, "my_counter");

    struct counter_bench bench = { optics, lens };
    optics_bench_mt(test_name, run_read_bench, &bench);

    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// mixed bench
// -----------------------------------------------------------------------------

void run_mixed_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    struct counter_bench *bench = data;
    optics_epoch_t epoch = optics_epoch(bench->optics);

    optics_bench_start(b);

    if (!id) {
        int64_t value;
        for (size_t i = 0; i < n; ++i)
            optics_counter_read(bench->lens, epoch, &value);
    }
    else {
        for (size_t i = 0; i < n; ++i)
            optics_counter_inc(bench->lens, 1);
    }
}


optics_test_head(lens_counter_mixed_bench_mt)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_counter_alloc(optics, "my_counter");

    struct counter_bench bench = { optics, lens };
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
        cmocka_unit_test(lens_counter_record_bench_st),
        cmocka_unit_test(lens_counter_record_bench_mt),
        cmocka_unit_test(lens_counter_read_bench_st),
        cmocka_unit_test(lens_counter_read_bench_mt),
        cmocka_unit_test(lens_counter_mixed_bench_mt),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
