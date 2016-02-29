/* backend_stdout.c
   Rémi Attab (remi.attab@gmail.com), 29 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/


// -----------------------------------------------------------------------------
// callbacks
// -----------------------------------------------------------------------------

static void dump(void *ctx, double ts, const char *key, double value)
{
    printf("[%lu] %s: %g\n", ts, key, value);
}


// -----------------------------------------------------------------------------
// register
// -----------------------------------------------------------------------------

void optics_dump_stdout(struct optics_poller *poller)
{
    optics_poller_backend(poller, NULL, &dump, NULL);
}
