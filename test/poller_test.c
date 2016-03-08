/* poller_test.c
   Rémi Attab (remi.attab@gmail.com), 08 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"


// -----------------------------------------------------------------------------
// backend
// -----------------------------------------------------------------------------

void backend_cb(void *ctx, uint64_t ts, const char *key, double value)
{
    (void) ts;

    struct optics_set *keys = ctx;
    assert_true(optics_set_put(keys, key, value));

    char buffer[1024];
    optics_set_print(keys, buffer, sizeof(buffer));
}


// -----------------------------------------------------------------------------
// nil
// -----------------------------------------------------------------------------

optics_test_head(poller_nil_test)
{
    struct optics_set result = {0};
    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_backend(poller, &result, backend_cb, NULL);

    for (size_t i = 0; i < 3; ++i) {
        optics_poller_poll_at(poller, i);
        assert_int_equal(result.n, 0);
    }

    optics_poller_free(poller);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// poller multi lens
// -----------------------------------------------------------------------------

optics_test_head(poller_multi_lens_test)
{
    struct optics_set result = {0};
    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_backend(poller, &result, backend_cb, NULL);

    struct optics *optics = optics_create(test_name);
    optics_set_prefix(optics, "bleh");

    time_t ts = 0;

    struct optics_lens *g1 = optics_gauge_alloc(optics, "g1");
    struct optics_lens *g2 = optics_gauge_alloc(optics, "g2");
    struct optics_lens *g3 = optics_gauge_alloc(optics, "g3");
    optics_gauge_set(g2, 1.0);
    optics_gauge_set(g3, 1.2e-4);

    optics_set_reset(&result);
    optics_poller_poll_at(poller, ++ts);
    assert_set_equal(&result, 0,
            make_kv("bleh.g1", 0.0),
            make_kv("bleh.g2", 1.0),
            make_kv("bleh.g3", 1.2e-4));

    struct optics_lens *g4 = optics_gauge_alloc(optics, "g4");
    optics_lens_free(g1);
    optics_gauge_set(g2, 2.0);
    optics_gauge_set(g4, -1.0);

    optics_set_reset(&result);
    optics_poller_poll_at(poller, ++ts);
    assert_set_equal(&result, 0,
            make_kv("bleh.g2", 2.0),
            make_kv("bleh.g3", 1.2e-4),
            make_kv("bleh.g4", -1.0));

    g1 = optics_gauge_alloc(optics, "g1");
    optics_gauge_set(g1, 1.0);

    optics_set_reset(&result);
    optics_poller_poll_at(poller, ++ts);
    assert_set_equal(&result, 0,
            make_kv("bleh.g1", 1.0),
            make_kv("bleh.g2", 2.0),
            make_kv("bleh.g3", 1.2e-4),
            make_kv("bleh.g4", -1.0));

    optics_lens_close(g1);
    optics_lens_close(g2);
    optics_lens_close(g3);
    optics_lens_close(g4);
    optics_close(optics);

    optics_set_reset(&result);
    optics_poller_poll_at(poller, ++ts);
    assert_int_equal(result.n, 0);

    optics_poller_free(poller);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// poller multi region
// -----------------------------------------------------------------------------

optics_test_head(poller_multi_region_test)
{
    struct optics_set result = {0};
    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_backend(poller, &result, backend_cb, NULL);

    time_t ts = 0;
    struct optics *r1 = optics_create("r1");

    struct optics *r2 = optics_create("r2");
    struct optics_lens *r2_l1 = optics_gauge_alloc(r2, "l1");
    optics_gauge_set(r2_l1, 1.0);

    struct optics *r3 = optics_create("r3");
    struct optics_lens *r3_l1 = optics_gauge_alloc(r3, "l1");
    struct optics_lens *r3_l2 = optics_gauge_alloc(r3, "l2");
    optics_gauge_set(r3_l1, 2.0);
    optics_gauge_set(r3_l2, 3.0);

    optics_set_reset(&result);
    optics_poller_poll_at(poller, ++ts);
    assert_set_equal(&result, 0,
            make_kv("r2.l1", 1.0),
            make_kv("r3.l1", 2.0),
            make_kv("r3.l2", 3.0));

    struct optics_lens *r1_l1 = optics_gauge_alloc(r1, "l1");
    optics_lens_close(r2_l1);
    optics_close(r2);
    struct optics *r4 = optics_create("r4");
    struct optics_lens *r4_l1 = optics_gauge_alloc(r4, "l1");
    optics_gauge_set(r4_l1, 10.0);

    optics_set_reset(&result);
    optics_poller_poll_at(poller, ++ts);
    assert_set_equal(&result, 0,
            make_kv("r1.l1", 0.0),
            make_kv("r3.l1", 2.0),
            make_kv("r3.l2", 3.0),
            make_kv("r4.l1", 10.0));

    optics_lens_close(r1_l1);
    optics_close(r1);

    optics_lens_close(r3_l1);
    optics_lens_close(r3_l2);
    optics_close(r3);

    optics_lens_close(r4_l1);
    optics_close(r4);

    optics_set_reset(&result);
    optics_poller_poll_at(poller, ++ts);
    assert_int_equal(result.n, 0);

    optics_poller_free(poller);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(poller_nil_test),
        cmocka_unit_test(poller_multi_lens_test),
        cmocka_unit_test(poller_multi_region_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
