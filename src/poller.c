/* poller.c
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "optics_priv.h"
#include "utils/errors.h"
#include "utils/time.h"
#include "utils/shm.h"
#include "utils/log.h"

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


struct optics_poller * optics_poller_alloc()
{
    struct optics_poller *poller = calloc(1, sizeof(*poller));
    optics_assert_alloc(poller);
    return poller;
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
        struct optics_poller *poller,
        enum optics_poll_type type,
        const struct optics_poll *poll)
{
    for (size_t i = 0; i < poller->backends_len; ++i)
        poller->backends[i].cb(poller->backends[i].ctx, type, poll);
}


// -----------------------------------------------------------------------------
// implementation
// -----------------------------------------------------------------------------

#include "poller_thread.c"
#include "poller_poll.c"
