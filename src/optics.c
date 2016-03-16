/* optics.c
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "optics.h"
#include "utils/compiler.h"
#include "utils/type_pun.h"
#include "utils/htable.h"
#include "utils/lock.h"
#include "utils/rng.h"
#include "utils/shm.h"
#include "utils/time.h"
#include "utils/bits.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>


// -----------------------------------------------------------------------------
// config
// -----------------------------------------------------------------------------

typedef uint64_t optics_off_t;
typedef atomic_uint_fast64_t atomic_off_t;

static_assert(
        sizeof(atomic_off_t) == sizeof(uint64_t),
        "if this fails then sucks to be you");

typedef size_t optics_epoch_t; // 0 - 1 value.
typedef atomic_size_t atomic_optics_epoch_t;

static const size_t page_len = 4096UL;


// -----------------------------------------------------------------------------
// impl
// -----------------------------------------------------------------------------

struct optics_lens
{
    struct optics *optics;
    struct lens *lens;
};

static void * optics_ptr(struct optics *optics, optics_off_t off, size_t len);
static optics_off_t optics_alloc(struct optics *optics, size_t len);
static void optics_free(struct optics *optics, optics_off_t off, size_t len);
static bool optics_defer_free(struct optics *optics, optics_off_t off, size_t len);

#include "region.c"
#include "alloc.c"
#include "lens.c"


// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct optics_packed optics_defer
{
    optics_off_t off;
    size_t len;

    optics_off_t next;
};

struct optics_packed optics_header
{
    atomic_size_t epoch;
    optics_ts_t epoch_last_inc;
    optics_off_t epoch_defers[2];

    atomic_off_t lens_head;

    char prefix[optics_name_max_len];

    struct alloc alloc;
};

struct optics
{
    struct region region;
    struct optics_header *header;

    struct slock lock;
    struct htable keys;
};


// -----------------------------------------------------------------------------
// open/close
// -----------------------------------------------------------------------------

struct optics * optics_create_at(const char *name, optics_ts_t now)
{
    struct optics *optics = calloc(1, sizeof(*optics));

    if (!region_create(&optics->region, name)) goto fail_region;

    optics->header = region_ptr(&optics->region, 0, sizeof(struct optics_header));
    if (!optics->header) goto fail_header;

    if (!optics_set_prefix(optics, name)) goto fail_prefix;
    alloc_init(&optics->header->alloc, sizeof(struct optics_header));
    optics->header->epoch_last_inc = now;

    return optics;

  fail_prefix:
  fail_header:
    region_close(&optics->region);

  fail_region:
    free(optics);
    return NULL;
}

struct optics * optics_create(const char *name)
{
    return optics_create_at(name, clock_wall());
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

bool optics_unlink(const char *name)
{
    return region_unlink(name);
}

int optics_unlink_all_cb(void *ctx, const char *name)
{
    (void) ctx;
    optics_unlink(name);
    return 1;
}

bool optics_unlink_all()
{
    return shm_foreach(NULL, optics_unlink_all_cb) >= 0;
}


const char *optics_get_prefix(struct optics *optics)
{
    return optics->header->prefix;
}

bool optics_set_prefix(struct optics *optics, const char *prefix)
{
    if (strnlen(prefix, optics_name_max_len) == optics_name_max_len) {
        optics_fail("prefix '%s' length is greater then max length '%d'",
                prefix, optics_name_max_len);
        return false;
    }

    strncpy(optics->header->prefix, prefix, optics_name_max_len);
    return true;
}


// -----------------------------------------------------------------------------
// alloc
// -----------------------------------------------------------------------------

static void * optics_ptr(struct optics *optics, optics_off_t off, size_t len)
{
    return region_ptr(&optics->region, off, len);
}

static optics_off_t optics_alloc(struct optics *optics, size_t len)
{
    optics_off_t off;

    {
        slock_lock(&optics->lock);

        off = alloc_alloc(&optics->header->alloc, &optics->region, len);

        slock_unlock(&optics->lock);
    }

    return off;
}

static void optics_free(struct optics *optics, optics_off_t off, size_t len)
{
    slock_lock(&optics->lock);

    alloc_free(&optics->header->alloc, &optics->region, off, len);

    slock_unlock(&optics->lock);
}

static bool optics_defer_free(struct optics *optics, optics_off_t off, size_t len)
{

    struct optics_defer *pnode;
    optics_off_t node = optics_alloc(optics, sizeof(*pnode));
    if (!node) return false;

    pnode = optics_ptr(optics, node, sizeof(*pnode));
    pnode->off = off;
    pnode->len = len;

    {
        slock_lock(&optics->lock);

        optics_epoch_t epoch = optics_epoch(optics);
        pnode->next = optics->header->epoch_defers[epoch];
        optics->header->epoch_defers[epoch] = node;

        slock_unlock(&optics->lock);
    }

    return true;
}

static void optics_free_defered(struct optics *optics, optics_epoch_t epoch)
{
    optics_off_t node;
    {
        slock_lock(&optics->lock);

        node = optics->header->epoch_defers[epoch];
        optics->header->epoch_defers[epoch] = 0;

        slock_unlock(&optics->lock);
    }

    while (node) {
        struct optics_defer *pnode = optics_ptr(optics, node, sizeof(*pnode));
        optics_free(optics, pnode->off, pnode->len);

        optics_off_t next = pnode->next;
        optics_free(optics, node, sizeof(*pnode));
        node = next;
    }
}


// -----------------------------------------------------------------------------
// epoch
// -----------------------------------------------------------------------------

// Memory order semantics are pretty weird here since the write (inc) op does
// not need to synchronize any data with the read op yet the read op should
// still prevent hoisting.

optics_epoch_t optics_epoch(struct optics *optics)
{
    return atomic_load_explicit(&optics->header->epoch, memory_order_acquire) & 1;
}

optics_epoch_t optics_epoch_inc(struct optics *optics)
{
    optics_free_defered(optics, optics_epoch(optics) ^ 1);
    return atomic_fetch_add_explicit(&optics->header->epoch, 1, memory_order_release) & 1;
}

optics_epoch_t optics_epoch_inc_at(
        struct optics *optics, optics_ts_t now, optics_ts_t *last_inc)
{
    *last_inc = optics->header->epoch_last_inc;
    optics->header->epoch_last_inc = now;

    return optics_epoch_inc(optics);
}


// -----------------------------------------------------------------------------
// list
// -----------------------------------------------------------------------------

static void optics_push_lens(struct optics *optics, struct lens *lens)
{
    optics_off_t head = atomic_load_explicit(
            &optics->header->lens_head, memory_order_relaxed);

    lens_set_next(optics, lens, head);
    atomic_store_explicit(
            &optics->header->lens_head, lens_off(lens), memory_order_release);
}

enum optics_ret optics_foreach_lens(struct optics *optics, void *ctx, optics_foreach_t cb)
{
    optics_off_t off = atomic_load_explicit(
            &optics->header->lens_head, memory_order_acquire);

    while (off) {

        struct lens *lens = lens_ptr(optics, off);
        if (!lens) return -1;

        if (!lens_is_dead(lens)) {
            struct optics_lens ol = { .optics = optics, .lens = lens };

            enum optics_ret ret = cb(ctx, &ol);
            if (ret != optics_ok) return ret;
        }

        off = lens_next(lens);
    }

    return optics_ok;
}


// -----------------------------------------------------------------------------
// lens
// -----------------------------------------------------------------------------

struct optics_lens * optics_lens_get(struct optics *optics, const char *name)
{
    struct optics_lens *lens = NULL;

    {
        slock_lock(&optics->lock);

        struct htable_ret ret = htable_get(&optics->keys, name);
        if (ret.ok) {
            lens = calloc(1, sizeof(*lens));
            lens->optics = optics;
            lens->lens = lens_ptr(optics, ret.value);
        }

        slock_unlock(&optics->lock);
    }

    return lens;
}

static struct optics_lens *
optics_lens_alloc(struct optics *optics, struct lens *lens)
{
    bool ok = false;
    {
        slock_lock(&optics->lock);

        ok = htable_put(&optics->keys, lens_name(lens), lens_off(lens)).ok;
        if (ok) optics_push_lens(optics, lens);

        slock_unlock(&optics->lock);
    }

    if (!ok) {
        optics_fail("lens '%s' already exists", lens_name(lens));
        return NULL;
    }

    struct optics_lens *ol = calloc(1, sizeof(struct optics_lens));
    ol->optics = optics;
    ol->lens = lens;
    return ol;

}

void optics_lens_close(struct optics_lens *lens)
{
    free(lens);
}

bool optics_lens_free(struct optics_lens *l)
{
    bool ok;
    {
        slock_lock(&l->optics->lock);

        ok = htable_del(&l->optics->keys, lens_name(l->lens)).ok;
        lens_kill(l->optics, l->lens);

        slock_unlock(&l->optics->lock);
    }

    if (!ok) return false;

    if (!lens_defer_free(l->optics, l->lens)) return false;
    optics_lens_close(l);

    return true;
}


enum optics_lens_type optics_lens_type(struct optics_lens *l)
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
    struct lens *counter = lens_counter_alloc(optics, name);
    if (!counter) return NULL;

    struct optics_lens *lens = optics_lens_alloc(optics, counter);
    if (lens) return lens;

    lens_free(optics, counter);
    return NULL;
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
    struct lens *gauge = lens_gauge_alloc(optics, name);
    if (!gauge) return NULL;

    struct optics_lens *lens = optics_lens_alloc(optics, gauge);
    if (lens) return lens;

    lens_free(optics, gauge);
    return NULL;
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
    struct lens *dist = lens_dist_alloc(optics, name);
    if (!dist) return NULL;

    struct optics_lens *lens = optics_lens_alloc(optics, dist);
    if (lens) return lens;

    lens_free(optics, dist);
    return NULL;
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
