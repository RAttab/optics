/* optics.c
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "optics.h"
#include "utils/macros.h"
#include "utils/lock.h"
#include "utils/rng.h"

#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>


// -----------------------------------------------------------------------------
// impl
// -----------------------------------------------------------------------------

#include "region.c"
#include "lens.c"


// -----------------------------------------------------------------------------
// config
// -----------------------------------------------------------------------------

typedef uint64_t optics_off_t;
typedef atomic_uint64_t atomic_off_t;

typedef size_t optics_epoch_t; // 0 - 1 value.
typedef atomic_size_t atomic_optics_epoch_t;

static const size_t cache_line_len = 64UL;
static const size_t page_len = 4096UL;

static const optics_off_t epoch_off = 0;


// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct optics_packed optics_header
{
    atomic_size_t epoch;
    atomic_off_t head;

    char prefix[optics_name_max_len];
};

struct optics
{
    struct region region;
    struct optics_header *header;
};

struct optics_lens
{
    struct optics *optics;
    struct lens *lens;
};


// -----------------------------------------------------------------------------
// open/close
// -----------------------------------------------------------------------------

struct optics * optics_create(const char *name)
{
    struct optics *optics = calloc(1, sizeof(*optics));

    if (!region_create(&optics->region, name)) goto fail_region;

    optics->header = region_ptr(&optics->region, 0, sizeof(struct optics_header));
    if (!optics->header) goto fail_header;

    if (!optics_prefix(optics, name)) goto fail_prefix;

    return optics;

  fail_prefix:
  fail_header:
    region_close(&optics->region);

  fail_region:
    free(optics);
    return NULL;
}

struct optics * optics_open(const char *name)
{
    struct optics *optics = calloc(1, sizeof(*optics));

    if (!region_open(&optics->region, name)) goto fail_region;

    optics->header = region_ptr(&optics->region, 0, sizeof(struct optics_header));
    if (!optics->header) goto fail_header;

    return optics;

  fail_header:
    region_close(&optics->region);

  fail_region:
    free(optics);
    return NULL;
}


void optics_close(struct optics *optics)
{
    region_close(&optics->region);
    free(optics);
}

const char *optics_get_prefix(struct optics *optics)
{
    return optics->header->prefix;
}

void optics_set_prefix(struct optics *, const char *prefix)
{
    if (strnlen(prefix, optics_name_max_len) == optics_name_max_len) {
        optics_fail("prefix '%s' length is greater then max length '%lu'",
                optics_name_max_len);
        return false;
    }

    strncpy(optics->header->prefix, name, optics_name_max_len);
}


// -----------------------------------------------------------------------------
// epoch
// -----------------------------------------------------------------------------

// Memory order semantics are pretty weird here since the write (inc) op does
// not need to synchronize any data with the read op yet the read op should
// still prevent hoisting.

optics_epoch_t optics_epoch(struct optics *optics)
{
    return atomic_read_explicit(optics->header->epoch, memory_order_acquire) & 1;
}

optics_epoch_t optics_epoch_inc(struct optics *optics)
{
    return atomic_fetch_add_explicit(optics->header->epoch, 1, memory_order_release) & 1;
}


// -----------------------------------------------------------------------------
// list
// -----------------------------------------------------------------------------

static bool
optics_push_lens(struct optics *optics, struct lens *lens)
{
    optics_off_t old = atomic_load(&optics->header->head);

    do {
        if (!lens_set_next(lens, old)) return false;
    } while (!atomic_compare_exchange_weak_explicit(
                    &optics->header->head, &old, lens_off(lens),
                    memory_order_release, memory_order_relaxed));

    return true;
}

static bool
optics_remove_lens(struct optics *optics, struct lens *lens)
{
    lens_kill(lens);
    return true;
}

bool optics_foreach_lens(struct optics *optics, void *ctx, optics_foreach_t cb)
{
    optics_off_t off = atomic_load_explicit(&optics->header->head, memory_order_acquire);
    while (off) {
        struct lens *lens = lens_ptr(&optics->region, off);

        if (!lens_is_dead(lens)) {
            struct optics_lens ol = { .optics = optics, .lens = lens };
            if (!cb(ctx, &ol)) return false;
        }

        off = lens_next(lens);
    }

    return true;
}


// -----------------------------------------------------------------------------
// lens
// -----------------------------------------------------------------------------

struct get_cb_ctx
{
    const char *name;
    struct optics_lens *result;
};


static bool optics_lens_get_cb(void *ctx_, struct optics_lens *lens)
{
    struct get_cb_ctx *ctx = ctx_;

    if (strncmp(lens_name(lens->lens), ctx->name)) return true;

    *ctx->result = *lens;
    return false;
}

struct optics_lens * optics_lens_get(struct optics *optics, const char *name)
{
    struct optics_lens result;
    struct get_cb_ctx ctx = { .name = name, .result = &result };

    if (optics_foreach_lens(optics, &ctx, optics_lens_get_cb))
        return NULL;

    struct optics_lens *lens = calloc(1, sizeof(*lens));
    *lens = result;
    return lens;
}

void optics_lens_close(struct optics_lens *lens)
{
    free(lens);
}

bool optics_lens_free(struct optics_lens *l)
{
    if (!optics_remove_lens(l->optics, l->lens)) return false;
    optics_lens_close(l);
    return true;
}


enum lens_type optics_lens_type(struct optics_lens *l)
{
    return lens_type(l->lens);
}

const char * optics_lens_name(struct optics_lens *l)
{
    return lens_name(l->lens);
}


// -----------------------------------------------------------------------------
// counter
// -----------------------------------------------------------------------------

struct optics_lens * optics_counter_alloc(struct optics *optics, const char *name)
{
    struct lens *counter = lens_counter_alloc(&optics->region, name);
    if (!counter) return NULL;

    struct optics_lens *l = calloc(1, struct optics_lens);
    l->optics = optics;
    l->lens = counter;
    return l;
}

bool optics_counter_inc(struct optics_lens *lens, int64_t value)
{
    return lens_counter_inc(lens, optics_epoch(lens->optics), value);
}

enum optics_ret
optics_counter_read(struct optics_lens *lens, optics_epoch_t epoch, int64_t *value)
{
    return lens_counter_read(lens, epoch, value);
}


// -----------------------------------------------------------------------------
// gauge
// -----------------------------------------------------------------------------

struct optics_lens * optics_gauge_alloc(struct optics *optics, const char *name)
{
    struct lens *gauge = lens_gauge_alloc(&optics->region, name);
    if (!gauge) return NULL;

    struct optics_lens *l = calloc(1, struct optics_lens);
    l->optics = optics;
    l->lens = gauge;
    return l;
}

bool optics_gauge_set(struct optics_lens *lens, double value)
{
    return lens_gauge_set(lens, optics_epoch(lens->optics), value);
}

enum optics_ret
optics_gauge_read(struct optics_lens *lens, optics_epoch_t epoch, double *value)
{
    return lens_gauge_read(lens, epoch, value);
}


// -----------------------------------------------------------------------------
// dist
// -----------------------------------------------------------------------------

struct optics_lens * optics_dist_alloc(struct optics *optics, const char *name)
{
    struct lens *dist = lens_dist_alloc(&optics->region, name);
    if (!dist) return NULL;

    struct optics_lens *l = calloc(1, struct optics_lens);
    l->optics = optics;
    l->lens = dist;
    return l;
}

bool optics_dist_record(struct optics_lens *lens, double value)
{
    return lens_dist_record(lens, optics_epoch(lens->optics), value);
}

enum optics_ret
optics_dist_read(struct optics_lens *lens, optics_epoch_t epoch, struct optics_dist *value)
{
    return lens_dist_read(lens, epoch, value);
}
