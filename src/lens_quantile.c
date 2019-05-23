/* lens_quantile.c
   Marina C., 19 Feb 2018
   FreeBSD-style copyright and disclaimer apply
*/

// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct optics_packed lens_quantile
{
     double target_quantile;
     double original_estimate;
     double adjustment_value;
     atomic_int_fast64_t multiplier;
     atomic_int_fast64_t count[2];
};

// -----------------------------------------------------------------------------
// impl
// -----------------------------------------------------------------------------

static struct lens *
lens_quantile_alloc(
        struct optics *optics,
        const char *name,
        double target_quantile,
        double original_estimate,
        double adjustment_value)
{
    struct lens *lens = lens_alloc(optics, optics_quantile, sizeof(struct lens_quantile), name);
    if (!lens) goto fail_alloc;

    struct lens_quantile *quantile = lens_sub_ptr(lens, optics_quantile);
    if (!quantile) goto fail_sub;

    quantile->target_quantile = target_quantile;
    quantile->original_estimate = original_estimate;
    quantile->adjustment_value = adjustment_value;

    return lens;

  fail_sub:
    lens_free(optics, lens);
  fail_alloc:
    return NULL;
}

static double calculate_quantile(struct lens_quantile *quantile)
{
    double adjustment =
        atomic_load_explicit(&quantile->multiplier, memory_order_relaxed) *
        quantile->adjustment_value;

    return quantile->original_estimate + adjustment;
}

static bool
lens_quantile_update(struct optics_lens *lens, optics_epoch_t epoch, double value)
{
    struct lens_quantile *quantile = lens_sub_ptr(lens->lens, optics_quantile);
    if (!quantile) return false;

    double current_estimate = calculate_quantile(quantile);
    bool probability_check = rng_gen_prob(rng_global(), quantile->target_quantile);

    if (value < current_estimate) {
        if (!probability_check)
            atomic_fetch_sub_explicit(&quantile->multiplier, 1, memory_order_relaxed);
    }
    else {
        if (probability_check)
             atomic_fetch_add_explicit(&quantile->multiplier, 1, memory_order_relaxed);
    }

    // Since we don't care too much how exact the count is (not used to modify
    // our estimates) then the write ordering doesn't matter so relaxed is fine.
    atomic_fetch_add_explicit(&quantile->count[epoch], 1, memory_order_relaxed);

    return true;
}

static enum optics_ret
lens_quantile_read(
        struct optics_lens *lens, optics_epoch_t epoch, struct optics_quantile *value)
{
    struct lens_quantile *quantile = lens_sub_ptr(lens->lens, optics_quantile);
    if (!quantile) return optics_err;

    double sample = calculate_quantile(quantile);
    size_t count = atomic_exchange_explicit(&quantile->count[epoch], 0, memory_order_relaxed);

    if (!value->quantile) {
        value->quantile = quantile->target_quantile;
        value->sample = sample;
        value->sample_count = value->count = count;
        return optics_ok;
    }
    else if (value->quantile != quantile->target_quantile) return optics_err;

    /* We basically have no good option for merging here which means that we
       have to somehow find the most representative sample without knowing how
       many samples we'll end up seeing.

       The reasoning behind our current (probably terrible) approach:

       1) Averaging quantiles is bad because it reintroduces the biases of large
       extremum values. You end up with the worst of both mesuring methods.

       2) If we have to pick one of the samples we see then ideally we want to
       introduce some randomness to make sure that all the values we see have a
       fair chance of being represented. This avoids bias that could be
       introduced by one the value sources consistently having a slightly higher
       sampling count with a skewed sample.

       3) Should weight for the number of samples being represented. A sample
       that represents a million values should take precendence over one that
       represents ten as it's more likely to be accurate.

       4) Probability should be skewed in favour of the quantile being
       estimated. If we're estimating the 90th quantile on even sample counts
       then the larger value is more likely to represent to true 90th quantile
       so we should skew in its favour. If we're estimating the median than we
       can't do better then 50/50.

       5) Sample count should weight more heavily then the quantile value
       as the larger sample count should be more representative.
     */

    bool pick = false;
    double delta = count > value->sample_count ?
        count - value->sample_count : value->sample_count - count;

    // If there's a large difference in sample count then prefer the larger sample.
    if (rng_gen_prob(rng_global(), delta / (count + value->sample_count)))
        pick = count >= value->sample_count;

    // Otherwise sample based on quantile probability
    else if (sample > value->sample)
        pick = rng_gen_prob(rng_global(), quantile->target_quantile);
    else
        pick = rng_gen_prob(rng_global(), 1.0 - quantile->target_quantile);

    if (pick) {
        value->sample = sample;
        value->sample_count = count;
    }
    value->count += count;

    return optics_ok;
}

static bool
lens_quantile_normalize(
        const struct optics_poll *poll, optics_normalize_cb_t cb, void *ctx)
{
    return cb(ctx, poll->ts, poll->key, poll->value.quantile.sample);
}
