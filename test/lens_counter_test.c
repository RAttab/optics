/* lens_counter_test.c
   Rémi Attab (remi.attab@gmail.com), 03 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"

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
}
optics_test_tail()


// -----------------------------------------------------------------------------
// record/read
// -----------------------------------------------------------------------------

optics_test_head(lens_counter_record_read_test)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_counter_alloc(optics, "my_counter");

    int64_t value;
    optics_epoch_t epoch = optics_epoch(optics);

    assert_int_equal(optics_counter_read(lens, epoch, &value), optics_ok);
    assert_int_equal(value, 0);

    optics_counter_inc(lens, 1);
    assert_int_equal(optics_counter_read(lens, epoch, &value), optics_ok);
    assert_int_equal(value, 1);
    assert_int_equal(optics_counter_read(lens, epoch, &value), optics_ok);
    assert_int_equal(value, 0);

    optics_counter_inc(lens, 1);
    optics_counter_inc(lens, 20);
    assert_int_equal(optics_counter_read(lens, epoch, &value), optics_ok);
    assert_int_equal(value, 21);
    assert_int_equal(optics_counter_read(lens, epoch, &value), optics_ok);
    assert_int_equal(value, 0);

    optics_counter_inc(lens, 1);
    optics_counter_inc(lens, 20);
    optics_counter_inc(lens, -2);
    assert_int_equal(optics_counter_read(lens, epoch, &value), optics_ok);
    assert_int_equal(value, 19);
    assert_int_equal(optics_counter_read(lens, epoch, &value), optics_ok);
    assert_int_equal(value, 0);

    optics_counter_inc(lens, 1);
    optics_counter_inc(lens, -1);
    assert_int_equal(optics_counter_read(lens, epoch, &value), optics_ok);
    assert_int_equal(value, 0);
    assert_int_equal(optics_counter_read(lens, epoch, &value), optics_ok);
    assert_int_equal(value, 0);

    optics_counter_inc(lens, 1);
    optics_counter_inc(lens, -2);
    assert_int_equal(optics_counter_read(lens, epoch, &value), optics_ok);
    assert_int_equal(value, -1);
    assert_int_equal(optics_counter_read(lens, epoch, &value), optics_ok);
    assert_int_equal(value, 0);

    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// epoch
// -----------------------------------------------------------------------------

optics_test_head(lens_counter_epoch_test)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_counter_alloc(optics, "my_counter");

    int64_t value;
    for (size_t i = 1; i < 5; ++i) {
        optics_epoch_t epoch = optics_epoch_inc(optics);
        optics_counter_inc(lens, i);

        assert_int_equal(optics_counter_read(lens, epoch, &value), optics_ok);
        assert_int_equal(value, i - 1);
    }
}
optics_test_tail()


// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(lens_counter_open_close_test),
        cmocka_unit_test(lens_counter_record_read_test),
        cmocka_unit_test(lens_counter_epoch_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
