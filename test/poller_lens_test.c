/* poller_lens_test.c
   Rémi Attab (remi.attab@gmail.com), 09 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"


// -----------------------------------------------------------------------------
// backend
// -----------------------------------------------------------------------------

void backend_cb(void *ctx, uint64_t ts, const char *key, double value)
{
    (void) ts;

    struct htable *keys = ctx;
    assert_true(htable_put(keys, key, pun_dtoi(value)).ok);
}


// -----------------------------------------------------------------------------
// gauge
// -----------------------------------------------------------------------------

optics_test_head(poller_gauge_test)
{
    struct htable result = {0};
    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_backend(poller, &result, backend_cb, NULL);

    optics_ts_t ts = 0;

    struct optics *optics = optics_create_at(test_name, ts);
    struct optics_lens *gauge = optics_gauge_alloc(optics, "blah");
    optics_set_prefix(optics, "bleh");

    optics_poller_poll_at(poller, ++ts);
    assert_htable_equal(&result, 0, make_kv("bleh.blah", 0.0));

    for (size_t i = 0; i < 3; ++i) {
        ts++;
        optics_gauge_set(gauge, 1.0);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0, make_kv("bleh.blah", 1.0));

        ts++;

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0, make_kv("bleh.blah", 1.0));

        ts++;
        optics_gauge_set(gauge, 1.34e-5);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 1e-7, make_kv("bleh.blah", 1.34e-5));

        ts += 10;
        optics_gauge_set(gauge, 10.0);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0.0, make_kv("bleh.blah", 10.0));
    }

    htable_reset(&result);
    optics_lens_close(gauge);
    optics_close(optics);
    optics_poller_free(poller);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// counter
// -----------------------------------------------------------------------------

optics_test_head(poller_counter_test)
{
    struct htable result = {0};
    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_backend(poller, &result, backend_cb, NULL);

    optics_ts_t ts = 0;

    struct optics *optics = optics_create_at(test_name, ts);
    struct optics_lens *counter = optics_counter_alloc(optics, "blah");
    optics_set_prefix(optics, "bleh");

    optics_poller_poll_at(poller, ++ts);
    assert_htable_equal(&result, 0, make_kv("bleh.blah", 0.0));

    for (size_t i = 0; i < 3; ++i) {
        ts++;
        optics_counter_inc(counter, 1);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0, make_kv("bleh.blah", 1.0));

        ts++;

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0, make_kv("bleh.blah", 0.0));

        ts++;
        for (size_t j = 0; j < 10; ++j)
            optics_counter_inc(counter, 1);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0.0, make_kv("bleh.blah", 10.0));

        ts++;
        for (size_t j = 0; j < 10; ++j)
            optics_counter_inc(counter, -1);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0.0, make_kv("bleh.blah", -10.0));

        ts += 10;
        for (size_t j = 0; j < 20; ++j)
            optics_counter_inc(counter, 3);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0.0, make_kv("bleh.blah", 6.0));
    }

    htable_reset(&result);
    optics_lens_close(counter);
    optics_close(optics);
    optics_poller_free(poller);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// dist
// -----------------------------------------------------------------------------

optics_test_head(poller_dist_test)
{
    struct htable result = {0};
    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_backend(poller, &result, backend_cb, NULL);

    optics_ts_t ts = 0;

    struct optics *optics = optics_create_at(test_name, ts);
    struct optics_lens *dist = optics_dist_alloc(optics, "blah");
    optics_set_prefix(optics, "bleh");

    ts++;

    optics_poller_poll_at(poller, ts);
    assert_htable_equal(&result, 0,
            make_kv("bleh.blah.count", 0.0),
            make_kv("bleh.blah.p50", 0.0),
            make_kv("bleh.blah.p90", 0.0),
            make_kv("bleh.blah.p99", 0.0),
            make_kv("bleh.blah.max", 0.0));

    for (size_t i = 0; i < 3; ++i) {
        ts++;
        optics_dist_record(dist, 1.0);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0,
                make_kv("bleh.blah.count", 1.0),
                make_kv("bleh.blah.p50", 1.0),
                make_kv("bleh.blah.p90", 1.0),
                make_kv("bleh.blah.p99", 1.0),
                make_kv("bleh.blah.max", 1.0));

        ts++;

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0,
                make_kv("bleh.blah.count", 0.0),
                make_kv("bleh.blah.p50", 0.0),
                make_kv("bleh.blah.p90", 0.0),
                make_kv("bleh.blah.p99", 0.0),
                make_kv("bleh.blah.max", 0.0));

        ts++;
        for (size_t i = 0; i < 10; ++i)
            optics_dist_record(dist, i + 1);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0,
                make_kv("bleh.blah.count", 10.0),
                make_kv("bleh.blah.p50", 6.0),
                make_kv("bleh.blah.p90", 10.0),
                make_kv("bleh.blah.p99", 10.0),
                make_kv("bleh.blah.max", 10.0));

        ts += 5;
        for (size_t i = 0; i < 10; ++i)
            optics_dist_record(dist, i + 1);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0,
                make_kv("bleh.blah.count", 2.0),
                make_kv("bleh.blah.p50", 6.0),
                make_kv("bleh.blah.p90", 10.0),
                make_kv("bleh.blah.p99", 10.0),
                make_kv("bleh.blah.max", 10.0));
    }

    htable_reset(&result);
    optics_lens_close(dist);
    optics_close(optics);
    optics_poller_free(poller);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(poller_gauge_test),
        cmocka_unit_test(poller_counter_test),
        cmocka_unit_test(poller_dist_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
