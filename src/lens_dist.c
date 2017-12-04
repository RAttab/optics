/* lens_dist.c
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct optics_packed lens_dist_epoch
{
    struct slock lock;

    size_t n;
    double max;
    double samples[optics_dist_samples];
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
        if (i >= optics_dist_samples)
            i = rng_gen_range(rng_global(), 0, dist->n);
        if (i < optics_dist_samples)
            dist->samples[i] = value;

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

static size_t lens_dist_merge(
        double *dst,
        const double *lhs, size_t lhs_len,
        const double *rhs, size_t rhs_len)
{
    size_t dst_len = 0;
    const double *to_merge = NULL; size_t to_merge_len = 0;

    if (lhs_len >= rhs_len) {
        dst_len = lhs_len;
        memcpy(dst, lhs, lhs_len * sizeof(*lhs));

        to_merge = rhs;
        to_merge_len = rhs_len;
    }
    else {
        dst_len = rhs_len;
        memcpy(dst, rhs, rhs_len * sizeof(*rhs));

        to_merge = lhs;
        to_merge_len = lhs_len;
    }

    assert(to_merge_len <= dst_len);
    if (!to_merge_len) return dst_len;

    // Fill up our reservoir if not already full.
    if (dst_len < optics_dist_samples) {
        size_t to_copy = optics_dist_samples - dst_len;
        if (to_copy > to_merge_len) to_copy = to_merge_len;

        memcpy(dst + dst_len, to_merge, to_copy * sizeof(*lhs));
        to_merge += to_copy;
        to_merge_len -= to_copy;

        if (!to_merge_len) return dst_len;
    }

    // We have non-sampled data so use the regular sampling method
    if (to_merge_len <= optics_dist_samples) {
        for (size_t i = 0; i < to_merge_len; ++i) {
            size_t index = rng_gen_range(&rng_global(), 0, dst_len);
            if (index < optics_dist_samples)
                dst[index] = to_merge[i];
            dst_len++;
        }
    }

    // We have two sampled set so pick from each set with proportion equal to
    // the number of values they represent.
    else {
        const double rate = (double) to_merge_len / (double) (to_merge_len + dst_len);
        for (size_t i = 0; i < optics_dist_samples; ++i) {
            if (rng_gen_prob(rng_global(), rate))
                dst[i] = to_merge[i];
        }
    }

    return optics_dist_samples;
}

static enum optics_ret
lens_dist_read(struct optics_lens *lens, optics_epoch_t epoch, struct optics_dist *value)
{
    struct lens_dist *dist_head = lens_sub_ptr(lens->lens, optics_dist);
    if (!dist_head) return optics_err;

    struct lens_dist_epoch *dist = &dist_head->epochs[epoch];
    memset(value, 0, sizeof(*value));

    size_t value_len = value->n, dist_len = 0;
    double samples[optics_dist_samples];
    {
        // Since we're not locking the active epoch, we shouldn't only contend
        // with straglers which can be dealt with by the poller.
        if (slock_is_locked(&dist->lock)) return optics_busy;

        value->n += dist->n;
        dist->n = 0;

        if (value->max < dist->max) value->max = dist->max;
        dist->max = 0;

        dist_len = value->n <= optics_dist_samples ? value->n : optics_dist_samples;

        // delay the sorting until we're not holding on the lock.
        memcpy(samples, dist->samples, dist_len * sizeof(double));

        slock_unlock(&dist->lock);
    }

    if (!dist_len) return optics_ok;

    double result[optics_dist_samples];
    size_t result_len = lens_dist_merge(result, samples, dist_len, value->samples, value_len);

    memcpy(value->samples, result, result_len * sizeof(double));
    qsort(result, result_len, sizeof(double), lens_dist_value_cmp);

    value->p50 = result[lens_dist_p(50, result_len)];
    value->p90 = result[lens_dist_p(90, result_len)];
    value->p99 = result[lens_dist_p(99, result_len)];

    return optics_ok;
}


static bool
lens_dist_normalize(
        const struct optics_poll *poll, optics_normalize_cb_t cb, void *ctx)
{
    bool ret = false;
    size_t old;

    struct optics_key key = {0};
    optics_key_push(&key, poll->key);

    old = optics_key_push(&key, "count");
    ret = cb(ctx, poll->ts, &key->data, lens_rescale(poll, poll->value.dist.n));
    optics_key_pop(&key, old);
    if (!ret) return false;

    old = optics_key_push(&key, "p50");
    ret = cb(ctx, poll->ts, &key->data, poll->value.dist.p50);
    optics_key_pop(&key, old);
    if (!ret) return false;

    old = optics_key_push(&key, "p90");
    ret = cb(ctx, poll->ts, &key->data, poll->value.dist.p90);
    optics_key_pop(&key, old);
    if (!ret) return false;

    old = optics_key_push(&key, "p99");
    ret = cb(ctx, poll->ts, &key->data, poll->value.dist.p99);
    optics_key_pop(&key, old);
    if (!ret) return false;

    old = optics_key_push(&key, "max");
    ret = cb(ctx, poll->ts, &key->data, poll->value.dist.max);
    optics_key_pop(&key, old);
    if (!ret) return false;

    return true;
}
