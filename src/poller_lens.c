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
        optics_warn_err("unable to read counter '%s'", ctx->key->buffer);
        return;
    }

    poller_backend_record(ctx->poller, ctx->ts, ctx->key->buffer, value);
}


// -----------------------------------------------------------------------------
// gauge
// -----------------------------------------------------------------------------

static void poller_poll_gauge(struct poller_poll_ctx *ctx, struct optics_lens *lens)
{
    double value;
    if (optics_gauge_read(lens, ctx->epoch, &value) != optics_ok) {
        optics_warn_err("unable to read gauge '%s'", ctx->key->buffer);
        return;
    }

    poller_backend_record(ctx->poller, ctx->ts, ctx->key->buffer, value);
}


// -----------------------------------------------------------------------------
// dist
// -----------------------------------------------------------------------------

static void poller_poll_dist(struct poller_poll_ctx *ctx, struct optics_lens *lens)
{
    struct optics_dist value;
    enum optics_ret ret = optics_dist_read(lens, ctx->epoch, &value);
    if (ret == optics_busy) {
        optics_warn("skipping lens '%s'", ctx->key->buffer);
        return;
    }
    else if (ret == optics_err) {
        optics_warn_err("unable to read dist '%s'", ctx->key->buffer);
        return;
    }

    size_t old;

    old = key_push(ctx->key, "count");
    poller_backend_record(ctx->poller, ctx->ts, ctx->key->buffer, value->n);
    key_pop(ctx->key, old);

    old = key_push(ctx->key, "p50");
    poller_backend_record(ctx->poller, ctx->ts, ctx->key->buffer, value->p50);
    key_pop(ctx->key, old);

    old = key_push(ctx->key, "p90");
    poller_backend_record(ctx->poller, ctx->ts, ctx->key->buffer, value->p90);
    key_pop(ctx->key, old);

    old = key_push(ctx->key, "p99");
    poller_backend_record(ctx->poller, ctx->ts, ctx->key->buffer, value->p99);
    key_pop(ctx->key, old);

    old = key_push(ctx->key, "max");
    poller_backend_record(ctx->poller, ctx->ts, ctx->key->buffer, value->max);
    key_pop(ctx->key, old);
}
