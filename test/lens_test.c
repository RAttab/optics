/* lens_test.c
   Rémi Attab (remi.attab@gmail.com), 03 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

bool lens_count_cb(void *ctx, struct optics_lens *lens)
{
    (void) lens;

    size_t *counter = ctx;
    *counter += 1;
    return true;
}

ssize_t lens_count(struct optics *optics)
{
    size_t counter = 0;
    if (optics_foreach_lens(optics, &counter, lens_count_cb) < 0) return -1;
    return counter;
}


// -----------------------------------------------------------------------------
// basics
// -----------------------------------------------------------------------------

optics_test_head(lens_basics_st_test)
{
    struct optics *optics = optics_create(test_name);

    enum { n = 100 };
    struct optics_lens *lens[n] = {0};

    for (size_t iteration = 0; iteration < 5; ++iteration) {
        for (size_t i = 0; i < n; ++i) {
            char name[optics_name_max_len];
            snprintf(name, sizeof(name), "lens_%lu", i);

            lens[i] = optics_counter_alloc(optics, name);
            assert_int_equal(lens_count(optics), i + 1);
        }

        for (size_t i = 0; i < n; ++i) {
            size_t j = iteration % 2 ? i : n - i - 1;
            
            char name[optics_name_max_len];
            snprintf(name, sizeof(name), "lens_%lu", j);

            struct optics_lens *lens = optics_lens_get(optics, name);
            assert_non_null(lens);
            assert_int_equal(optics_lens_type(lens), optics_counter);
            assert_string_equal(optics_lens_name(lens), name);

            optics_lens_close(lens);
        }

        for (size_t i = 0; i < n; ++i) {
            size_t j = iteration % 2 ? i : n - i - 1;
            assert_true(optics_lens_free(lens[j]));
            assert_int_equal(lens_count(optics), n - (i + 1));
        }
    }

    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// basics_mt_test
// -----------------------------------------------------------------------------

void run_basics_mt(size_t thread_id, void *ctx)
{
    struct optics *optics = ctx;

    enum { n = 100 };
    struct optics_lens *lens[n] = {0};
    
    char name[optics_name_max_len];
    size_t name_i = snprintf(name, sizeof(name), "prefix_%lu_", thread_id);
    

    for (size_t iteration = 0; iteration < 100; ++iteration) {
        for (size_t i = 0; i < n; ++i) {
            snprintf(name + name_i, sizeof(name) - name_i, "lens_%lu", i);
            lens[i] = optics_counter_alloc(optics, name);
            assert_non_null(lens[i]);
        }

        for (size_t i = 0; i < n; ++i) {
            size_t j = iteration % 2 ? i : n - i - 1;
            snprintf(name + name_i, sizeof(name) - name_i, "lens_%lu", j);

            struct optics_lens *lens = optics_lens_get(optics, name);
            assert_non_null(lens);
            assert_int_equal(optics_lens_type(lens), optics_counter);
            assert_string_equal(optics_lens_name(lens), name);

            optics_lens_close(lens);
        }

        for (size_t i = 0; i < n; ++i) {
            size_t j = iteration % 2 ? i : n - i - 1;
            assert_true(optics_lens_free(lens[j]));
            lens[j] = NULL;
        }
    }
}

optics_test_head(lens_basics_mt_test)
{
    struct optics *optics = optics_create(test_name);
    run_threads(run_basics_mt, optics, 0);
    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(lens_basics_st_test),
        cmocka_unit_test(lens_basics_mt_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
