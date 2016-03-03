/* region_test.c
   Rémi Attab (remi.attab@gmail.com), 03 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"


// -----------------------------------------------------------------------------
// open/close
// -----------------------------------------------------------------------------

optics_test_head(region_open_close_test)
{
    for (size_t i = 0; i < 3; ++i) {
        assert_null(optics_open(test_name));

        struct optics *writer = optics_create(test_name);
        if (!writer) optics_abort();

        struct optics *reader = optics_open(test_name);
        if (!reader) optics_abort();

        optics_close(reader);
        optics_close(writer);
    }
}
optics_test_tail()


// -----------------------------------------------------------------------------
// unlink
// -----------------------------------------------------------------------------

void optics_crash(void) {}

// This test leaks intentionally since we're trying to re-create what happens
// after a crash.
optics_test_head(region_unlink_test)
{
    assert_false(optics_unlink(test_name));

    assert_non_null(optics_create(test_name));

    optics_crash();
    assert_non_null(optics_create(test_name));

    optics_crash();
    assert_true(optics_unlink(test_name));
    assert_null(optics_open(test_name));
}
optics_test_tail()


// -----------------------------------------------------------------------------
// prefix
// -----------------------------------------------------------------------------

optics_test_head(region_prefix_test)
{
    struct optics *writer = optics_create(test_name);
    assert_string_equal(optics_get_prefix(writer), test_name);

    const char *new_prefix = "blah";
    optics_set_prefix(writer, new_prefix);;
    assert_string_equal(optics_get_prefix(writer), new_prefix);

    struct optics *reader = optics_open(test_name);
    assert_string_equal(optics_get_prefix(reader), new_prefix);

    optics_close(reader);
    optics_close(writer);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// epoch
// -----------------------------------------------------------------------------

optics_test_head(region_epoch_test)
{
    struct optics *writer = optics_create(test_name);
    struct optics *reader = optics_open(test_name);

    for (size_t i = 0; i < 5; ++i) {
        optics_epoch_t epoch = optics_epoch(writer);
        assert_false(epoch & ~1);
        assert_int_equal(optics_epoch(writer), epoch);

        assert_int_equal(optics_epoch_inc(reader), epoch);
        assert_int_not_equal(epoch, optics_epoch(writer));
    }

    optics_close(reader);
    optics_close(writer);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(region_open_close_test),
        cmocka_unit_test(region_unlink_test),
        cmocka_unit_test(region_prefix_test),
        cmocka_unit_test(region_epoch_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
