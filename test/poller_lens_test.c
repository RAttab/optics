/* poller_lens_test.c
   Rémi Attab (remi.attab@gmail.com), 09 Mar 2016
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
// gauge
// -----------------------------------------------------------------------------

optics_test_head(poller_gauge_test)
{
    struct htable result = {0};
    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_set_host(poller, "host");
    optics_poller_backend(poller, &result, backend_cb, NULL);

    optics_ts_t ts = 0;

    struct optics *optics = optics_create_at(test_name, ts);
    struct optics_lens *gauge = optics_gauge_alloc(optics, "gauge");
    optics_set_prefix(optics, "prefix");

    optics_poller_poll_at(poller, ++ts);
    assert_htable_equal(&result, 0, make_kv("prefix.host.gauge", 0.0));

    for (size_t i = 0; i < 3; ++i) {
        ts++;
        optics_gauge_set(gauge, 1.0);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0, make_kv("prefix.host.gauge", 1.0));

        ts++;

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0, make_kv("prefix.host.gauge", 1.0));

        ts++;
        optics_gauge_set(gauge, 1.34e-5);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 1e-7, make_kv("prefix.host.gauge", 1.34e-5));

        ts += 10;
        optics_gauge_set(gauge, 10.0);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0.0, make_kv("prefix.host.gauge", 10.0));
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
    optics_poller_set_host(poller, "host");
    optics_poller_backend(poller, &result, backend_cb, NULL);

    optics_ts_t ts = 0;

    struct optics *optics[2];
    for (size_t i = 0; i < 2; ++i) {
        optics[i] = optics_create_idx_at(test_name, i, ts);
        optics_set_prefix(optics[i], "prefix");
    }

    struct optics_lens *l0 = optics_counter_alloc(optics[0], "counter");
    struct optics_lens *l1 = optics_counter_alloc(optics[1], "counter");

    optics_poller_poll_at(poller, ++ts);
    assert_htable_equal(&result, 0, make_kv("prefix.host.counter", 0.0));

    for (size_t i = 0; i < 3; ++i) {
        ts++;
        optics_counter_inc(l0, 1);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0, make_kv("prefix.host.counter", 1.0));

        ts++;

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0, make_kv("prefix.host.counter", 0.0));

        ts++;
        for (size_t j = 0; j < 10; ++j)
            optics_counter_inc(l0, 1);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0.0, make_kv("prefix.host.counter", 10.0));

        ts++;
        for (size_t j = 0; j < 10; ++j)
            optics_counter_inc(l0, -1);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0.0, make_kv("prefix.host.counter", -10.0));

        ts += 10;
        for (size_t j = 0; j < 20; ++j)
            optics_counter_inc(l0, 3);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0.0, make_kv("prefix.host.counter", 6.0));

        ts++;
        optics_counter_inc(l0, 1);
        optics_counter_inc(l1, 20);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0.0, make_kv("prefix.host.counter", 21));
    }

    htable_reset(&result);
    optics_lens_close(l0);
    optics_lens_close(l1);
    for (size_t i = 0; i < 2; ++i) optics_close(optics[i]);
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
    optics_poller_set_host(poller, "host");
    optics_poller_backend(poller, &result, backend_cb, NULL);

    optics_ts_t ts = 0;

    struct optics *optics[2];
    for (size_t i = 0; i < 2; ++i) {
        optics[i] = optics_create_idx_at(test_name, i, ts);
        optics_set_prefix(optics[i], "prefix");
    }

    struct optics_lens *l0 = optics_dist_alloc(optics[0], "dist");
    struct optics_lens *l1 = optics_dist_alloc(optics[1], "dist");

    ts++;

    optics_poller_poll_at(poller, ts);
    assert_htable_equal(&result, 0,
            make_kv("prefix.host.dist.count", 0.0),
            make_kv("prefix.host.dist.p50", 0.0),
            make_kv("prefix.host.dist.p90", 0.0),
            make_kv("prefix.host.dist.p99", 0.0),
            make_kv("prefix.host.dist.max", 0.0));

    for (size_t i = 0; i < 3; ++i) {
        ts++;
        optics_dist_record(l0, 1.0);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0,
                make_kv("prefix.host.dist.count", 1.0),
                make_kv("prefix.host.dist.p50", 1.0),
                make_kv("prefix.host.dist.p90", 1.0),
                make_kv("prefix.host.dist.p99", 1.0),
                make_kv("prefix.host.dist.max", 1.0));

        ts++;

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0,
                make_kv("prefix.host.dist.count", 0.0),
                make_kv("prefix.host.dist.p50", 0.0),
                make_kv("prefix.host.dist.p90", 0.0),
                make_kv("prefix.host.dist.p99", 0.0),
                make_kv("prefix.host.dist.max", 0.0));

        ts++;
        for (size_t i = 0; i < 10; ++i)
            optics_dist_record(l0, i + 1);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0,
                make_kv("prefix.host.dist.count", 10.0),
                make_kv("prefix.host.dist.p50", 6.0),
                make_kv("prefix.host.dist.p90", 10.0),
                make_kv("prefix.host.dist.p99", 10.0),
                make_kv("prefix.host.dist.max", 10.0));

        ts += 5;
        for (size_t i = 0; i < 10; ++i)
            optics_dist_record(l0, i + 1);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0,
                make_kv("prefix.host.dist.count", 2.0),
                make_kv("prefix.host.dist.p50", 6.0),
                make_kv("prefix.host.dist.p90", 10.0),
                make_kv("prefix.host.dist.p99", 10.0),
                make_kv("prefix.host.dist.max", 10.0));

        ts++;
        for (size_t i = 0; i < 10; ++i) {
            optics_dist_record(l0, i + 1);
            optics_dist_record(l1, i + 1);
        }

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0,
                make_kv("prefix.host.dist.count", 20.0),
                make_kv("prefix.host.dist.p50", 6.0),
                make_kv("prefix.host.dist.p90", 10.0),
                make_kv("prefix.host.dist.p99", 10.0),
                make_kv("prefix.host.dist.max", 10.0));
    }

    htable_reset(&result);
    optics_lens_close(l0);
    optics_lens_close(l1);
    for (size_t i = 0; i < 2; ++i) optics_close(optics[i]);
    optics_poller_free(poller);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// histo
// -----------------------------------------------------------------------------

optics_test_head(poller_histo_test)
{
    struct htable result = {0};
    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_set_host(poller, "host");
    optics_poller_backend(poller, &result, backend_cb, NULL);

    optics_ts_t ts = 0;

    struct optics *optics[2];
    for (size_t i = 0; i < 2; ++i) {
        optics[i] = optics_create_idx_at(test_name, i, ts);
        optics_set_prefix(optics[i], "prefix");
    }

    const double buckets[] = {1, 2, 3};
    struct optics_lens *l0 = optics_histo_alloc(optics[0], "histo", buckets, 3);
    struct optics_lens *l1 = optics_histo_alloc(optics[1], "histo", buckets, 3);

    ts++;

    optics_poller_poll_at(poller, ts);
    assert_htable_equal(&result, 0,
            make_kv("prefix.host.histo.below", 0.0),
            make_kv("prefix.host.histo.bucket_1_2", 0.0),
            make_kv("prefix.host.histo.bucket_2_3", 0.0),
            make_kv("prefix.host.histo.above", 0.0));

    for (size_t i = 0; i < 3; ++i) {
        ts++;
        optics_histo_inc(l0, 1.0);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0,
                make_kv("prefix.host.histo.below", 0.0),
                make_kv("prefix.host.histo.bucket_1_2", 1.0),
                make_kv("prefix.host.histo.bucket_2_3", 0.0),
                make_kv("prefix.host.histo.above", 0.0));

        ts++;

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0,
                make_kv("prefix.host.histo.below", 0.0),
                make_kv("prefix.host.histo.bucket_1_2", 0.0),
                make_kv("prefix.host.histo.bucket_2_3", 0.0),
                make_kv("prefix.host.histo.above", 0.0));

        ts++;
        for (size_t i = 0; i < 80; ++i)
            optics_histo_inc(l0, i % 4);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0,
                make_kv("prefix.host.histo.below", 20.0),
                make_kv("prefix.host.histo.bucket_1_2", 20.0),
                make_kv("prefix.host.histo.bucket_2_3", 20.0),
                make_kv("prefix.host.histo.above", 20.0));

        ts += 5;
        for (size_t i = 0; i < 80; ++i)
            optics_histo_inc(l0, i % 4);

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0,
                make_kv("prefix.host.histo.below", 4.0),
                make_kv("prefix.host.histo.bucket_1_2", 4.0),
                make_kv("prefix.host.histo.bucket_2_3", 4.0),
                make_kv("prefix.host.histo.above", 4.0));

        ts++;
        for (size_t i = 0; i < 80; ++i) {
            optics_histo_inc(l0, i % 4);
            optics_histo_inc(l1, i % 4);
        }

        htable_reset(&result);
        optics_poller_poll_at(poller, ts);
        assert_htable_equal(&result, 0,
                make_kv("prefix.host.histo.below", 40.0),
                make_kv("prefix.host.histo.bucket_1_2", 40.0),
                make_kv("prefix.host.histo.bucket_2_3", 40.0),
                make_kv("prefix.host.histo.above", 40.0));

    }

    htable_reset(&result);
    optics_lens_close(l0);
    optics_lens_close(l1);
    for (size_t i = 0; i < 2; ++i) optics_close(optics[i]);
    optics_poller_free(poller);
}
optics_test_tail()

// -----------------------------------------------------------------------------
// quantile
// -----------------------------------------------------------------------------

optics_test_head(poller_quantile_test)
{
    struct htable result = {0};
    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_set_host(poller, "host");
    optics_poller_backend(poller, &result, backend_cb, NULL);

    optics_ts_t ts = 0;

    struct optics *optics = optics_create_at(test_name, ts);
    struct optics_lens *quantile = optics_quantile_alloc(optics, "quantile", 0.9, 50, 0.05);
    optics_set_prefix(optics, "prefix");
    optics_set_source(optics, "source");

    optics_poller_poll_at(poller, ++ts);
    assert_htable_equal(&result, 0, make_kv("prefix.host.source.quantile", 50));
 
     for(size_t i = 0; i < 1000; i++){
        for (size_t j = 0; j < 100; j++){
            optics_quantile_update(quantile, j);
        }
    }

    ts += 10;

    htable_reset(&result);
    optics_poller_poll_at(poller, ts);
    assert_htable_equal(&result, 1, make_kv("prefix.host.source.quantile", 90));

    htable_reset(&result);
    optics_lens_close(quantile);
    optics_close(optics);
    optics_poller_free(poller);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    rng_seed_with(rng_global(), 0);

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(poller_gauge_test),
        cmocka_unit_test(poller_counter_test),
        cmocka_unit_test(poller_dist_test),
        cmocka_unit_test(poller_histo_test),
        cmocka_unit_test(poller_quantile_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
