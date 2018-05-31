/* lens_histo_test.c
   Rémi Attab (remi.attab@gmail.com), 03 Mar 2017
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"
#include "utils/rng.h"
#include "utils/time.h"


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

#define calc_len(buckets) (sizeof(buckets) / sizeof(typeof((buckets)[0])))

#define checked_histo_read(lens, epoch)                                 \
    ({                                                                  \
        struct optics_histo value = {0};                                \
        assert_int_equal(optics_histo_read(lens, epoch, &value), optics_ok); \
        value;                                                          \
    })


#define assert_histo_equal(histo, _buckets, _below, _above, ...)        \
    do {                                                                \
        assert_int_equal(histo.buckets_len, calc_len(_buckets));        \
                                                                        \
        for (size_t i = 0; i < calc_len(_buckets); ++i)                 \
            assert_float_equal(_buckets[i], histo.buckets[i], 0.001);   \
                                                                        \
        size_t counts[] = {__VA_ARGS__};                                \
        for (size_t i = 0; i < calc_len(_buckets) - 1; ++i)             \
            assert_int_equal(histo.counts[i], counts[i]);               \
                                                                        \
        assert_int_equal(histo.below, _below);                          \
        assert_int_equal(histo.above, _above);                          \
    } while(false)


// -----------------------------------------------------------------------------
// open/close
// -----------------------------------------------------------------------------

optics_test_head(lens_histo_open_close_test)
{
    struct optics *optics = optics_create(test_name);
    const char *lens_name = "my_histo";
    const double buckets[] = {1, 2};

    for (size_t i = 0; i < 3; ++i) {
        struct optics_lens *lens =
            optics_histo_alloc(optics, lens_name, buckets, calc_len(buckets));
        if (!lens) optics_abort();

        assert_int_equal(optics_lens_type(lens), optics_histo);
        assert_string_equal(optics_lens_name(lens), lens_name);

        assert_null(optics_histo_alloc(optics, lens_name, buckets, calc_len(buckets)));
        optics_lens_close(lens);
        assert_null(optics_histo_alloc(optics, lens_name, buckets, calc_len(buckets)));

        assert_non_null(lens = optics_lens_get(optics, lens_name));
        optics_lens_free(lens);
    }

    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// alloc_get
// -----------------------------------------------------------------------------

optics_test_head(lens_histo_alloc_get_test)
{
    struct optics *optics = optics_create(test_name);
    const char *lens_name = "my_histo";
    const double buckets[] = {1, 10, 100};

    for (size_t i = 0; i < 3; ++i) {
        struct optics_lens *l0 = optics_histo_alloc_get(
                optics, lens_name, buckets, calc_len(buckets));
        if (!l0) optics_abort();
        for (size_t j = 0; j < 50; ++j) optics_histo_inc(l0, j);

        struct optics_lens *l1 =
            optics_histo_alloc_get(optics, lens_name, buckets, calc_len(buckets));
        if (!l1) optics_abort();
        for (size_t j = 0; j < 50; ++j) optics_histo_inc(l0, j + 50);

        optics_epoch_t epoch = optics_epoch_inc(optics);

        struct optics_histo value = {0};
        optics_histo_read(l0, epoch, &value);
        assert_histo_equal(value, buckets, 1, 0, 9, 90);

        optics_lens_close(l0);
        optics_lens_free(l1);
    }

    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// validate
// -----------------------------------------------------------------------------

optics_test_head(lens_histo_validate_test)
{
    struct optics *optics = optics_create(test_name);
    const char *lens_name = "my_histo";

    {
        const double buckets[] = {};
        assert_null(optics_histo_alloc(optics, lens_name, buckets, calc_len(buckets)));
    }

    {
        const double buckets[] = {1};
        assert_null(optics_histo_alloc(optics, lens_name, buckets, calc_len(buckets)));
    }

    {
        const double buckets[] = {2, 1};
        assert_null(optics_histo_alloc(optics, lens_name, buckets, calc_len(buckets)));
    }

    {
        const double buckets[] = {1, 1};
        assert_null(optics_histo_alloc(optics, lens_name, buckets, calc_len(buckets)));
    }

    {
        double buckets[optics_histo_buckets_max + 1];
        for (size_t i = 0; i < calc_len(buckets); ++i) buckets[i] = i;
        assert_non_null(optics_histo_alloc(optics, lens_name, buckets, calc_len(buckets)));
    }

    {
        double buckets[optics_histo_buckets_max + 2];
        for (size_t i = 0; i < calc_len(buckets); ++i) buckets[i] = i;
        assert_null(optics_histo_alloc(optics, lens_name, buckets, calc_len(buckets)));
    }

    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// record/read
// -----------------------------------------------------------------------------

optics_test_head(lens_histo_record_read_test)
{
    struct optics *optics = optics_create(test_name);

    const double buckets[] = {10, 20, 30, 40, 50};
    struct optics_lens *lens = optics_histo_alloc(optics, "my_histo", buckets, calc_len(buckets));

    struct optics_histo value;
    optics_epoch_t epoch = optics_epoch(optics);

    value = checked_histo_read(lens, epoch);
    assert_histo_equal(value, buckets, 0, 0, 0, 0, 0, 0);

    assert_true(optics_histo_inc(lens, 0));
    value = checked_histo_read(lens, epoch);
    assert_histo_equal(value, buckets, 1, 0, 0, 0, 0, 0);

    assert_true(optics_histo_inc(lens, 10));
    value = checked_histo_read(lens, epoch);
    assert_histo_equal(value, buckets, 0, 0, 1, 0, 0, 0);

    assert_true(optics_histo_inc(lens, 20));
    value = checked_histo_read(lens, epoch);
    assert_histo_equal(value, buckets, 0, 0, 0, 1, 0, 0);

    assert_true(optics_histo_inc(lens, 35));
    value = checked_histo_read(lens, epoch);
    assert_histo_equal(value, buckets, 0, 0, 0, 0, 1, 0);

    assert_true(optics_histo_inc(lens, 49));
    value = checked_histo_read(lens, epoch);
    assert_histo_equal(value, buckets, 0, 0, 0, 0, 0, 1);

    assert_true(optics_histo_inc(lens, 50));
    value = checked_histo_read(lens, epoch);
    assert_histo_equal(value, buckets, 0, 1, 0, 0, 0, 0);

    optics_lens_close(lens);
    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// merge
// -----------------------------------------------------------------------------

optics_test_head(lens_histo_merge_test)
{
    const double buckets[] = {10, 20, 30, 40, 50};
    struct optics *optics = optics_create(test_name);
    struct optics_lens *l0 = optics_histo_alloc(optics, "l0", buckets, calc_len(buckets));
    struct optics_lens *l1 = optics_histo_alloc(optics, "l1", buckets, calc_len(buckets));
    optics_epoch_t epoch = optics_epoch(optics);

    {
        optics_histo_inc(l0, 11);
        optics_histo_inc(l1, 22);

        struct optics_histo value = {0};
        assert_int_equal(optics_histo_read(l0, epoch, &value), optics_ok);
        assert_int_equal(optics_histo_read(l1, epoch, &value), optics_ok);
        assert_histo_equal(value, buckets, 0, 0, 1, 1, 0, 0);
    }

    {
        optics_histo_inc(l0, 11);
        optics_histo_inc(l1, 12);

        struct optics_histo value = {0};
        assert_int_equal(optics_histo_read(l0, epoch, &value), optics_ok);
        assert_int_equal(optics_histo_read(l1, epoch, &value), optics_ok);
        assert_histo_equal(value, buckets, 0, 0, 2, 0, 0, 0);
    }

    {
        optics_histo_inc(l0, 1);
        optics_histo_inc(l0, 51);

        optics_histo_inc(l1, 2);
        optics_histo_inc(l1, 52);

        struct optics_histo value = {0};
        assert_int_equal(optics_histo_read(l0, epoch, &value), optics_ok);
        assert_int_equal(optics_histo_read(l1, epoch, &value), optics_ok);
        assert_histo_equal(value, buckets, 2, 2, 0, 0, 0, 0);
    }

    {
        const double buckets[] = {10, 20};
        struct optics_lens *l2 = optics_histo_alloc(optics, "l2", buckets, calc_len(buckets));

        struct optics_histo value = {0};
        assert_int_equal(optics_histo_read(l0, epoch, &value), optics_ok);
        assert_int_equal(optics_histo_read(l2, epoch, &value), optics_err);
        assert_int_equal(optics_histo_read(l1, epoch, &value), optics_ok);

        optics_lens_free(l2);
    }
    
    {
        const double buckets[] = {10, 25, 30, 40, 50};
        struct optics_lens *l2 = optics_histo_alloc(optics, "l2", buckets, calc_len(buckets));

        struct optics_histo value = {0};
        assert_int_equal(optics_histo_read(l0, epoch, &value), optics_ok);
        assert_int_equal(optics_histo_read(l2, epoch, &value), optics_err);
        assert_int_equal(optics_histo_read(l1, epoch, &value), optics_ok);

        optics_lens_free(l2);
    }

    optics_lens_free(l0);
    optics_lens_free(l1);
    optics_close(optics);
}
optics_test_tail()



// -----------------------------------------------------------------------------
// type
// -----------------------------------------------------------------------------

optics_test_head(lens_histo_type_test)
{
    const char * lens_name = "blah";
    struct optics *optics = optics_create(test_name);

    struct optics_histo value;
    optics_epoch_t epoch = optics_epoch(optics);

    {
        struct optics_lens *lens = optics_counter_alloc(optics, lens_name);

        assert_false(optics_histo_inc(lens, 1));
        assert_int_equal(optics_histo_read(lens, epoch, &value), optics_err);

        optics_lens_close(lens);
    }


    {
        struct optics_lens *lens = optics_lens_get(optics, lens_name);

        assert_false(optics_histo_inc(lens, 1));
        assert_int_equal(optics_histo_read(lens, epoch, &value), optics_err);

        optics_lens_close(lens);
    }

    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// epoch
// -----------------------------------------------------------------------------

optics_test_head(lens_histo_epoch_st_test)
{
    struct optics *optics = optics_create(test_name);

    const double buckets[] = {1, 2, 3, 4, 5};
    struct optics_lens *lens =
        optics_histo_alloc(optics, "my_histo", buckets, calc_len(buckets));

    for (size_t i = 1; i < 5; ++i) {
        optics_epoch_t epoch = optics_epoch_inc(optics);
        optics_histo_inc(lens, i);

        struct optics_histo value = checked_histo_read(lens, epoch);

        switch (i) {
        case 1: assert_histo_equal(value, buckets, 0, 0, 0, 0, 0, 0); break;
        case 2: assert_histo_equal(value, buckets, 0, 0, 1, 0, 0, 0); break;
        case 3: assert_histo_equal(value, buckets, 0, 0, 0, 1, 0, 0); break;
        case 4: assert_histo_equal(value, buckets, 0, 0, 0, 0, 1, 0); break;
        case 5: assert_histo_equal(value, buckets, 0, 0, 0, 0, 0, 1); break;
        default: optics_abort();
        }
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

void epoch_test_read_lens(struct epoch_test *test, struct optics_histo *result)
{
    optics_epoch_t epoch = optics_epoch_inc(test->optics);
    nsleep(1 * 1000);

    struct optics_histo value = checked_histo_read(test->lens, epoch);

    result->buckets_len = value.buckets_len;
    for (size_t i = 0; i < value.buckets_len; ++i)
        result->counts[i] += value.counts[i];
}

void run_epoch_test(size_t id, void *ctx)
{
    struct epoch_test *test = ctx;
    enum { iterations = 10 * 1000 * 1000 };

    rng_seed_with(rng_global(), 0);

    if (id) {
        for (size_t i = 0; i < iterations; ++i)
            optics_histo_inc(test->lens, i % 50);

        atomic_fetch_add_explicit(&test->done, 1, memory_order_release);
    }

    else {
        size_t done;
        struct optics_histo result = {0};
        size_t writers = test->workers - 1;

        do {
            epoch_test_read_lens(test, &result);
            done = atomic_load_explicit(&test->done, memory_order_acquire);
        } while (done < writers);

        // Read whatever is leftover in the remaining epochs
        for (size_t i = 0; i < 2; ++i)
            epoch_test_read_lens(test, &result);

        for (size_t i = 0; i < result.buckets_len; ++i) {
            size_t val = result.counts[i];
            size_t exp = (iterations / 50) * writers;
            optics_assert(val != exp, "%zu != %zu", val, exp);
        }
    }
}

optics_test_head(lens_histo_epoch_mt_test)
{
    assert_mt();
    struct optics *optics = optics_create(test_name);

    const double buckets[] = { 0, 10, 20, 30, 40, 50 };
    struct optics_lens *lens =
        optics_histo_alloc(optics, "my_histo", buckets, calc_len(buckets));

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
        /* cmocka_unit_test(lens_histo_open_close_test), */
        /* cmocka_unit_test(lens_histo_alloc_get_test), */
        /* cmocka_unit_test(lens_histo_validate_test), */
        /* cmocka_unit_test(lens_histo_record_read_test), */
        cmocka_unit_test(lens_histo_merge_test),
        /* cmocka_unit_test(lens_histo_type_test), */
        /* cmocka_unit_test(lens_histo_epoch_st_test), */
        /* cmocka_unit_test(lens_histo_epoch_mt_test), */
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
