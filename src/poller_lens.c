/* poller_lens.c
   Rémi Attab (remi.attab@gmail.com), 26 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/


// -----------------------------------------------------------------------------
// counter
// -----------------------------------------------------------------------------

static void poller_poll_counter(struct poller_poll_ctx *ctx, struct optics_lens *lens)
{
    int64_t value;
    if (optics_counter_read(lens, ctx->epoch, &value) != optics_ok) {
        optics_warn("unable to read counter '%s': %s", ctx->key->data, optics_errno.msg);
        return;
    }

    value /= (int64_t) ctx->elapsed;
    poller_backend_record(ctx->poller, ctx->ts, ctx->key->data, value);
}


// -----------------------------------------------------------------------------
// gauge
// -----------------------------------------------------------------------------

static void poller_poll_gauge(struct poller_poll_ctx *ctx, struct optics_lens *lens)
{
    double value;
    if (optics_gauge_read(lens, ctx->epoch, &value) != optics_ok) {
        optics_warn("unable to read gauge '%s': %s", ctx->key->data, optics_errno.msg);
        return;
    }

    poller_backend_record(ctx->poller, ctx->ts, ctx->key->data, value);
}


// -----------------------------------------------------------------------------
// dist
// -----------------------------------------------------------------------------

static void poller_poll_dist(struct poller_poll_ctx *ctx, struct optics_lens *lens)
{
    struct optics_dist value;
    enum optics_ret ret = optics_dist_read(lens, ctx->epoch, &value);
    if (ret == optics_busy) {
        optics_warn("skipping lens '%s'", ctx->key->data);
        return;
    }
    else if (ret == optics_err) {
        optics_warn("unable to read dist '%s': %s", ctx->key->data, optics_errno.msg);
        return;
    }

    size_t old;

    old = optics_key_push(ctx->key, "count");
    poller_backend_record(ctx->poller, ctx->ts, ctx->key->data, value.n / ctx->elapsed);
    optics_key_pop(ctx->key, old);

    old = optics_key_push(ctx->key, "p50");
    poller_backend_record(ctx->poller, ctx->ts, ctx->key->data, value.p50);
    optics_key_pop(ctx->key, old);

    old = optics_key_push(ctx->key, "p90");
    poller_backend_record(ctx->poller, ctx->ts, ctx->key->data, value.p90);
    optics_key_pop(ctx->key, old);

    old = optics_key_push(ctx->key, "p99");
    poller_backend_record(ctx->poller, ctx->ts, ctx->key->data, value.p99);
    optics_key_pop(ctx->key, old);

    old = optics_key_push(ctx->key, "max");
    poller_backend_record(ctx->poller, ctx->ts, ctx->key->data, value.max);
    optics_key_pop(ctx->key, old);
}
