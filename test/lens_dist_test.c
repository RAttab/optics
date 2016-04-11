/* lens_dist_test.c
   Rémi Attab (remi.attab@gmail.com), 03 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"
#include "utils/rng.h"
#include "utils/time.h"


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

#define assert_dist_equal(dist, _n, _p50, _p90, _p99, _max, epsilon)    \
    do {                                                                \
        assert_int_equal(value.n, (_n));                                \
        assert_float_equal(value.p50, (_p50), (epsilon));               \
        assert_float_equal(value.p90, (_p90), (epsilon));               \
        assert_float_equal(value.p99, (_p99), (epsilon));               \
        assert_float_equal(value.max, (_max), (0.0001));                \
    } while (false)


static inline double p(double percentile, double max)
{
    return (max / 100) * percentile;
}


// -----------------------------------------------------------------------------
// open/close
// -----------------------------------------------------------------------------

optics_test_head(lens_dist_open_close_test)
{
    struct optics *optics = optics_create(test_name);
    const char *lens_name = "my_dist";

    for (size_t i = 0; i < 3; ++i) {
        struct optics_lens *lens = optics_dist_alloc(optics, lens_name);
        if (!lens) optics_abort();

        assert_int_equal(optics_lens_type(lens), optics_dist);
        assert_string_equal(optics_lens_name(lens), lens_name);

        assert_null(optics_dist_alloc(optics, lens_name));
        optics_lens_close(lens);
        assert_null(optics_dist_alloc(optics, lens_name));

        assert_non_null(lens = optics_lens_get(optics, lens_name));
        optics_lens_free(lens);
    }

    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// record/read - exact
// -----------------------------------------------------------------------------

optics_test_head(lens_dist_record_read_exact_test)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_dist_alloc(optics, "my_dist");

    struct optics_dist value;
    optics_epoch_t epoch = optics_epoch(optics);

    assert_int_equal(optics_dist_read(lens, epoch, &value), optics_ok);
    assert_dist_equal(value, 0, 0, 0, 0, 0, 0);

    assert_true(optics_dist_record(lens, 1));
    assert_int_equal(optics_dist_read(lens, epoch, &value), optics_ok);
    assert_dist_equal(value, 1, 1, 1, 1, 1, 0);

    for (size_t max = 10; max <= 200; max *= 10) {
        for (size_t i = 0; i < max; ++i) {
            assert_true(optics_dist_record(lens, i));
        }

        assert_int_equal(optics_dist_read(lens, epoch, &value), optics_ok);
        assert_dist_equal(
                value, max, p(50, max), p(90, max), p(99, max), max - 1, 1);

        assert_int_equal(optics_dist_read(lens, epoch, &value), optics_ok);
        assert_dist_equal(value, 0, 0, 0, 0, 0, 0);
    }

    for (size_t max = 10; max <= 200; max *= 10) {
        for (size_t i = 0; i < max; ++i) {
            assert_true(optics_dist_record(lens, max - (i + 1)));
        }

        assert_int_equal(optics_dist_read(lens, epoch, &value), optics_ok);
        assert_dist_equal(
                value, max, p(50, max), p(90, max), p(99, max), max - 1, 1);

        assert_int_equal(optics_dist_read(lens, epoch, &value), optics_ok);
        assert_dist_equal(value, 0, 0, 0, 0, 0, 0);
    }

    optics_lens_close(lens);
    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// record/read - random
// -----------------------------------------------------------------------------

optics_test_head(lens_dist_record_read_random_test)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_dist_alloc(optics, "my_dist");

    struct optics_dist value;
    optics_epoch_t epoch = optics_epoch(optics);

    const size_t max = 1 * 1000 * 1000;

    // Since we're using reservoir sampling, the error rate is proportional to
    // the number of elements recorded and since everything is randomized we
    // have to be quite generous with these error bounds to avoid false
    // negatives.
    const double epsilon = max / 20.0;

    {
        for (size_t i = 0; i < max; ++i) {
            assert_true(optics_dist_record(lens, i));
        }

        assert_int_equal(optics_dist_read(lens, epoch, &value), optics_ok);
        assert_dist_equal(
                value, max, p(50, max), p(90, max), p(99, max), max - 1, epsilon);

        assert_int_equal(optics_dist_read(lens, epoch, &value), optics_ok);
        assert_dist_equal(value, 0, 0, 0, 0, 0, 0);
    }

    {
        for (size_t i = 0; i < max; ++i) {
            assert_true(optics_dist_record(lens, max - i - 1));
        }

        assert_int_equal(optics_dist_read(lens, epoch, &value), optics_ok);
        assert_dist_equal(
                value, max, p(50, max), p(90, max), p(99, max), max - 1, epsilon);

        assert_int_equal(optics_dist_read(lens, epoch, &value), optics_ok);
        assert_dist_equal(value, 0, 0, 0, 0, 0, 0);
    }

    struct rng rng;
    rng_seed_with(&rng, 0);

    {
        size_t max_val = 0;
        for (size_t i = 0; i < max; ++i) {
            size_t val = rng_gen_range(&rng, 0, max);
            if (val > max_val) max_val = val;
            assert_true(optics_dist_record(lens, val));
        }

        assert_int_equal(optics_dist_read(lens, epoch, &value), optics_ok);
        assert_dist_equal(
                value, max, p(50, max), p(90, max), p(99, max), max_val, epsilon);

        assert_int_equal(optics_dist_read(lens, epoch, &value), optics_ok);
        assert_dist_equal(value, 0, 0, 0, 0, 0, 0);
    }

    optics_lens_close(lens);
    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// type
// -----------------------------------------------------------------------------

optics_test_head(lens_dist_type_test)
{
    const char * lens_name = "blah";
    struct optics *optics = optics_create(test_name);

    struct optics_dist value;
    optics_epoch_t epoch = optics_epoch(optics);

    {
        struct optics_lens *lens = optics_counter_alloc(optics, lens_name);

        assert_false(optics_dist_record(lens, 1));
        assert_int_equal(optics_dist_read(lens, epoch, &value), optics_err);

        optics_lens_close(lens);
    }


    {
        struct optics_lens *lens = optics_lens_get(optics, lens_name);

        assert_false(optics_dist_record(lens, 1));
        assert_int_equal(optics_dist_read(lens, epoch, &value), optics_err);

        optics_lens_close(lens);
    }

    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// epoch
// -----------------------------------------------------------------------------

optics_test_head(lens_dist_epoch_st_test)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_dist_alloc(optics, "my_dist");

    struct optics_dist value;
    for (size_t i = 1; i < 5; ++i) {
        optics_epoch_t epoch = optics_epoch_inc(optics);
        optics_dist_record(lens, i);

        assert_int_equal(optics_dist_read(lens, epoch, &value), optics_ok);

        size_t n = i - 1 ? 1 : 0;
        size_t v = i - 1;
        assert_dist_equal(value, n, v, v, v, v, 0);
    }

    optics_lens_close(lens);
    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// epoch mt
// -----------------------------------------------------------------------------

struct epoch_test
{
    struct optics *optics;
    struct optics_lens *lens;
    size_t workers;

    atomic_size_t done;
};

size_t epoch_test_read_lens(struct epoch_test *test)
{
    optics_epoch_t epoch = optics_epoch_inc(test->optics);

    struct optics_dist value;
    enum optics_ret ret;

    nsleep(1 * 1000 * 1000);

    while ((ret = optics_dist_read(test->lens, epoch, &value)) == optics_busy);
    assert_int_equal(ret, optics_ok);

    return value.n;
}

void run_epoch_test(size_t id, void *ctx)
{
    struct epoch_test *test = ctx;
    enum { iterations = 100 * 1000 };

    rng_seed_with(rng_global(), 0);

    if (id) {
        for (size_t i = 0; i < iterations; ++i)
            optics_dist_record(test->lens, 1.0);

        atomic_fetch_add_explicit(&test->done, 1, memory_order_release);
    }

    else {
        size_t done;
        uint64_t result = 0;
        size_t writers = test->workers - 1;

        do {
            result += epoch_test_read_lens(test);
            done = atomic_load_explicit(&test->done, memory_order_acquire);
        } while (done < writers);

        // Read whatever is leftover in the remaining epochs
        for (size_t i = 0; i < 2; ++i)
            result += epoch_test_read_lens(test);

        // cmocka just plain sucks when it comes to mt.
        optics_assert(result == writers * iterations, "%lu != %lu",
                result, writers * iterations);
    }
}

optics_test_head(lens_dist_epoch_mt_test)
{
    assert_mt();
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_dist_alloc(optics, "my_dist");

    struct epoch_test data = {
        .optics = optics,
        .lens = lens,
        .workers = cpus(),
    };
    run_threads(run_epoch_test, &data, data.workers);

    optics_lens_close(lens);
    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    rng_seed_with(rng_global(), 0);

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(lens_dist_open_close_test),
        cmocka_unit_test(lens_dist_record_read_exact_test),
        cmocka_unit_test(lens_dist_record_read_random_test),
        cmocka_unit_test(lens_dist_type_test),
        cmocka_unit_test(lens_dist_epoch_st_test),
        cmocka_unit_test(lens_dist_epoch_mt_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
