/* timing_bench.c
   Rémi Attab (remi.attab@gmail.com), 07 Apr 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "bench.h"
#include "utils/time.h"


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

inline void clock_monotonic_ptr(struct timespec *ts)
{
    if (optics_unlikely(clock_gettime(CLOCK_MONOTONIC, ts) == -1)) {
        optics_fail_errno("unable to read monotonic clock");
        optics_abort();
    }
}

optics_noinline struct timespec clock_monotonic_ret()
{
    struct timespec ts;
    if (optics_unlikely(clock_gettime(CLOCK_MONOTONIC, &ts) == -1)) {
        optics_fail_errno("unable to read monotonic clock");
        optics_abort();
    }

    return ts;
}


inline uint64_t delta(struct timespec *t0, struct timespec *t1)
{
    static const uint64_t seconds = 1UL * 1000 * 1000 * 1000;
    return (t1->tv_sec - t0->tv_sec) * seconds
        + (t1->tv_nsec - t0->tv_nsec);
}

inline double to_double(struct timespec *ts)
{
    return ts->tv_sec + ts->tv_nsec * 1e-9;
}


// -----------------------------------------------------------------------------
// monotonic
// -----------------------------------------------------------------------------

void run_clock_monotonic_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    (void) data, (void) id, (void) n;

    optics_bench_start(b);

    for (size_t i = 0; i < n; ++i) {
        struct timespec ts;
        clock_monotonic_ptr(&ts);
    }
}

optics_test_head(clock_monotonic_bench)
{
    optics_bench_st(test_name, run_clock_monotonic_bench, NULL);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// delta timespec ptr
// -----------------------------------------------------------------------------

void run_delta_timespec_ptr_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    (void) data, (void) id, (void) n;

    optics_bench_start(b);

    for (size_t i = 0; i < n; ++i) {
        struct timespec a;
        clock_monotonic_ptr(&a);

        struct timespec b;
        clock_monotonic_ptr(&b);

        uint64_t d = delta(&a, &b);
        optics_no_opt_val(d);
    }
}

optics_test_head(delta_timespec_ptr_bench)
{
    optics_bench_st(test_name, run_delta_timespec_ptr_bench, NULL);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// delta timespec ret
// -----------------------------------------------------------------------------

void run_delta_timespec_ret_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    (void) data, (void) id, (void) n;

    optics_bench_start(b);

    for (size_t i = 0; i < n; ++i) {
        struct timespec a = clock_monotonic_ret();
        struct timespec b = clock_monotonic_ret();

        uint64_t diff = delta(&a, &b);
        optics_no_opt_val(diff);
    }
}

optics_test_head(delta_timespec_ret_bench)
{
    optics_bench_st(test_name, run_delta_timespec_ret_bench, NULL);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// delta double
// -----------------------------------------------------------------------------

void run_delta_double_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    (void) data, (void) id, (void) n;

    optics_bench_start(b);

    for (size_t i = 0; i < n; ++i) {
        double a;
        {
            struct timespec ts;
            clock_monotonic_ptr(&ts);
            a = to_double(&ts);
        }

        double b;
        {
            struct timespec ts;
            clock_monotonic_ptr(&ts);
            b = to_double(&ts);
        }

        double diff = b - a;
        optics_no_opt_val(diff);
    }
}

optics_test_head(delta_double_bench)
{
    optics_bench_st(test_name, run_delta_double_bench, NULL);
}
optics_test_tail()



// -----------------------------------------------------------------------------
// optics timer
// -----------------------------------------------------------------------------

void run_optics_timer_bench(struct optics_bench *b, void *data, size_t id, size_t n)
{
    (void) data, (void) id, (void) n;

    optics_bench_start(b);

    for (size_t i = 0; i < n; ++i) {
        optics_timer_t timer;
        optics_timer_start(&timer);

        double diff = optics_timer_elapsed(&timer, optics_nsec);
        optics_no_opt_val(diff);
    }
}

optics_test_head(optics_timer_bench)
{
    optics_bench_st(test_name, run_optics_timer_bench, NULL);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// main
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(clock_monotonic_bench),
        cmocka_unit_test(delta_timespec_ptr_bench),
        cmocka_unit_test(delta_timespec_ret_bench),
        cmocka_unit_test(delta_double_bench),
        cmocka_unit_test(optics_timer_bench),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
