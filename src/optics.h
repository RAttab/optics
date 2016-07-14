/* optics.h
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

#include <time.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>


// -----------------------------------------------------------------------------
// config
// -----------------------------------------------------------------------------

enum {
    optics_name_max_len = 256,
    optics_err_msg_cap = 1024,
    optics_err_backtrace_cap = 256,
};

typedef uint64_t optics_ts_t;


// -----------------------------------------------------------------------------
// error
// -----------------------------------------------------------------------------

struct optics_error
{
    bool warning;

    const char *file;
    int line;

    int errno_; // errno can be a macro hence the underscore.
    char msg[optics_err_msg_cap];

    void *backtrace[optics_err_backtrace_cap];
    int backtrace_len;
};

extern __thread struct optics_error optics_errno;

void optics_perror(struct optics_error *err);
size_t optics_strerror(struct optics_error *err, char *dest, size_t len);


// -----------------------------------------------------------------------------
// optics
// -----------------------------------------------------------------------------

struct optics;

struct optics * optics_open(const char *name);
struct optics * optics_create(const char *name);
struct optics * optics_create_at(const char *name, optics_ts_t now);
void optics_close(struct optics *);
bool optics_unlink(const char *name);
bool optics_unlink_all();

const char *optics_get_prefix(struct optics *);
bool optics_set_prefix(struct optics *, const char *prefix);

const char *optics_get_source(struct optics *);
bool optics_set_source(struct optics *, const char *source);


// -----------------------------------------------------------------------------
// lens
// -----------------------------------------------------------------------------

struct optics_lens;

enum optics_lens_type
{
    optics_counter,
    optics_gauge,
    optics_dist,
};

enum optics_ret
{
    optics_ok = 0,
    optics_err = -1,
    optics_busy = 1,
    optics_break = 2,
};

struct optics_lens * optics_lens_get(struct optics *, const char *name);
enum optics_lens_type optics_lens_type(struct optics_lens *);
const char * optics_lens_name(struct optics_lens *);
void optics_lens_close(struct optics_lens *);
bool optics_lens_free(struct optics_lens *);

struct optics_lens * optics_counter_alloc(struct optics *, const char *name);
struct optics_lens * optics_counter_alloc_get(struct optics *, const char *name);
bool optics_counter_inc(struct optics_lens *, int64_t value);

struct optics_lens * optics_gauge_alloc(struct optics *, const char *name);
struct optics_lens * optics_gauge_alloc_get(struct optics *, const char *name);
bool optics_gauge_set(struct optics_lens *, double value);

struct optics_dist
{
    size_t n;
    double p50;
    double p90;
    double p99;
    double max;
};

struct optics_lens * optics_dist_alloc(struct optics *, const char *name);
struct optics_lens * optics_dist_alloc_get(struct optics *, const char *name);
bool optics_dist_record(struct optics_lens *, double value);


// -----------------------------------------------------------------------------
// key
// -----------------------------------------------------------------------------

struct optics_key
{
    size_t len;
    char data[optics_name_max_len];
};

size_t optics_key_push(struct optics_key *key, const char *suffix);
void optics_key_pop(struct optics_key *key, size_t pos);


// -----------------------------------------------------------------------------
// timer
// -----------------------------------------------------------------------------

typedef struct timespec optics_timer_t;

static const double optics_sec = 1.0e-9;
static const double optics_msec = 1.0e-6;
static const double optics_usec = 1.0e-3;
static const double optics_nsec = 1.0;

inline void optics_timer_start(optics_timer_t *t0)
{
    if (clock_gettime(CLOCK_MONOTONIC, t0)) abort();
}

inline double optics_timer_elapsed(optics_timer_t *t0, double scale)
{
    struct timespec t1;
    if (clock_gettime(CLOCK_MONOTONIC, &t1)) abort();

    uint64_t secs = t1.tv_sec - t0->tv_sec;
    uint64_t nanos = t1.tv_nsec - t0->tv_nsec;
    return (secs * 1UL * 1000 * 1000 * 1000 + nanos) * scale;
}


// -----------------------------------------------------------------------------
// poll
// -----------------------------------------------------------------------------

union optics_poll_value
{
        int64_t counter;
        double gauge;
        struct optics_dist dist;
};

struct optics_poll
{
    const char *host;
    const char *prefix;
    const char *source;
    struct optics_key *key;

    enum optics_lens_type type;
    union optics_poll_value value;

    optics_ts_t ts;
    optics_ts_t elapsed;
};

typedef bool (*optics_normalize_cb_t) (
        void *ctx, optics_ts_t ts, const char *key, double value);

bool optics_poll_normalize(
        const struct optics_poll *poll, optics_normalize_cb_t cb, void *ctx);


// -----------------------------------------------------------------------------
// poller
// -----------------------------------------------------------------------------

struct optics_poller;

struct optics_poller * optics_poller_alloc();
void optics_poller_free(struct optics_poller *);

enum optics_poll_type
{
    optics_poll_begin,
    optics_poll_metric,
    optics_poll_done,
};

typedef void (*optics_backend_free_t) (void *ctx);
typedef void (*optics_backend_cb_t) (
        void *ctx,
        enum optics_poll_type type,
        const struct optics_poll *poll);

bool optics_poller_backend(
        struct optics_poller *,
        void *ctx,
        optics_backend_cb_t cb,
        optics_backend_free_t free);

bool optics_poller_set_host(struct optics_poller *poller, const char *host);
const char * optics_poller_get_host(struct optics_poller *poller);

bool optics_poller_poll(struct optics_poller *poller);
bool optics_poller_poll_at(struct optics_poller *poller, optics_ts_t ts);


// -----------------------------------------------------------------------------
// thread
// -----------------------------------------------------------------------------

struct optics_thread;

struct optics_thread * optics_thread_start(
        struct optics_poller *poller, optics_ts_t freq);
bool optics_thread_stop(struct optics_thread *thread);


// -----------------------------------------------------------------------------
// backends
// -----------------------------------------------------------------------------

struct crest;

void optics_dump_stdout(struct optics_poller *);
void optics_dump_carbon(struct optics_poller *, const char *host, const char *port);
void optics_dump_prometheus(struct optics_poller *poller, struct crest *crest);
void optics_dump_rest(struct optics_poller *poller, struct crest *crest);
