/* lens_dist.c
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

// -----------------------------------------------------------------------------
// config
// -----------------------------------------------------------------------------

// The size here is a trade-off between memory usage and the growth rate of the
// error bounds as more elements are added to the reservoir. Since we're
// calculating percentiles, we need at least 100 values which requires a
// non-trivial amount space (sizeof(double) * 100 * 2 = 1600 bytes). Now since
// there's no way to achieve a constant error bound with reservoir sampling, we
// tweaked it to stay on the low side of memory consumption.
enum { dist_reservoir_len = 200 };


// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct optics_packed lens_dist_epoch
{
    struct slock lock;

    size_t n;
    double max;
    double reservoir[dist_reservoir_len];
};

struct optics_packed lens_dist
{
    struct lens_dist_epoch epochs[2];
};


// -----------------------------------------------------------------------------
// impl
// -----------------------------------------------------------------------------


static struct lens *
lens_dist_alloc(struct optics *optics, const char *name)
{
    return lens_alloc(optics, optics_dist, sizeof(struct lens_dist), name);
}

static bool
lens_dist_record(struct optics_lens* lens, optics_epoch_t epoch, double value)
{
    struct lens_dist *dist_head = lens_sub_ptr(lens->lens, optics_dist);
    if (!dist_head) return false;

    struct lens_dist_epoch *dist = &dist_head->epochs[epoch];
    {
        slock_lock(&dist->lock);

        size_t i = dist->n;
        if (i >= dist_reservoir_len)
            i = rng_gen_range(rng_global(), 0, dist->n);
        if (i < dist_reservoir_len)
            dist->reservoir[i] = value;

        dist->n++;
        if (value > dist->max) dist->max = value;

        slock_unlock(&dist->lock);
    }
    return true;
}


static int lens_dist_value_cmp(const void *lhs, const void *rhs)
{
    return *((double *) lhs) - *((double *) rhs);
}

static inline size_t lens_dist_p(size_t percentile, size_t n)
{
    return (n * percentile) / 100;
}

static enum optics_ret
lens_dist_read(struct optics_lens *lens, optics_epoch_t epoch, struct optics_dist *value)
{
    struct lens_dist *dist_head = lens_sub_ptr(lens->lens, optics_dist);
    if (!dist_head) return optics_err;

    struct lens_dist_epoch *dist = &dist_head->epochs[epoch];
    memset(value, 0, sizeof(*value));

    double reservoir[dist_reservoir_len];
    {
        // Since we're not locking the active epoch, we shouldn't only contend
        // with straglers which can be dealt with by the poller.
        if (slock_is_locked(&dist->lock)) return optics_busy;

        value->n = dist->n;
        dist->n = 0;

        value->max = dist->max;
        dist->max = 0;

        // delay the sorting until we're not holding on the lock.
        memcpy(reservoir, dist->reservoir, value->n * sizeof(double));

        slock_unlock(&dist->lock);
    }

    if (value->n > 0) {
        qsort(reservoir, value->n, sizeof(double), lens_dist_value_cmp);
        value->p50 = reservoir[lens_dist_p(50, value->n)];
        value->p90 = reservoir[lens_dist_p(90, value->n)];
        value->p99 = reservoir[lens_dist_p(99, value->n)];
    }

    return optics_ok;
}

static bool
lens_dist_normalize(
        const struct optics_poll *poll, optics_normalize_cb_t cb, void *ctx)
{
    bool ret = false;
    size_t old;

    old = optics_key_push(poll->key, "count");
    double rescaled = (double) poll->value.dist.n / poll->elapsed;
    ret = cb(ctx, poll->ts, poll->key->data, rescaled);
    optics_key_pop(poll->key, old);
    if (!ret) return false;

    old = optics_key_push(poll->key, "p50");
    ret = cb(ctx, poll->ts, poll->key->data, poll->value.dist.p50);
    optics_key_pop(poll->key, old);
    if (!ret) return false;

    old = optics_key_push(poll->key, "p90");
    ret = cb(ctx, poll->ts, poll->key->data, poll->value.dist.p90);
    optics_key_pop(poll->key, old);
    if (!ret) return false;

    old = optics_key_push(poll->key, "p99");
    ret = cb(ctx, poll->ts, poll->key->data, poll->value.dist.p99);
    optics_key_pop(poll->key, old);
    if (!ret) return false;

    old = optics_key_push(poll->key, "max");
    ret = cb(ctx, poll->ts, poll->key->data, poll->value.dist.max);
    optics_key_pop(poll->key, old);
    if (!ret) return false;

    return true;
}
