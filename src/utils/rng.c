/* rng.c
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply

   Xorshift random number generator for testing and statsd sampling

   See George Marsaglia (2003). Xorshift RNGs.  DOI: 10.18637/jss.v008.i14
     http://www.jstatsoft.org/article/view/v008i14
   (section 4, function xor128)

   Current implementation is the xorshift64* variant which has better
   statistical properties.
*/

#include "rng.h"


// -----------------------------------------------------------------------------
// init
// -----------------------------------------------------------------------------

struct rng *rng_global()
{
    static __thread struct rng rng = {0};
    if (!rng.x) rng_seed(&rng);
    return &rng;
}

void rng_seed(struct rng *rng)
{
    rng_seed_with(rng, clock_rdtsc());
}

void rng_seed_with(struct rng *rng, uint64_t seed)
{
    // We xor the seed with a randomly chosen number to avoid ending up with a 0
    // state which would be bad.
    rng->x = seed ^ UINT64_C(0xedef335f00e170b3);
    optics_assert(rng->x, "invalid nil state for rng");
}



// -----------------------------------------------------------------------------
// gen
// -----------------------------------------------------------------------------

uint64_t rng_gen(struct rng *rng)
{
    rng->x ^= rng->x >> 12;
    rng->x ^= rng->x << 25;
    rng->x ^= rng->x >> 27;
    return rng->x * UINT64_C(2685821657736338717);
}

uint64_t rng_gen_range(struct rng *rng, uint64_t min, uint64_t max)
{
    return rng_gen(rng) % (max - min) + min;
}

bool rng_gen_prob(struct rng *rng, double prob)
{
    return rng_gen(rng) <= (uint64_t) (prob * rng_max());
}
