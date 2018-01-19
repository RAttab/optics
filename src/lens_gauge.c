/* lens_gauge.c
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply

   \todo Doesn't respect epochs because it must retain it's previous value
   across epoch changes even if no records happen.
*/

// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct optics_packed lens_gauge
{
    atomic_uint_fast64_t value;
};

static_assert(sizeof(atomic_uint_fast64_t) == sizeof(double),
        "gauge storage needs to be the size of a double");

static_assert(sizeof(uint64_t) == sizeof(double),
        "if this failed then sucks to be you");


// -----------------------------------------------------------------------------
// impl
// -----------------------------------------------------------------------------

static struct lens *
lens_gauge_alloc(struct optics *optics, const char *name)
{
    return lens_alloc(optics, optics_gauge, sizeof(struct lens_gauge), name);
}

static bool
lens_gauge_set(struct optics_lens *lens, optics_epoch_t epoch, double value)
{
    (void) epoch;

    struct lens_gauge *gauge = lens_sub_ptr(lens->lens, optics_gauge);
    if (!gauge) return false;

    atomic_store_explicit(&gauge->value, pun_dtoi(value), memory_order_relaxed);
    return true;
}

static enum optics_ret
lens_gauge_read(struct optics_lens *lens, optics_epoch_t epoch, double *value)
{
    (void) epoch;

    struct lens_gauge *gauge = lens_sub_ptr(lens->lens, optics_gauge);
    if (!gauge) return optics_err;

    uint64_t result = atomic_load_explicit(&gauge->value, memory_order_relaxed);
    *value = pun_itod(result);

    return optics_ok;
}


static bool
lens_gauge_normalize(
        const struct optics_poll *poll, optics_normalize_cb_t cb, void *ctx)
{
    return cb(ctx, poll->ts, poll->key, poll->value.gauge);
}
