/* bench_bench.c
   Rémi Attab (remi.attab@gmail.com), 24 Jan 2016
   FreeBSD-style copyright and disclaimer apply

   control tests for the bench framework
*/

#include "bench.h"

#include "utils/time.h"
#include "utils/lock.h"
#include "utils/thread.h"


// -----------------------------------------------------------------------------
// harness bench
// -----------------------------------------------------------------------------

void run_harness_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    (void) id, (void) data;
    optics_bench_start(b);

    for (size_t i = 0; i < n; ++i) optics_no_opt();
}

void harness_bench_st(optics_unused void **state)
{
    optics_bench_st("harness_bench_st", run_harness_bench, NULL);
}

void harness_bench_mt(void **state)
{
    (void) state;

    optics_bench_mt("harness_bench_mt", run_harness_bench, NULL);
}


// -----------------------------------------------------------------------------
// sleep bench
// -----------------------------------------------------------------------------

void run_sleep_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    (void) id, (void) data;
    optics_bench_start(b);

    optics_nsleep(n);
}

void sleep_bench_st(optics_unused void **state)
{
    optics_bench_st("sleep_bench_st", run_sleep_bench, NULL);
}

void sleep_bench_mt(optics_unused void **state)
{
    optics_bench_mt("sleep_bench_mt", run_sleep_bench, NULL);
}


// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(harness_bench_st),
        cmocka_unit_test(harness_bench_mt),
        cmocka_unit_test(sleep_bench_st),
        cmocka_unit_test(sleep_bench_mt),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
