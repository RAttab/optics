/* backend_stdout.c
   Rémi Attab (remi.attab@gmail.com), 29 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "poller.h"

#include <stdio.h>


// -----------------------------------------------------------------------------
// callbacks
// -----------------------------------------------------------------------------

static void stdout_dump(void *ctx, uint64_t ts, const char *key, double value)
{
    (void) ctx;

    printf("[%lu] %s: %g\n", ts, key, value);
}


// -----------------------------------------------------------------------------
// register
// -----------------------------------------------------------------------------

void optics_dump_stdout(struct optics_poller *poller)
{
    optics_poller_backend(poller, NULL, stdout_dump, NULL);
}
