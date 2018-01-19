/* lens_counter.c
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/


// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct optics_packed lens_counter
{
    atomic_int_fast64_t value[2];
};


// -----------------------------------------------------------------------------
// impl
// -----------------------------------------------------------------------------

static struct lens *
lens_counter_alloc(struct optics *optics, const char *name)
{
    return lens_alloc(optics, optics_counter, sizeof(struct lens_counter), name);
}

static bool
lens_counter_inc(struct optics_lens *lens, optics_epoch_t epoch, int64_t value)
{
    struct lens_counter *counter = lens_sub_ptr(lens->lens, optics_counter);
    if (!counter) return false;

    atomic_fetch_add_explicit(&counter->value[epoch], value, memory_order_relaxed);
    return true;
}

static enum optics_ret
lens_counter_read(struct optics_lens *lens, optics_epoch_t epoch, int64_t *value)
{
    struct lens_counter *counter = lens_sub_ptr(lens->lens, optics_counter);
    if (!counter) return optics_err;

    *value += atomic_exchange_explicit(&counter->value[epoch], 0, memory_order_relaxed);
    return optics_ok;
}

static bool
lens_counter_normalize(
        const struct optics_poll *poll, optics_normalize_cb_t cb, void *ctx)
{
    return cb(ctx, poll->ts, poll->key, lens_rescale(poll, poll->value.counter));
}
