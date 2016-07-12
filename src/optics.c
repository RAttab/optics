/* optics.c
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "optics_priv.h"
#include "utils/compiler.h"
#include "utils/errors.h"
#include "utils/type_pun.h"
#include "utils/htable.h"
#include "utils/lock.h"
#include "utils/rng.h"
#include "utils/shm.h"
#include "utils/time.h"
#include "utils/bits.h"
#include "utils/log.h"
#include "utils/socket.h"

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
static const size_t cache_line_len = 64UL;;


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
    atomic_off_t epoch_defers[2];

    atomic_off_t lens_head;

    char prefix[optics_name_max_len];
    char host[optics_name_max_len];

    struct alloc alloc;
};

struct optics
{
    struct region region;

    // Synchronizes:
    //   - optics.keys: read and write
    //   - optics.header->lens_head: write-only (reads are lock-free).
    //
    // Even though it's not strictly required, it's simpler to keep both of
    // these structures consistent with each-other.
    struct slock lock;

    struct optics_header *header;
    struct htable keys;
};

static bool set_default_host(struct optics *optics);


// -----------------------------------------------------------------------------
// open/close
// -----------------------------------------------------------------------------

struct optics * optics_create_at(const char *name, optics_ts_t now)
{
    struct optics *optics = calloc(1, sizeof(*optics));
    optics_assert_alloc(optics);

    if (!region_create(&optics->region, name, sizeof(*optics->header)))
        goto fail_region;

    optics->header = region_ptr_unsafe(&optics->region, 0, sizeof(*optics->header));
    if (!optics->header) goto fail_header;

    if (!optics_set_prefix(optics, name)) goto fail_prefix;
    if (!set_default_host(optics)) goto fail_host;

    alloc_init(&optics->header->alloc);
    optics->header->epoch_last_inc = now;

    return optics;

  fail_host:
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
    optics_assert_alloc(optics);

    if (!region_open(&optics->region, name)) goto fail_region;

    optics->header = region_ptr(&optics->region, 0, sizeof(*optics->header));
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
    optics_assert(slock_try_lock(&optics->lock),
            "closing optics with active thread");

    htable_reset(&optics->keys);
    region_close(&optics->region);
    free(optics);
}

bool optics_unlink(const char *name)
{
    return region_unlink(name);
}

static int optics_unlink_all_cb(void *ctx, const char *name)
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

const char *optics_get_host(struct optics *optics)
{
    return optics->header->host;
}

bool optics_set_host(struct optics *optics, const char *host)
{
    if (strnlen(host, optics_name_max_len) == optics_name_max_len) {
        optics_fail("host '%s' length is greater then max length '%d'",
                host, optics_name_max_len);
        return false;
    }

    strncpy(optics->header->host, host, optics_name_max_len);
    return true;
}

static bool set_default_host(struct optics *optics)
{
    char host[optics_name_max_len];
    if (!hostname(host, sizeof(host))) return false;

    return optics_set_host(optics, host);
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
    return alloc_alloc(&optics->header->alloc, &optics->region, len);
}

static void optics_free(struct optics *optics, optics_off_t off, size_t len)
{
    alloc_free(&optics->header->alloc, &optics->region, off, len);
}

static bool optics_defer_free(struct optics *optics, optics_off_t off, size_t len)
{

    struct optics_defer *pnode;
    optics_off_t node = optics_alloc(optics, sizeof(*pnode));
    if (!node) return false;

    pnode = optics_ptr(optics, node, sizeof(*pnode));
    if (!pnode) return false;

    pnode->off = off;
    pnode->len = len;

    optics_epoch_t epoch = optics_epoch(optics);
    atomic_off_t *head = &optics->header->epoch_defers[epoch];

    // Synchronizes with optics_free_defered to make sure that our node is fully
    // written befor it is read.
    optics_off_t old = atomic_load_explicit(head, memory_order_relaxed);
    do {
        pnode->next = old;
    } while (!atomic_compare_exchange_weak_explicit(
                    head, &old, node, memory_order_release, memory_order_relaxed));

    return true;
}

static void optics_free_defered(struct optics *optics, optics_epoch_t epoch)
{
    // Synchronizes with optics_defer_free to make sure that all nodes have been
    // fully written before we read them.
    atomic_off_t *head = &optics->header->epoch_defers[epoch];
    optics_off_t node = atomic_exchange_explicit(head, 0, memory_order_acquire);

    while (node) {
        struct optics_defer *pnode = optics_ptr(optics, node, sizeof(*pnode));
        if (!pnode) {
            optics_warn("leaked memory during defered free: %s", optics_errno.msg);
            break;
        }

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

    return atomic_fetch_add(&optics->header->epoch, 1) & 1;
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
    optics_assert(!slock_try_lock(&optics->lock), "pushing lens without lock held");

    atomic_off_t *head = &optics->header->lens_head;
    optics_off_t old_head = atomic_load_explicit(head, memory_order_relaxed);
    lens_set_next(optics, lens, old_head);

    // Synchronizes with optics_foreach_lens to ensure that the node is fully
    // written before it is accessed.
    atomic_store_explicit(head, lens_off(lens), memory_order_release);
}

static void optics_remove_lens(struct optics *optics, struct lens *lens)
{
    optics_assert(!slock_try_lock(&optics->lock), "removing lens without lock held");

    lens_kill(optics, lens);

    atomic_off_t *head = &optics->header->lens_head;
    optics_off_t old_head = atomic_load_explicit(head, memory_order_relaxed);
    if (old_head == lens_off(lens))
        atomic_store_explicit(head, lens_next(lens), memory_order_relaxed);
}

// Should be a lock-free traversal of the lenses so that the poller doens't
// block on any record operations.
enum optics_ret optics_foreach_lens(struct optics *optics, void *ctx, optics_foreach_t cb)
{

    // Synchronizes with optics_push_lens to ensure that all nodes are fully
    // written before we access them.
    atomic_off_t *head = &optics->header->lens_head;
    optics_off_t off = atomic_load_explicit(head, memory_order_acquire);

    while (off) {
        struct lens *lens = lens_ptr(optics, off);
        if (!lens) return -1;

        struct optics_lens ol = { .optics = optics, .lens = lens };
        enum optics_ret ret = cb(ctx, &ol);
        if (ret != optics_ok) return ret;

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
            optics_assert_alloc(lens);
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
    optics_assert_alloc(ol);
    ol->optics = optics;
    ol->lens = lens;
    return ol;

}

static struct optics_lens *
optics_lens_alloc_get(struct optics *optics, struct lens *lens)
{
    {
        slock_lock(&optics->lock);

        struct htable_ret ret = htable_put(&optics->keys, lens_name(lens), lens_off(lens));

        if (ret.ok) optics_push_lens(optics, lens);
        else lens = lens_ptr(optics, ret.value);

        slock_unlock(&optics->lock);
    }

    struct optics_lens *ol = calloc(1, sizeof(struct optics_lens));
    optics_assert_alloc(ol);
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
        if (ok) optics_remove_lens(l->optics, l->lens);

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

struct optics_lens * optics_counter_alloc_get(struct optics *optics, const char *name)
{
    struct lens *counter = lens_counter_alloc(optics, name);
    if (!counter) return NULL;

    struct optics_lens *lens = optics_lens_alloc_get(optics, counter);
    if (lens->lens != counter) lens_free(optics, counter);

    return lens;
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

struct optics_lens * optics_gauge_alloc_get(struct optics *optics, const char *name)
{
    struct lens *gauge = lens_gauge_alloc(optics, name);
    if (!gauge) return NULL;

    struct optics_lens *lens = optics_lens_alloc_get(optics, gauge);
    if (lens->lens != gauge) lens_free(optics, gauge);

    return lens;
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

struct optics_lens * optics_dist_alloc_get(struct optics *optics, const char *name)
{
    struct lens *dist = lens_dist_alloc(optics, name);
    if (!dist) return NULL;

    struct optics_lens *lens = optics_lens_alloc_get(optics, dist);
    if (lens->lens != dist) lens_free(optics, dist);

    return lens;
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


// -----------------------------------------------------------------------------
// value
// -----------------------------------------------------------------------------

bool optics_poll_normalize(
        const struct optics_poll *poll, optics_normalize_cb_t cb, void *ctx)
{
    switch (poll->type) {
    case optics_counter: return lens_counter_normalize(poll, cb, ctx);
    case optics_gauge: return lens_gauge_normalize(poll, cb, ctx);
    case optics_dist: return lens_dist_normalize(poll, cb, ctx);
    default:
        optics_fail("unknown lens type '%d'", poll->type);
        return false;
    }
}

