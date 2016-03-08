/* poller.c
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "poller.h"
#include "utils/key.h"
#include "utils/time.h"
#include "utils/shm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// -----------------------------------------------------------------------------
// config
// -----------------------------------------------------------------------------

enum { poller_max_backends = 8 };


// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct backend
{
    void *ctx;
    optics_backend_cb_t cb;
    optics_backend_free_t free;
};


struct optics_poller
{
    size_t backends_len;
    struct backend backends[poller_max_backends];
};


// -----------------------------------------------------------------------------
// open/close
// -----------------------------------------------------------------------------


struct optics_poller *optics_poller_alloc()
{
    return calloc(1, sizeof(struct optics_poller));
}

void optics_poller_free(struct optics_poller *poller)
{
    for (size_t i = 0; i < poller->backends_len; ++i) {
        struct backend *backend = &poller->backends[i];
        if (backend->free) backend->free(backend->ctx);
    }

    free(poller);
}


// -----------------------------------------------------------------------------
// backends
// -----------------------------------------------------------------------------

bool optics_poller_backend(
        struct optics_poller *poller,
        void *ctx,
        optics_backend_cb_t cb,
        optics_backend_free_t free)
{
    if (poller->backends_len >= poller_max_backends) {
        optics_fail("reached poller backend capacity '%d'", poller_max_backends);
        return false;
    }

    poller->backends[poller->backends_len] = (struct backend) { ctx, cb, free };
    poller->backends_len++;

    return true;
}

static void poller_backend_record(
        struct optics_poller *poller, time_t ts, const char *key, double value)
{
    for (size_t i = 0; i < poller->backends_len; ++i)
        poller->backends[i].cb(poller->backends[i].ctx, ts, key, value);
}


// -----------------------------------------------------------------------------
// poll
// -----------------------------------------------------------------------------

struct poller_poll_ctx
{
    struct optics_poller *poller;
    optics_epoch_t epoch;
    struct key *key;
    time_t ts;
};


#include "poller_lens.c"


static bool poller_poll_lens(void *ctx_, struct optics_lens *lens)
{
    struct poller_poll_ctx *ctx = ctx_;

    size_t old_key = key_push(ctx->key, optics_lens_name(lens));

    switch (optics_lens_type(lens)) {
    case optics_counter: poller_poll_counter(ctx, lens); break;
    case optics_gauge:   poller_poll_gauge(ctx, lens); break;
    case optics_dist:    poller_poll_dist(ctx, lens); break;
    default:
        optics_fail("unknown poller type '%d'", optics_lens_type(lens));
        break;
    }

    key_pop(ctx->key, old_key);
    return true;
}


static void poller_poll_optics(
        struct optics_poller *poller,
        struct optics *optics,
        optics_epoch_t epoch,
        time_t ts)
{
    struct key *key = calloc(1, sizeof(*key));
    key_push(key, optics_get_prefix(optics));

    struct poller_poll_ctx ctx = {
        .poller = poller,
        .ts = ts,
        .key = key,
        .epoch = epoch,
    };

    optics_foreach_lens(optics, &ctx, poller_poll_lens);

    free(key);
}

enum { poller_max_optics = 128 };
struct poller_list
{
    size_t len;
    struct optics *optics[poller_max_optics];
    size_t epochs[poller_max_optics];
};

static int poller_shm_cb(void *ctx, const char *name)
{
    struct poller_list *list = ctx;

    list->optics[list->len] = optics_open(name);
    if (!list->optics[list->len]) {
        optics_warn("unable to open optics '%s': %s", name, optics_errno.msg);
        return 1;
    }

    list->len++;
    if (list->len >= poller_max_optics) {
        optics_warn("reached optics polling capcity '%d'", poller_max_optics);
        return 0;
    }

    return 1;
}

bool optics_poller_poll(struct optics_poller *poller)
{
    return optics_poller_poll_at(poller, time(NULL));
}

bool optics_poller_poll_at(struct optics_poller *poller, time_t ts)
{
    struct poller_list to_poll = {0};
    if (shm_foreach(&to_poll, poller_shm_cb) < 0) return false;
    if (!to_poll.len) return true;

    for (size_t i = 0; i < to_poll.len; ++i)
        to_poll.epochs[i] = optics_epoch_inc(to_poll.optics[i]);

    // give a chance for stragglers to finish. We'd need full EBR to do this
    // properly but that would add overhead on the record side so we instead
    // just wait a bit and deal with stragglers if we run into them.
    optics_yield();

    for (size_t i = 0; i < to_poll.len; ++i)
        poller_poll_optics(poller, to_poll.optics[i], to_poll.epochs[i], ts);

    for (size_t i = 0; i < to_poll.len; ++i)
        optics_close(to_poll.optics[i]);

    return true;
}
