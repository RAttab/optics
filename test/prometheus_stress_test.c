/* prometheus_stress_test.c
   Rémi Attab (remi.attab@gmail.com), 04 Aug 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "optics.h"
#include "utils/time.h"
#include "utils/rng.h"
#include "utils/crest/crest.h"

#include <stdio.h>

enum
{
    port = 40001,

    lens_len = 1UL << 16,
    churn = lens_len / 1000,
};

void lens_inc(struct optics_lens *lens, struct rng *rng)
{
    size_t value = rng_gen_range(rng, 1, rng_gen_range(rng, 2, 100));
    if (!optics_counter_inc(lens, value)) optics_abort();
}

void lens_cycle(struct optics *optics, struct optics_lens **lens, size_t id)
{
    if (*lens) {
        printf("rmv: %s\n", optics_lens_name(*lens));
        if(!optics_lens_free(*lens)) optics_abort();
    }

    char name[optics_name_max_len];
    snprintf(name, sizeof(name), "counter-%lu", id);

    *lens = optics_counter_alloc(optics, name);
    if (!*lens) optics_abort();

    printf("add: %s\n", optics_lens_name(*lens));
}

int main(int argc, const char **argv)
{
    (void) argc, (void) argv;

    struct crest *crest = crest_new();
    if (!crest) optics_abort();

    struct optics_poller *poller = optics_poller_alloc();
    if (!poller) optics_abort();

    optics_dump_prometheus(poller, crest);
    if (!optics_thread_start(poller, 1)) optics_abort();
    if (!crest_bind(crest, port)) optics_abort();

    struct rng rng;
    rng_seed_with(&rng, 0);

    size_t lens_id = 0;
    struct optics_lens *lens[lens_len] = {0};

    struct optics *optics = optics_create("prometheus-stress-test");
    if (!optics) optics_abort();

    if (!optics_set_source(optics, "source")) optics_abort();

    for (size_t i = 0; i < lens_len; ++i) {
        lens_cycle(optics, &lens[i], lens_id++);
        lens_inc(lens[i], &rng);
    }

    struct timespec ts;
    clock_monotonic(&ts);

    while (true) {
        ts.tv_sec++;
        nsleep_until(&ts);

        for (size_t i = 0; i < churn ; ++i) {
            size_t pos = rng_gen_range(&rng, 0, lens_len);
            lens_cycle(optics, &lens[pos], lens_id++);
        }

        for (size_t i = 0; i < lens_len; ++i)
            lens_inc(lens[i], &rng);
    }
}
