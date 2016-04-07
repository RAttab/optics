/* bench.c
   Rémi Attab (remi.attab@gmail.com), 23 Jan 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "bench.h"
#include "utils/lock.h"
#include "utils/time.h"
#include "utils/thread.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------


static double scale_elapsed(double t, char *s)
{
    static const char scale[] = "smunp";

    size_t i = 0;
    for (i = 0; i < sizeof(scale); ++i) {
        if (t >= 1.0) break;
        t *= 1000.0;
    }

    *s = scale[i];
    return t;
}



// -----------------------------------------------------------------------------
// optics_bench
// -----------------------------------------------------------------------------

struct optics_bench
{
    void **setup_data;
    struct sbarrier *setup_barrier;

    bool started;
    struct timespec start;
    struct sbarrier *start_barrier;
    struct sbarrier *stop_barrier;

    bool stopped;
    struct timespec stop;
};

void *optics_bench_setup(struct optics_bench *bench, void *data)
{
    if (data) *bench->setup_data = data;
    if (bench->setup_barrier) sbarrier_wait(bench->setup_barrier);
    return *bench->setup_data;
}

void optics_bench_start(struct optics_bench *bench)
{
    optics_assert(!bench->started, "bench started twice");
    bench->started = true;

    if (bench->start_barrier) sbarrier_wait(bench->start_barrier);

    clock_monotonic(&bench->start);
}

void optics_bench_stop(struct optics_bench *bench)
{
    optics_no_opt_clobber(); // make sure everything is done before we stop.

    clock_monotonic(&bench->stop);
    bench->stopped = true;

    if (bench->stop_barrier) sbarrier_wait(bench->stop_barrier);
}


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------


static double  sec() { return 1; }
static double msec() { return  sec() / 1000; }
static double usec() { return msec() / 1000; }
static double nsec() { return usec() / 1000; }

static double bench_elapsed(struct optics_bench *bench)
{
    return
        (bench->stop.tv_sec - bench->start.tv_sec) +
        ((bench->stop.tv_nsec - bench->start.tv_nsec) * nsec());
}

static double run_bench(
        struct optics_bench *bench,
        optics_bench_fn_t fn,
        void *ctx,
        size_t id,
        size_t n)
{
    fn(bench, ctx, id, n);

    if (!bench->stopped) optics_bench_stop(bench);
    optics_assert(bench->started, "bench_start was not called");

    return bench_elapsed(bench);
}

typedef void (* bench_policy) (optics_bench_fn_t, void *, size_t, size_t, double *);

int cmp_elapsed(const void *_lhs, const void *_rhs)
{
    double lhs = *((const double *) _lhs);
    double rhs = *((const double *) _rhs);

    if (lhs < rhs) return -1;
    if (lhs > rhs) return 1;
    return 0;
}


// Note that I'm aware that this calculates the quantiles over averages and that
// in the process I'm making millions of statisticians cry out in agony.
static void bench_report(
        const char *title, size_t n, size_t threads, double *dist, size_t dist_len)
{
    for (size_t i = 0; i < dist_len; ++i) dist[i] /= n;
    qsort(dist, dist_len, sizeof(double), cmp_elapsed);

    char p0_mul = ' ';
    double p0_val = scale_elapsed(dist[0], &p0_mul);

    char p50_mul = ' ';
    double p50_val = scale_elapsed(dist[dist_len / 2], &p50_mul);

    char p90_mul = ' ';
    double p90_val = scale_elapsed(dist[(dist_len * 90) / 100], &p90_mul);

    printf("bench: %-30s  %4lu %8lu    p0:%6.2f%c    p50:%6.2f%c    p90:%6.2f%c\n",
            title, threads, n, p0_val, p0_mul, p50_val, p50_mul, p90_val, p90_mul);
}

static void bench_runner(
        bench_policy pol,
        const char *title,
        optics_bench_fn_t fn,
        void *ctx,
        size_t threads)
{
    const double duration = 1 * msec();
    const size_t iterations = 1000;

    size_t dist_len = threads * iterations;
    double dist[dist_len];
    memset(dist, 0, dist_len * sizeof(double));

    size_t n = 1;
    double elapsed = -1;
    for (; elapsed < duration; n *= 2) {
        optics_assert(n < 100UL * 1000 * 1000 * 1000,
                "bench doesn't scale with n");

        pol(fn, ctx, n, threads, dist);

        elapsed = 0;
        for (size_t i = 0; i < threads; ++i) elapsed += dist[i];
        elapsed /= threads;
    }

    for (size_t i = 0; i < iterations; ++i)
        pol(fn, ctx, n, threads, &dist[i * threads]);

    bench_report(title, n, threads, dist, dist_len);
}


// -----------------------------------------------------------------------------
// bench st
// -----------------------------------------------------------------------------

static void bench_st_policy(
        optics_bench_fn_t fn, void *ctx, size_t n, size_t threads, double *dist)
{
    (void) threads;

    void *setup_data;
    struct optics_bench bench = { .setup_data = &setup_data };
    *dist = run_bench(&bench, fn, ctx, 0, n);
}

void optics_bench_st(const char *title, optics_bench_fn_t fn, void *ctx)
{
    bench_runner(bench_st_policy, title, fn, ctx, 1);
}


// -----------------------------------------------------------------------------
// bench mt
// -----------------------------------------------------------------------------

struct bench_mt
{
    size_t n;
    void *ctx;
    optics_bench_fn_t fn;

    double *dist;
    void *setup_data;
    struct sbarrier setup_barrier;
    struct sbarrier start_barrier;
    struct sbarrier stop_barrier;
};

static void bench_thread(size_t id, void *ctx)
{
    struct bench_mt *data = ctx;

    struct optics_bench bench = {
        .setup_data = &data->setup_data,
        .setup_barrier = &data->setup_barrier,
        .start_barrier = &data->start_barrier,
        .stop_barrier = &data->stop_barrier,
    };
    data->dist[id] = run_bench(&bench, data->fn, data->ctx, id, data->n);
}

static void bench_mt_policy(
        optics_bench_fn_t fn, void *ctx, size_t n, size_t threads, double *dist)
{
    struct bench_mt data = { .fn = fn, .ctx = ctx, .n = n, .dist = dist };
    sbarrier_init(&data.setup_barrier, threads);
    sbarrier_init(&data.start_barrier, threads);
    sbarrier_init(&data.stop_barrier, threads);

    run_threads(bench_thread, &data, threads);
}

void optics_bench_mt(const char *title, optics_bench_fn_t fn, void *ctx)
{
    bench_runner(bench_mt_policy, title, fn, ctx, cpus());
}
