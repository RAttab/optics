/* poller_thread.c
   Rémi Attab (remi.attab@gmail.com), 15 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "pthread.h"


// -----------------------------------------------------------------------------
// thread
// -----------------------------------------------------------------------------

struct optics_thread
{
    struct optics_poller *poller;
    optics_ts_t freq;

    pthread_t handle;
};


// -----------------------------------------------------------------------------
// poller
// -----------------------------------------------------------------------------

void * thread_fn(void *ctx)
{
    struct optics_thread *thread = ctx;

    while (true) {
        optics_poller_poll(thread->poller);

        // pthread cancellation point.
        nsleep(thread->freq * 1000 * 1000 * 1000);
    }
}


// -----------------------------------------------------------------------------
// start / kill
// -----------------------------------------------------------------------------


struct optics_thread * optics_thread_start(struct optics_poller *poller, optics_ts_t freq)
{
    if (!freq) {
        optics_fail("invalid nil thread freq");
        return NULL;
    }

    struct optics_thread *thread = calloc(1, sizeof(*thread));
    thread->poller = poller;
    thread->freq = freq;

    int err = pthread_create(&thread->handle, NULL, thread_fn, thread);
    if (err) {
        optics_fail_ierrno(err, "unable to create optics thread");
        goto fail_thread;
    }

    return thread;

  fail_thread:
    free(thread);

    return NULL;
}

bool optics_thread_stop(struct optics_thread *thread)
{
    int err = pthread_cancel(thread->handle);
    if (err) {
        optics_fail_ierrno(err, "unable to cancel optics thread");
        return false;
    }

    err = pthread_join(thread->handle, NULL);
    if (err) {
        optics_fail_ierrno(err, "unable to join optics thread");
        return false;
    }

    free(thread);
    return true;
}
