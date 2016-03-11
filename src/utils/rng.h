/* rng.h
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

#include <stdint.h>

// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct rng { bool initialized; uint64_t x; };


// -----------------------------------------------------------------------------
// init
// -----------------------------------------------------------------------------

struct rng *rng_global();
void rng_seed(struct rng *rng);
void rng_seed_with(struct rng *rng, uint64_t seed);


// -----------------------------------------------------------------------------
// gen
// -----------------------------------------------------------------------------

uint64_t rng_gen(struct rng *rng);
double rng_gen_float(struct rng *rng);
uint64_t rng_gen_range(struct rng *rng, uint64_t min, uint64_t max);
