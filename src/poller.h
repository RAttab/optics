/* poller.h
   Rémi Attab (remi.attab@gmail.com), 26 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

#include "optics.h"


// -----------------------------------------------------------------------------
// poller
// -----------------------------------------------------------------------------

struct optics_poller;

struct optics_poller *optics_poller_new();
void optics_poller_free(struct optics_poller *);

typedef bool (*optics_backend_cb_t) (void *ctx, time_t ts, const char *key, double value);
typedef void (*optics_backend_free_t) (void *ctx);
void optics_poller_backend(
        struct optics_poller *, void *ctx, optics_backend_cb_t cb, optics_backend_free_t free);


// -----------------------------------------------------------------------------
// backends
// -----------------------------------------------------------------------------

bool optics_dump_stdout(struct optics_poller *);
bool optics_dump_carbon(struct optics_poller *, const char *host, int port);
