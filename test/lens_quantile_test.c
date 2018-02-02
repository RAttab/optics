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
    const char *lens_name = "my_quantile";

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
// update
// -----------------------------------------------------------------------------

optics_test_head(lens_quantile_update_read_test)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_quantile_alloc(optics, "my_gauge", 0.90, 50, 0.05);

    optics_epoch_t epoch = optics_epoch(optics);
    
    for(int i = 0; i < 1000; i++){
        for (int j = 0; j < 100; j++){
            optics_quantile_update(lens, j);
        }
    }
    
    double value = 0;
    assert_int_equal(optics_quantile_read(lens, epoch, &value), optics_ok);
    assert_float_equal(value, 90, 1);

    optics_lens_close(lens);
    optics_close(optics);
}
optics_test_tail()

// -----------------------------------------------------------------------------
// epoch
// -----------------------------------------------------------------------------
/*
optics_test_head(lens_gauge_epoch_test)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_gauge_alloc(optics, "my_gauge");

    double value = 0;

    optics_epoch_t e0 = optics_epoch(optics);
    optics_gauge_set(lens, 1.0);

    optics_epoch_inc(optics);

    assert_int_equal(optics_gauge_read(lens, e0, &value), optics_ok);
    assert_float_equal(value, 1.0, 0.0);

    optics_gauge_set(lens, 2.0);

    // Normally this shouldn't change due to the epoch change. This is a quirk
    // of the current implementation.
    assert_int_equal(optics_gauge_read(lens, e0, &value), optics_ok);
    assert_float_equal(value, 2.0, 0.0);

    optics_epoch_t e1 = optics_epoch_inc(optics);

    // Normally this shouldn't change due to the epoch change. This is a quirk
    // of the current implementation.
    assert_int_equal(optics_gauge_read(lens, e1, &value), optics_ok);
    assert_float_equal(value, 2.0, 0.0);

    optics_lens_close(lens);
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
        cmocka_unit_test(lens_quantile_open_close_test),
        cmocka_unit_test(lens_quantile_update_read_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
