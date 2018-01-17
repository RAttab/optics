/* lens_streaming.c
   Marina C., 19 Feb 2018
   FreeBSD-style copyright and disclaimer apply
*/

// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct optics_packed lens_streaming
{
     double quantile;
     double estimate;
     double adjustment_value;
};

// -----------------------------------------------------------------------------
// impl
// -----------------------------------------------------------------------------

static struct lens *
lens_streaming_alloc(struct optics *optics, const char *name, double quantile, double estimate, double adjustment_value)
{
    struct lens *lens = lens_alloc(optics, optics_streaming, sizeof(struct lens_streaming), name); //TO Do: optics_streaming
    if (!lens) goto fail_alloc;

    struct lens_streaming *streaming = lens_sub_ptr(lens, optics_streaming);
    if (!streaming) goto fail_sub;

    streaming->quantile = quantile;
    streaming->estimate = estimate;
    streaming->adjustment_value = adjustment_value;
    
    return lens;
  fail_sub:
    lens_free(optics, lens);
  fail_alloc:
    return NULL;
}

static bool
lens_streaming_update(struct optics_lens *lens, optics_epoch_t epoch, double value)
{
    (void) epoch;

    struct lens_streaming *streaming = lens_sub_ptr(lens->lens, optics_streaming);
    if (!streaming) return false;
    
    double existing_estimate  = streaming->estimate;
 
    bool smaller_than_quantile = rng_gen_prob(rng_global(), streaming->quantile);
    
    if (value < existing_estimate && !smaller_than_quantile){
        streaming->estimate -= streaming->adjustment_value;
    }		
    else if (smaller_than_quantile){
        streaming->estimate += streaming->adjustment_value;
    }

    return true;
}

static enum optics_ret
lens_streaming_read(struct optics_lens *lens, optics_epoch_t epoch, double *value)
{
    struct lens_streaming *streaming = lens_sub_ptr(lens->lens, optics_streaming);
    if (!streaming) return optics_err;

    *value = streaming->estimate;
    return optics_ok;
}
	
