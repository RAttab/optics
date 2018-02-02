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
     double estimate;
     double adjustment_value;
};

// -----------------------------------------------------------------------------
// impl
// -----------------------------------------------------------------------------

static struct lens *
lens_quantile_alloc(struct optics *optics, const char *name, double target_quantile, double estimate, double adjustment_value)
{
    struct lens *lens = lens_alloc(optics, optics_quantile, sizeof(struct lens_quantile), name); //TO Do: optics_quantile
    if (!lens) goto fail_alloc;

    struct lens_quantile *quantile = lens_sub_ptr(lens, optics_quantile);
    if (!quantile) goto fail_sub;

    quantile->target_quantile = target_quantile;
    quantile->estimate = estimate;
    quantile->adjustment_value = adjustment_value;
    
    return lens;
  fail_sub:
    lens_free(optics, lens);
  fail_alloc:
    return NULL;
}

static bool
lens_quantile_update(struct optics_lens *lens, optics_epoch_t epoch, double value)
{
    (void) epoch;

    struct lens_quantile *quantile = lens_sub_ptr(lens->lens, optics_quantile);
    if (!quantile) return false;
    
    double existing_estimate  = quantile->estimate;
 
    bool smaller_than_quantile = rng_gen_prob(rng_global(), quantile->target_quantile);
    
    if (value < existing_estimate){
	if (!smaller_than_quantile){
            quantile->estimate -= quantile->adjustment_value;
        }
    }		
    else if (smaller_than_quantile){
        quantile->estimate += quantile->adjustment_value;
    }
		 
    return true;
}

static enum optics_ret
lens_quantile_read(struct optics_lens *lens, optics_epoch_t epoch, double *value)
{
    (void) epoch;
    struct lens_quantile *quantile = lens_sub_ptr(lens->lens, optics_quantile);
    if (!quantile) return optics_err;

    *value = quantile->estimate;
   
    return optics_ok;
}

static bool
lens_quantile_normalize(
        const struct optics_poll *poll, optics_normalize_cb_t cb, void *ctx)
{
    return cb(ctx, poll->ts, poll->key->data, poll->value.quantile);
}

