/* poller.c
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "poller.h"
#include "utils/key.h"
#include "utils/time.h"

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>


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


struct optics_poller *optics_poller_new()
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
    struct key *key = calloc(1, sizeof(struct key));
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


static size_t poller_list_optics(struct optics **list, size_t list_max)
{
    static const char shm_dir[] = "/dev/shm";
    static const char shm_prefix[] = "optics.";

    DIR *dir = opendir(shm_dir);
    if (!dir) {
        optics_fail_errno("unable to open '%s'", shm_dir);
        optics_abort();
    }

    size_t list_len = 0;

    // \todo: use re-entrant readdir_r
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_type != DT_REG) continue;
        if (memcmp(entry->d_name, shm_prefix, sizeof(shm_prefix)))
            continue;

        list[list_len] = optics_open(entry->d_name + sizeof(shm_prefix));
        if (!list[list_len]) {
            optics_warn("unable to open optics '%s': %s",
                    entry->d_name + sizeof(shm_prefix), optics_errno.msg);
            continue;
        }

        list_len++;
        if (list_len >= list_max) {
            optics_warn("reached optics polling capcity '%lu'", list_max);
            break;
        }
    }

    if (errno) {
        optics_fail_errno("unable to read dir '%s'", shm_dir);
        optics_abort();
    }

    closedir(dir);
    return list_len;
}

void poller_poll(struct optics_poller *poller)
{
    enum { poller_max_optics = 16 };

    struct optics *to_poll[poller_max_optics];
    size_t to_poll_len = poller_list_optics(to_poll, poller_max_optics);
    if (!to_poll_len) return;

    optics_epoch_t epochs[to_poll_len];
    for (size_t i = 0; i < to_poll_len; ++i)
        epochs[i] = optics_epoch_inc(to_poll[i]);

    time_t ts = time(NULL);

    // give a chance for stragglers to finish. We'd need full EBR to do this
    // properly but that would add overhead on the record side so we instead
    // just wait a bit and deal with stragglers if we run into them.
    optics_yield();

    for (size_t i = 0; i < to_poll_len; ++i)
        poller_poll_optics(poller, to_poll[i], epochs[i], ts);

    for (size_t i = 0; i < to_poll_len; ++i)
        optics_close(to_poll[i]);
}
