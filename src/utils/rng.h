/* rng.h
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

// -----------------------------------------------------------------------------
// rng
// -----------------------------------------------------------------------------

struct rng
{
    uint32_t x, y, z, w;
};

struct rng *rng_global();
void rng_init(struct rng *rng, uint32_t seed);
uint32_t rng_gen(struct rng *rng);
uint32_t rng_gen_range(struct rng *rng, uint32_t min, uint32_t max);
