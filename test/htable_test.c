/* htable_test.c
   Rémi Attab (remi.attab@gmail.com), 15 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

#define assert_htable_ret(op, exp_ok, exp_value)       \
    do {                                               \
        struct htable_ret ret = op;                    \
        assert_int_equal(ret.ok, exp_ok);              \
        assert_int_equal(ret.value, exp_value);        \
    } while(false)


// -----------------------------------------------------------------------------
// basics
// -----------------------------------------------------------------------------

void basics_test(void **state)
{
    (void) state;
    struct htable ht = {0};

    for (size_t i = 0; i < 10; ++i) {
        char key[256];
        snprintf(key, sizeof(key), "key-%lu", i);

        assert_int_equal(ht.len, 0);
        assert_false(htable_get(&ht, key).ok);
        assert_false(htable_xchg(&ht, key, 10).ok);
        assert_false(htable_del(&ht, key).ok);

        assert_true(htable_put(&ht, key, 10).ok);

        assert_int_equal(ht.len, 1);
        assert_htable_ret(htable_put(&ht, key, 20), false, 10);

        assert_htable_ret(htable_get(&ht, key), true, 10);
        assert_htable_ret(htable_xchg(&ht, key, 30), true, 10);
        assert_htable_ret(htable_del(&ht, key), true, 30);

        assert_int_equal(ht.len, 0);
        assert_false(htable_get(&ht, key).ok);
        assert_false(htable_xchg(&ht, key, 10).ok);
        assert_false(htable_del(&ht, key).ok);
    }

    htable_reset(&ht);
}


// -----------------------------------------------------------------------------
// resize
// -----------------------------------------------------------------------------

void resize_test(void **state)
{
    (void) state;

    enum { n = 1024 };
    struct htable ht = {0};

    for (size_t i = 0; i < n; ++i) {
        char key[256];
        snprintf(key, sizeof(key), "key-%lu", i);
        assert_true(htable_put(&ht, key, i).ok);
    }
    assert_int_equal(ht.len, n);

    for (size_t i = 0; i < n; ++i) {
        char key[256];
        snprintf(key, sizeof(key), "key-%lu", i);
        assert_htable_ret(htable_get(&ht, key), true, i);
    }

    for (size_t i = 0; i < n; ++i) {
        char key[256];
        snprintf(key, sizeof(key), "key-%lu", i);
        assert_htable_ret(htable_del(&ht, key), true, i);
    }
    assert_int_equal(ht.len, 0);

    for (size_t i = 0; i < n; ++i) {
        char key[256];
        snprintf(key, sizeof(key), "key-%lu", i);
        assert_false(htable_get(&ht, key).ok);
    }

    htable_reset(&ht);
    assert_int_equal(ht.len, 0);
}


// -----------------------------------------------------------------------------
// foreach
// -----------------------------------------------------------------------------

void foreach_test(void **state)
{
    (void) state;
    struct htable ht = {0};

    for (size_t i = 0; i < 1024; ++i) {
        char key[256];
        snprintf(key, sizeof(key), "key-%lu", i);
        assert_true(htable_put(&ht, key, i).ok);
    }

    struct htable_bucket *it;
    struct htable result = {0};
    for (it = htable_next(&ht, NULL); it; it = htable_next(&ht, it))
        htable_put(&result, it->key, it->value);

    struct htable diff = {0};
    htable_diff(&ht, &result, &diff);
    assert_int_equal(diff.len, 0);

    htable_reset(&diff);
    htable_diff(&result, &ht, &diff);
    assert_int_equal(diff.len, 0);

    htable_reset(&result);
    htable_reset(&ht);
}


// -----------------------------------------------------------------------------
// put
// -----------------------------------------------------------------------------

// Test used to make sure that our linear probing for puts fully completes
// before we insert. In other words, if a hole is created in the middle of a
// probing window and the item being inserted happens to be on the other side of
// that hole then we shouldn't insert the item in the hole.
//
// Note that I don't really want to brute force a bunch of hash keys which would
// be a pain in the case where we changed the hash function. Instead we just try
// a bunch of keys and hope we hit the condition at least once.
void put_test(void **state)
{
    (void) state;
    enum { keys = 100 };

    for (size_t iteration = 0; iteration < 100; ++iteration) {
        struct htable ht = {0};

        for (size_t i = 0; i < keys; ++i) {
            char key[256];
            snprintf(key, sizeof(key), "key-%lu", i * keys + iteration);

            assert_true(htable_put(&ht, key, 10).ok);
        }

        // Create a hole.
        {
            char key[256];
            snprintf(key, sizeof(key), "key-%lu", 0 * keys + iteration);

            assert_htable_ret(htable_del(&ht, key), true, 10);
        }

        // Make sure that we can't insert keys that are already there.
        for (size_t i = 1; i < keys; ++i) {
            char key[256];
            snprintf(key, sizeof(key), "key-%lu", i * keys + iteration);

            assert_htable_ret(htable_put(&ht, key, 10), false, 10);
        }

        htable_reset(&ht);
    }
}


// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(basics_test),
        cmocka_unit_test(resize_test),
        cmocka_unit_test(foreach_test),
        cmocka_unit_test(put_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
