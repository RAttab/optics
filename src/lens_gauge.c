/* lens_gauge.c
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct optics_packed lens_gauge
{
    atomic_uint64_t value;
};


// -----------------------------------------------------------------------------
// impl
// -----------------------------------------------------------------------------

static struct lens *
lens_gauge_alloc(struct region *region, const char *name)
{
    return lens_alloc(region, optics_gauge, sizeof(struct lens_gauge), name);
}

static bool
lens_gauge_set(struct optics_lens *lens, optics_epoch_t epoch, double value)
{
    (void) epoch;

    struct lens_gauge *gauge = lens_sub_ptr(lens->lens, sizeof(struct lens_gauge));
    if (!gauge) return false;

    atomic_fetch_add_explicit(&gauge->value, (uint64_t) value, memory_order_relaxed);
    return true;
}

static enum optics_ret
lens_gauge_read(struct optics_lens *lens, optics_epoch_t epoch, double *value)
{
    (void) epoch;

    struct lens_gauge *gauge = lens_sub_ptr(lens->lens, sizeof(struct lens_gauge));
    if (!gauge) return optics_err;

    *result = (double) atomic_load_explicit(&gauge->value, memory_order_relaxed);
    return optics_ok;
}
