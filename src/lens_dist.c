/* lens_dist.c
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

// -----------------------------------------------------------------------------
// config
// -----------------------------------------------------------------------------

enum { dist_reservoir_len = 1000 };


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
lens_dist_alloc(struct region *region, const char *name)
{
    return lens_alloc(region, optics_dist, sizeof(struct lens_dist), name);
}

static bool
lens_dist_record(struct optics_lens* lens, optics_epoch_t epoch, double value)
{
    struct lens_dist *dist_head = lens_sub_ptr(lens->lens, optics_dist);
    if (!dist_head) return false;

    struct lens_dist_epoch *dist = &dist_head->epochs[epoch];
    slock_lock(&dist->lock);

    size_t i = dist->n;
    if (i >= dist_reservoir_len)
        i = rng_gen_range(rng_global(), 0, dist->n);
    if (i < dist_reservoir_len)
        dist->reservoir[i] = value;

    dist->n++;
    if (value > dist->max) dist->max = value;

    slock_unlock(&dist->lock);
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

    size_t entries;
    double reservoir[dist_reservoir_len];
    {
        // Since we're not locking the active epoch, we shouldn't only contend
        // with straglers which can be delt with by the poller.
        if (!slock_try_lock(&dist->lock)) return optics_busy;

        value->n = dist->n;
        dist->n = 0;

        value->max = dist->max;
        dist->max = 0;

        entries = value->n <= dist_reservoir_len ? value->n : dist_reservoir_len;

        // delay the sorting until we're not holding on the lock.
        memcpy(reservoir, dist->reservoir, entries * sizeof(double));

        slock_unlock(&dist->lock);
    }

    if (entries > 0) {
        qsort(reservoir, entries, sizeof(double), lens_dist_value_cmp);
        value->p50 = reservoir[lens_dist_p(50, entries)];
        value->p90 = reservoir[lens_dist_p(90, entries)];
        value->p99 = reservoir[lens_dist_p(99, entries)];
    }

    return optics_ok;
}
