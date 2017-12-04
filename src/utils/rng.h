/* rng.h
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

#include <stdint.h>

// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct rng { uint64_t x; };


// -----------------------------------------------------------------------------
// init
// -----------------------------------------------------------------------------

struct rng *rng_global();
void rng_seed(struct rng *rng);
void rng_seed_with(struct rng *rng, uint64_t seed);

inline uint64_t rng_max() { return (uint64_t) -1UL; }


// -----------------------------------------------------------------------------
// gen
// -----------------------------------------------------------------------------

uint64_t rng_gen(struct rng *rng);
uint64_t rng_gen_range(struct rng *rng, uint64_t min, uint64_t max);

bool rng_gen_prob(struct rng *rng, double prob);
