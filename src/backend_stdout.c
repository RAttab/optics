/* backend_stdout.c
   Rémi Attab (remi.attab@gmail.com), 29 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "optics.h"

#include <stdio.h>


// -----------------------------------------------------------------------------
// callbacks
// -----------------------------------------------------------------------------

static bool stdout_dump_normalized(
        void *ctx, optics_ts_t ts, const char *key, double value)
{
    const struct optics_poll *poll = ctx;

    printf("[%lu] %s.%s{host='%s'} = %g\n",
            ts, poll->prefix, key, poll->host, value);

    return true;
}

static void stdout_dump(
        void *ctx, enum optics_poll_type type, const struct optics_poll *poll)
{
    (void) ctx;
    if (type != optics_poll_metric) return;

    (void) optics_poll_normalize(poll, stdout_dump_normalized, (void *) poll);
}


// -----------------------------------------------------------------------------
// register
// -----------------------------------------------------------------------------

void optics_dump_stdout(struct optics_poller *poller)
{
    optics_poller_backend(poller, NULL, stdout_dump, NULL);
}
