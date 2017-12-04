/* lens_histo.c
   Rémi Attab (remi.attab@gmail.com), 07 Jul 2017
   FreeBSD-style copyright and disclaimer apply
*/


// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct optics_packed lens_histo_epoch
{
    atomic_size_t below, above;
    atomic_size_t counts[optics_histo_buckets_max];
};

struct optics_packed lens_histo
{
    struct lens_histo_epoch epochs[2];

    double buckets[optics_histo_buckets_max + 1];
    size_t buckets_len;
};


// -----------------------------------------------------------------------------
// impl
// -----------------------------------------------------------------------------

static struct lens *
lens_histo_alloc(
        struct optics *optics, const char *name,
        const double *buckets, size_t buckets_len)
{
    if (buckets_len < 2) {
        optics_fail("invalid histo bucket length '%lu' < '2'", buckets_len);
        goto fail_buckets;
    }

    if (buckets_len > optics_histo_buckets_max + 1) {
        optics_fail("invalid histo bucket length '%lu' > '%d'",
                buckets_len, optics_histo_buckets_max + 1);
        goto fail_buckets;
    }

    for (size_t i = 0; i < buckets_len - 1; ++i) {
        if (buckets[i] >= buckets[i + 1]) {
            optics_fail("invalid histo buckets '%lu:%lf' >= '%lu:%lf'",
                    i, buckets[i], i + 1, buckets[i + 1]);
            goto fail_buckets;
        }
    }

    struct lens *lens = lens_alloc(optics, optics_histo, sizeof(struct lens_histo), name);
    if (!lens) goto fail_alloc;

    struct lens_histo *histo = lens_sub_ptr(lens, optics_histo);
    if (!histo) goto fail_sub;

    histo->buckets_len = buckets_len;
    memcpy(histo->buckets, buckets, buckets_len * sizeof(histo->buckets[0]));

    return lens;

  fail_sub:
    lens_free(optics, lens);
  fail_alloc:
  fail_buckets:
    return NULL;
}

static bool
lens_histo_inc(struct optics_lens *lens, optics_epoch_t epoch, double value)
{
    (void) epoch;

    struct lens_histo *histo = lens_sub_ptr(lens->lens, optics_histo);
    if (!histo) return false;

    atomic_size_t *bucket = NULL;

    if (value < histo->buckets[0])
        bucket = &histo->epochs[epoch].below;

    else if (value >= histo->buckets[histo->buckets_len - 1])
        bucket = &histo->epochs[epoch].above;

    else {
        for (size_t i = 1; i < histo->buckets_len; ++i) {
            if (value < histo->buckets[i]) {
                bucket = &histo->epochs[epoch].counts[i - 1];
                break;
            }
        }
    }

    optics_assert(!!bucket, "value outside of all bucket ranges");

    atomic_fetch_add_explicit(bucket, 1, memory_order_relaxed);
    return true;
}

static enum optics_ret
lens_histo_read(struct optics_lens *lens, optics_epoch_t epoch, struct optics_histo *value)
{
    (void) epoch;

    struct lens_histo *histo = lens_sub_ptr(lens->lens, optics_histo);
    if (!histo) return optics_err;

    value->buckets_len = histo->buckets_len;
    memcpy(value->buckets, histo->buckets, histo->buckets_len * sizeof(histo->buckets[0]));

    struct lens_histo_epoch *counters = &histo->epochs[epoch];
    value->below = atomic_exchange_explicit(&counters->below, 0, memory_order_relaxed);
    value->above = atomic_exchange_explicit(&counters->above, 0, memory_order_relaxed);
    for (size_t i = 0; i < histo->buckets_len - 1; ++i) {
        value->counts[i] +=
            atomic_exchange_explicit(&counters->counts[i], 0, memory_order_relaxed);
    }

    return optics_ok;
}

static bool
lens_histo_normalize(
        const struct optics_poll *poll, optics_normalize_cb_t cb, void *ctx)
{
    bool ret = false;
    size_t old;
    const struct optics_histo *histo = &poll->value.histo;

    old = optics_key_push(poll->key, "below");
    ret = cb(ctx, poll->ts, poll->key->data, lens_rescale(poll, histo->below));
    optics_key_pop(poll->key, old);
    if (!ret) return false;

    old = optics_key_push(poll->key, "above");
    ret = cb(ctx, poll->ts, poll->key->data, lens_rescale(poll, histo->above));
    optics_key_pop(poll->key, old);
    if (!ret) return false;

    for (size_t i = 0; i < histo->buckets_len - 1; ++i) {
        old = optics_key_pushf(
                poll->key, "bucket_%.3g_%.3g", histo->buckets[i], histo->buckets[i + 1]);
        ret = cb(ctx, poll->ts, poll->key->data, lens_rescale(poll, histo->counts[i]));
        optics_key_pop(poll->key, old);
        if (!ret) return false;
    }

    return true;
}
