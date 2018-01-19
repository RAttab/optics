/* poller_test.c
   Rémi Attab (remi.attab@gmail.com), 08 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"


// -----------------------------------------------------------------------------
// backend
// -----------------------------------------------------------------------------

struct backend_ctx
{
    struct htable *keys;
    const struct  optics_poll *poll;
};

bool backend_normalized_cb(void *ctx_, uint64_t ts, const char *key_suffix, double value)
{
    (void) ts;

    struct backend_ctx *ctx = ctx_;

    struct optics_key key = {0};
    optics_key_push(&key, ctx->poll->prefix);
    optics_key_push(&key, ctx->poll->host);
    optics_key_push(&key, key_suffix);

    assert_true(htable_put(ctx->keys, key.data, pun_dtoi(value)).ok);

    return true;
}

void backend_cb(void *ctx, enum optics_poll_type type, const struct optics_poll *poll)
{
    if (type != optics_poll_metric) return;

    struct backend_ctx norm_ctx = { .keys = ctx, .poll = poll};
    (void) optics_poll_normalize(poll, backend_normalized_cb, &norm_ctx);
}


// -----------------------------------------------------------------------------
// nil
// -----------------------------------------------------------------------------

optics_test_head(poller_nil_test)
{
    struct htable result = {0};
    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_backend(poller, &result, backend_cb, NULL);

    for (size_t i = 0; i < 3; ++i) {
        optics_poller_poll_at(poller, i);
        assert_int_equal(result.len, 0);
    }

    optics_poller_free(poller);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// poller multi lens
// -----------------------------------------------------------------------------

optics_test_head(poller_multi_lens_test)
{
    struct htable result = {0};
    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_set_host(poller, "host");
    optics_poller_backend(poller, &result, backend_cb, NULL);

    optics_ts_t ts = 0;

    struct optics *optics = optics_create_at(test_name, ts);
    optics_set_prefix(optics, "prefix");

    struct optics_lens *g1 = optics_gauge_alloc(optics, "g1");
    struct optics_lens *g2 = optics_gauge_alloc(optics, "g2");
    struct optics_lens *g3 = optics_gauge_alloc(optics, "g3");
    optics_gauge_set(g2, 1.0);
    optics_gauge_set(g3, 1.2e-4);

    htable_reset(&result);
    optics_poller_poll_at(poller, ++ts);
    assert_htable_equal(&result, 0,
            make_kv("prefix.host.g1", 0.0),
            make_kv("prefix.host.g2", 1.0),
            make_kv("prefix.host.g3", 1.2e-4));

    struct optics_lens *g4 = optics_gauge_alloc(optics, "g4");
    optics_lens_free(g1);
    optics_gauge_set(g2, 2.0);
    optics_gauge_set(g4, -1.0);

    htable_reset(&result);
    optics_poller_poll_at(poller, ++ts);
    assert_htable_equal(&result, 0,
            make_kv("prefix.host.g2", 2.0),
            make_kv("prefix.host.g3", 1.2e-4),
            make_kv("prefix.host.g4", -1.0));

    g1 = optics_gauge_alloc(optics, "g1");
    optics_gauge_set(g1, 1.0);

    htable_reset(&result);
    optics_poller_poll_at(poller, ++ts);
    assert_htable_equal(&result, 0,
            make_kv("prefix.host.g1", 1.0),
            make_kv("prefix.host.g2", 2.0),
            make_kv("prefix.host.g3", 1.2e-4),
            make_kv("prefix.host.g4", -1.0));

    optics_lens_close(g1);
    optics_lens_close(g2);
    optics_lens_close(g3);
    optics_lens_close(g4);
    optics_close(optics);

    htable_reset(&result);
    optics_poller_poll_at(poller, ++ts);
    assert_int_equal(result.len, 0);

    optics_poller_free(poller);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// poller multi region
// -----------------------------------------------------------------------------

#if 0 // \todo redo me.

optics_test_head(poller_multi_region_test)
{
    struct htable result = {0};
    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_set_host(poller, "h");
    optics_poller_backend(poller, &result, backend_cb, NULL);

    optics_ts_t ts = 0;
    struct optics *r1 = optics_create_at("r1", ts);
    optics_set_source(r1, "s1");

    struct optics *r2 = optics_create_at("r2", ts);
    optics_set_source(r2, "s2");
    struct optics_lens *r2_l1 = optics_gauge_alloc(r2, "l1");
    optics_gauge_set(r2_l1, 1.0);

    struct optics *r3 = optics_create_at("r3", ts);
    optics_set_source(r3, "s3");
    struct optics_lens *r3_l1 = optics_gauge_alloc(r3, "l1");
    struct optics_lens *r3_l2 = optics_gauge_alloc(r3, "l2");
    optics_gauge_set(r3_l1, 2.0);
    optics_gauge_set(r3_l2, 3.0);

    htable_reset(&result);
    optics_poller_poll_at(poller, ++ts);
    assert_htable_equal(&result, 0,
            make_kv("r2.h.s2.l1", 1.0),
            make_kv("r3.h.s3.l1", 2.0),
            make_kv("r3.h.s3.l2", 3.0));

    struct optics_lens *r1_l1 = optics_gauge_alloc(r1, "l1");
    optics_lens_close(r2_l1);
    optics_close(r2);
    struct optics *r4 = optics_create_at("r4", ts);
    optics_set_source(r4, "s4");
    struct optics_lens *r4_l1 = optics_gauge_alloc(r4, "l1");
    optics_gauge_set(r4_l1, 10.0);

    htable_reset(&result);
    optics_poller_poll_at(poller, ++ts);
    assert_htable_equal(&result, 0,
            make_kv("r1.h.s1.l1", 0.0),
            make_kv("r3.h.s3.l1", 2.0),
            make_kv("r3.h.s3.l2", 3.0),
            make_kv("r4.h.s4.l1", 10.0));

    optics_lens_close(r1_l1);
    optics_close(r1);

    optics_lens_close(r3_l1);
    optics_lens_close(r3_l2);
    optics_close(r3);

    optics_lens_close(r4_l1);
    optics_close(r4);

    htable_reset(&result);
    optics_poller_poll_at(poller, ++ts);
    assert_int_equal(result.len, 0);

    optics_poller_free(poller);
}
optics_test_tail()

#endif


// -----------------------------------------------------------------------------
// poller_freq_test
// -----------------------------------------------------------------------------

optics_test_head(poller_freq_test)
{
    struct htable result = {0};
    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_set_host(poller, "h");
    optics_poller_backend(poller, &result, backend_cb, NULL);

    optics_ts_t ts = 0;

    struct optics *optics = optics_create_at("r", 20);
    struct optics_lens *lens = optics_counter_alloc(optics, "l");

    ts += 10;
    optics_counter_inc(lens, 10);

    // If the optics ts is greater then the poll ts we default back to 1 as the
    // elapsed time.
    fprintf(stderr, "\n--- EXPECTED WARNING - START ---\n");
    htable_reset(&result);
    assert_true(optics_poller_poll_at(poller, ts));
    assert_htable_equal(&result, 0, make_kv("r.h.s.l", 10));
    fprintf(stderr, "--- EXPECTED WARNING - END ---\n\n");

    ts += 10;
    optics_counter_inc(lens, 10);

    htable_reset(&result);
    assert_true(optics_poller_poll_at(poller, ts));
    assert_htable_equal(&result, 0, make_kv("r.h.l", 1));

    ts += 10;
    optics_counter_inc(lens, 10);

    htable_reset(&result);
    assert_true(optics_poller_poll_at(poller, ts));
    assert_htable_equal(&result, 0, make_kv("r.h.l", 1));

    ts += 0; // not a mistake
    optics_counter_inc(lens, 10);

    // If the ts is 0 then elapsed is adjusted back to 1.
    htable_reset(&result);
    assert_true(optics_poller_poll_at(poller, ts));
    assert_htable_equal(&result, 0, make_kv("r.h.l", 10));

    htable_reset(&result);
    optics_lens_close(lens);
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
        cmocka_unit_test(poller_nil_test),
        cmocka_unit_test(poller_multi_lens_test),
//        cmocka_unit_test(poller_multi_region_test),
        cmocka_unit_test(poller_freq_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
