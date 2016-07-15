/* key_test.c
   Rémi Attab (remi.attab@gmail.com), 07 Apr 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"


// -----------------------------------------------------------------------------
// basics
// -----------------------------------------------------------------------------

optics_test_head(basics_test)
{
    struct optics_key key = {0};

    size_t i = optics_key_push(&key, "blah");
    assert_string_equal(key.data, "blah");

    size_t j = optics_key_push(&key, "bleh");
    assert_string_equal(key.data, "blah.bleh");

    size_t k = optics_key_push(&key, "bloh");
    assert_string_equal(key.data, "blah.bleh.bloh");

    optics_key_pop(&key, k);
    assert_string_equal(key.data, "blah.bleh");

    optics_key_push(&key, "blyh");
    assert_string_equal(key.data, "blah.bleh.blyh");

    optics_key_pop(&key, j);
    assert_string_equal(key.data, "blah");

    optics_key_pop(&key, i);
    assert_string_equal(key.data, "");
}
optics_test_tail()


// -----------------------------------------------------------------------------
// overflow
// -----------------------------------------------------------------------------

optics_test_head(overflow_test)
{
    struct optics_key key = {0};

    char long_key[optics_name_max_len * 2] = {0};
    for (size_t i = 0; i < sizeof(long_key) - 1; ++i) long_key[i] = 'a';

    size_t i = optics_key_push(&key, long_key);
    assert_int_equal(key.len, optics_name_max_len - 1);

    size_t j = optics_key_push(&key, "a");
    assert_int_equal(j, optics_name_max_len - 1);
    assert_int_equal(key.len, optics_name_max_len - 1);

    optics_key_pop(&key, i);
    assert_string_equal(key.data, "");
}
optics_test_tail()


// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(basics_test),
        cmocka_unit_test(overflow_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
