/* rng.c
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "rng.h"

static void rdtsc(uint64_t *t, uint64_t *u)
{
#ifdef __amd64
    __asm__ __volatile__ ("rdtsc" : "=a" (*t), "=d" (*u));
#else
#error "Read your platform's perf counter here"
#endif
}

struct rng *rng_global()
{
    static __thread struct rng rng;
    return &rng;
}

void rng_init(struct rng *rng)
{
    *rng = (struct rng) {
        .x = 123456789,
        .y = 362436069,
        .z = 521288629,
        .w = 88675123
    };

    uint64_t t, u;
    rdtsc(&t, &u);

    rng->x ^= t >> 32;
    rng->y ^= t;
    rng->z ^= u >> 32;
    rng->w ^= u;
}

uint32_t rng_gen(struct rng *rng)
{
    uint32_t t = rng->x ^ (rng->x << 11);
    rng->x = rng->y;
    rng->y = rng->z;
    rng->z = rng->w;
    return rng->w = rng->w ^ (rng->w >> 19) ^ (t ^ (t >> 8));
}

uint32_t rng_gen_range(struct rng *rng, uint32_t min, uint32_t max)
{
    return rng_gen(rng) % (max - min) + min;
}
