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
    static __thread struct rng rng = {0};
    if (!rng.x && !rng.y && !rng.z && !rng.w) rng_init(&rng);
    return &rng;
}

static void rng_init_impl(struct rng *rng, uint64_t t, uint64_t u)
{
    *rng = (struct rng) {
        .x = 123456789,
        .y = 362436069,
        .z = 521288629,
        .w = 88675123
    };

    rng->x ^= t >> 32;
    rng->y ^= t;
    rng->z ^= u >> 32;
    rng->w ^= u;
}

void rng_init(struct rng *rng)
{
    uint64_t t, u;
    rdtsc(&t, &u);
    rng_init_impl(rng, t, u);
}

void rng_init_seed(struct rng *rng, uint64_t seed)
{
    rng_init_impl(rng, seed, seed);
}

uint32_t rng_gen(struct rng *rng)
{
    uint32_t t = rng->x ^ (rng->x << 11);
    rng->x = rng->y;
    rng->y = rng->z;
    rng->z = rng->w;
    return rng->w = rng->w ^ (rng->w >> 19) ^ (t ^ (t >> 8));
}

double rng_gen_float(struct rng *rng)
{
    static const double rand_mul = 2.32830643653869628906e-010; // 2 ^ -32
    return rng_gen(rng) * rand_mul;
}

uint32_t rng_gen_range(struct rng *rng, uint32_t min, uint32_t max)
{
    return rng_gen(rng) % (max - min) + min;
}
