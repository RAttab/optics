/* poller.h
   Rémi Attab (remi.attab@gmail.com), 26 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

#include "optics.h"

#include <time.h>


// -----------------------------------------------------------------------------
// poller
// -----------------------------------------------------------------------------

struct optics_poller;

struct optics_poller *optics_poller_alloc();
void optics_poller_free(struct optics_poller *);

typedef void (*optics_backend_cb_t) (void *ctx, uint64_t ts, const char *key, double value);
typedef void (*optics_backend_free_t) (void *ctx);
bool optics_poller_backend(
        struct optics_poller *, void *ctx, optics_backend_cb_t cb, optics_backend_free_t free);

bool optics_poller_poll(struct optics_poller *poller);
bool optics_poller_poll_at(struct optics_poller *poller, optics_ts_t ts);


// -----------------------------------------------------------------------------
// backends
// -----------------------------------------------------------------------------

void optics_dump_stdout(struct optics_poller *);
void optics_dump_carbon(struct optics_poller *, const char *host, const char *port);
