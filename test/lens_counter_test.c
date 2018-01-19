/* lens_counter_test.c
   Rémi Attab (remi.attab@gmail.com), 03 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

#define assert_read(lens, epoch, exp)                                   \
    do {                                                                \
        int64_t value = 0;                                              \
        assert_int_equal(optics_counter_read(lens, epoch, &value), optics_ok); \
        assert_int_equal(value, exp);                                   \
    } while (false)


// -----------------------------------------------------------------------------
// open/close
// -----------------------------------------------------------------------------

optics_test_head(lens_counter_open_close_test)
{
    struct optics *optics = optics_create(test_name);
    const char *lens_name = "my_counter";

    for (size_t i = 0; i < 3; ++i) {
        struct optics_lens *lens = optics_counter_alloc(optics, lens_name);
        if (!lens) optics_abort();

        assert_int_equal(optics_lens_type(lens), optics_counter);
        assert_string_equal(optics_lens_name(lens), lens_name);

        assert_null(optics_counter_alloc(optics, lens_name));
        optics_lens_close(lens);
        assert_null(optics_counter_alloc(optics, lens_name));

        assert_non_null(lens = optics_lens_get(optics, lens_name));
        optics_lens_free(lens);
    }

    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// alloc_get
// -----------------------------------------------------------------------------

optics_test_head(lens_counter_alloc_get_test)
{
    struct optics *optics = optics_create(test_name);
    const char *lens_name = "blah";

    for (size_t i = 0; i < 3; ++i) {
        struct optics_lens *l0 = optics_counter_alloc_get(optics, lens_name);
        if (!l0) optics_abort();
        optics_counter_inc(l0, 1);

        struct optics_lens *l1 = optics_counter_alloc_get(optics, lens_name);
        if (!l1) optics_abort();
        optics_counter_inc(l1, 1);

        optics_epoch_t epoch = optics_epoch_inc(optics);
        assert_read(l0, epoch, 2);

        optics_lens_close(l0);
        optics_lens_free(l1);
    }

    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// record/read
// -----------------------------------------------------------------------------

optics_test_head(lens_counter_record_read_test)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_counter_alloc(optics, "my_counter");

    optics_epoch_t epoch = optics_epoch(optics);

    assert_read(lens, epoch, 0);

    optics_counter_inc(lens, 1);
    assert_read(lens, epoch, 1);
    assert_read(lens, epoch, 0);

    optics_counter_inc(lens, 1);
    optics_counter_inc(lens, 20);
    assert_read(lens, epoch, 21);
    assert_read(lens, epoch, 0);

    optics_counter_inc(lens, 1);
    optics_counter_inc(lens, 20);
    optics_counter_inc(lens, -2);
    assert_read(lens, epoch, 19);
    assert_read(lens, epoch, 0);

    optics_counter_inc(lens, 1);
    optics_counter_inc(lens, -1);
    assert_read(lens, epoch, 0);
    assert_read(lens, epoch, 0);

    optics_counter_inc(lens, 1);
    optics_counter_inc(lens, -2);
    assert_read(lens, epoch, -1);
    assert_read(lens, epoch, 0);

    optics_lens_close(lens);
    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// merge
// -----------------------------------------------------------------------------

optics_test_head(lens_counter_merge_test)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *l0 = optics_counter_alloc(optics, "l0");
    struct optics_lens *l1 = optics_counter_alloc(optics, "l1");
    optics_epoch_t epoch = optics_epoch(optics);

    {
        int64_t value = 0;
        assert_int_equal(optics_counter_read(l0, epoch, &value), optics_ok);
        assert_int_equal(optics_counter_read(l1, epoch, &value), optics_ok);
        assert_int_equal(value, 0);
    }

    {
        int64_t value = 0;

        optics_counter_inc(l0, 1);
        optics_counter_inc(l1, 10);

        assert_int_equal(optics_counter_read(l0, epoch, &value), optics_ok);
        assert_int_equal(optics_counter_read(l1, epoch, &value), optics_ok);
        assert_int_equal(value, 11);
    }


    {
        int64_t value = 0;

        optics_counter_inc(l0, 1);
        optics_counter_inc(l1, -10);

        assert_int_equal(optics_counter_read(l0, epoch, &value), optics_ok);
        assert_int_equal(optics_counter_read(l1, epoch, &value), optics_ok);
        assert_int_equal(value, -9);
    }

    optics_lens_close(l0);
    optics_lens_close(l1);
    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// type
// -----------------------------------------------------------------------------

optics_test_head(lens_counter_type_test)
{
    const char * lens_name = "blah";
    struct optics *optics = optics_create(test_name);

    int64_t value;
    optics_epoch_t epoch = optics_epoch(optics);

    {
        struct optics_lens *lens = optics_gauge_alloc(optics, lens_name);

        assert_false(optics_counter_inc(lens, 1));
        assert_int_equal(optics_counter_read(lens, epoch, &value), optics_err);

        optics_lens_close(lens);
    }


    {
        struct optics_lens *lens = optics_lens_get(optics, lens_name);

        assert_false(optics_counter_inc(lens, 1));
        assert_int_equal(optics_counter_read(lens, epoch, &value), optics_err);

        optics_lens_close(lens);
    }

    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// epoch st
// -----------------------------------------------------------------------------

optics_test_head(lens_counter_epoch_st_test)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_counter_alloc(optics, "my_counter");

    for (size_t i = 1; i < 5; ++i) {
        optics_epoch_t epoch = optics_epoch_inc(optics);
        optics_counter_inc(lens, i);

        assert_read(lens, epoch, i - 1);
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

    int64_t value = 0;
    assert_int_equal(optics_counter_read(test->lens, epoch, &value), optics_ok);

    return value;
}

void run_epoch_test(size_t id, void *ctx)
{
    struct epoch_test *test = ctx;
    enum { iterations = 1000 * 1000 };

    if (id) {
        for (size_t i = 0; i < iterations; ++i)
            optics_counter_inc(test->lens, 1);

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

optics_test_head(lens_counter_epoch_mt_test)
{
    assert_mt();
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_counter_alloc(optics, "my_counter");

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
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(lens_counter_open_close_test),
        cmocka_unit_test(lens_counter_alloc_get_test),
        cmocka_unit_test(lens_counter_record_read_test),
        cmocka_unit_test(lens_counter_merge_test),
        cmocka_unit_test(lens_counter_type_test),
        cmocka_unit_test(lens_counter_epoch_st_test),
        cmocka_unit_test(lens_counter_epoch_mt_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
