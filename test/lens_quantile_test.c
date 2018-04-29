/* lens_gauge_test.c
   Marina C., January 22, 2018
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"

// -----------------------------------------------------------------------------
// open/close
// -----------------------------------------------------------------------------

optics_test_head(lens_quantile_open_close_test)
{
    struct optics *optics = optics_create(test_name);
    const char *lens_name = "bob_the_quantile";

    for (size_t i = 0; i < 3; ++i) {
        struct optics_lens *lens = optics_quantile_alloc(optics, lens_name, 0.99, 0, 0.05);
        if (!lens) optics_abort();

        assert_int_equal(optics_lens_type(lens), optics_quantile);
        assert_string_equal(optics_lens_name(lens), lens_name);

        assert_null(optics_quantile_alloc(optics, lens_name, 0.99, 0, 0.05));
        optics_lens_close(lens);
        assert_null(optics_quantile_alloc(optics, lens_name, 0.99, 0, 0.05));

        assert_non_null(lens = optics_lens_get(optics, lens_name));
        optics_lens_free(lens);
    }

    optics_close(optics);
}
optics_test_tail()

// -----------------------------------------------------------------------------
// update ST
// -----------------------------------------------------------------------------

optics_test_head(lens_quantile_update_read_test)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_quantile_alloc(optics, "bob_the_quantile", 0.90, 70, 0.05);

    optics_epoch_t epoch = optics_epoch(optics);

    for(int i = 0; i < 1000; i++){
        for (int j = 0; j < 100; j++){
            optics_quantile_update(lens, j);
        }
    }

    struct optics_quantile value = {0};
    assert_int_equal(optics_quantile_read(lens, epoch, &value), optics_ok);
    assert_float_equal(value.sample, 90, 1);
    assert_int_equal(value.count, 1000 * 100);

    optics_lens_close(lens);
    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// merge test
// -----------------------------------------------------------------------------

static void test_merge(
        struct optics *optics,
        double quantile,
        size_t n0, double v0,
        size_t n1, double v1,
        double exp)
{
    optics_log("test", "quantile=%g, 0={%zu, %g}, 1={%zu, %g}, exp=%g",
            quantile, n0, v0, n1, v1, exp);

    // reset the seed for consistent result.
    rng_seed_with(rng_global(), 0);

    // By the setting the adjusment to 0, I'm guaranteeing the value of the
    // read. In other words, I'm cheating :)
    struct { size_t n; double val; struct optics_lens *lens; } item[2] = {
        { n0, v0, optics_quantile_alloc(optics, "l0", quantile, v0, 0) },
        { n1, v1, optics_quantile_alloc(optics, "l1", quantile, v1, 0) },
    };

    enum { iterations = 100 };
    double result = 0.0;
    optics_epoch_t epoch = optics_epoch(optics);

    for (size_t it = 0; it < iterations; ++it) {
        for (size_t i = 0; i < 2; ++i) {
            for (size_t j = 0; j < item[i].n; ++j)
                optics_quantile_update(item[i].lens, item[i].val);
        }

        epoch = optics_epoch_inc(optics);

        struct optics_quantile value = {0};
        for (size_t i = 0; i < 2; ++i)
            assert_int_equal(optics_quantile_read(item[i].lens, epoch, &value), optics_ok);

        assert_int_equal(value.count, n0 + n1);
        if (value.sample == v1) result++;
        else if (value.sample != v0) assert(false && "something went terribly wrong");

    }

    assert_float_equal(result / iterations, exp, 0.1);

    for (size_t i = 0; i < 2; ++i)
        optics_lens_free(item[i].lens);
}

optics_test_head(lens_quantile_merge_test)
{
    struct optics *optics = optics_create(test_name);

    const double v0 = 0;
    const double v1 = 1;

    test_merge(optics, 0.1, 100, v0, 100, v1, 0.1);
    test_merge(optics, 0.3, 100, v0, 100, v1, 0.3);
    test_merge(optics, 0.5, 100, v0, 100, v1, 0.5);
    test_merge(optics, 0.7, 100, v0, 100, v1, 0.7);
    test_merge(optics, 0.9, 100, v0, 100, v1, 0.9);

    test_merge(optics, 0.5, 1, v0, 1000, v1, 1.00);
    test_merge(optics, 0.5, 50, v0, 100, v1, 0.75);
    test_merge(optics, 0.5, 100, v0, 50, v1, 0.25);
    test_merge(optics, 0.5, 1000, v0, 1, v1, 0.00);

    test_merge(optics, 0.1, 1, v0, 1000, v1, 1.00);
    test_merge(optics, 0.1, 1000, v0, 1, v1, 0.00);
    test_merge(optics, 0.9, 1000, v0, 1, v1, 0.00);
    test_merge(optics, 0.9, 1, v0, 1000, v1, 1.00);

    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// update MT
// -----------------------------------------------------------------------------

struct mt_test
{
    struct optics *optics;
    struct optics_lens *lens;
    size_t workers;

    atomic_size_t done;
};

double mt_test_read_lens(struct mt_test *test)
{
    optics_epoch_t epoch = optics_epoch(test->optics);

    struct optics_quantile value = {0};
    assert_int_equal(optics_quantile_read(test->lens, epoch, &value), optics_ok);

    return value.sample;
}

void run_mt_test(size_t id, void *ctx)
{
    struct mt_test *test = ctx;
    enum { iterations = 100 };

    if (id) {
        for (size_t i = 0; i < iterations; ++i){
            for (size_t j = 0; j < 100; ++j)
                optics_quantile_update(test->lens, j);
        }

        atomic_fetch_add_explicit(&test->done, 1, memory_order_release);
    }
    else {
        size_t done;
        double result = 0;
        size_t writers = test->workers - 1;

        do {
            done = atomic_load_explicit(&test->done, memory_order_acquire);
        } while (done < writers);

        result = mt_test_read_lens(test);

        assert_float_equal(result, 90, 1);
    }
}

optics_test_head(lens_quantile_update_read_mt_test)
{
    assert_mt();
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_quantile_alloc(optics, "bob_the_quantile", 0.90, 50, 0.05);

    struct mt_test data = {
        .optics = optics,
        .lens = lens,
        .workers = cpus(),
    };
    run_threads(run_mt_test, &data, data.workers);

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
        cmocka_unit_test(lens_quantile_open_close_test),
        cmocka_unit_test(lens_quantile_update_read_test),
        cmocka_unit_test(lens_quantile_merge_test),
        cmocka_unit_test(lens_quantile_update_read_mt_test)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
