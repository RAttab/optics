/* type_pun.h
   Rémi Attab (remi.attab@gmail.com), 10 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

#include <assert.h>

// -----------------------------------------------------------------------------
// float - int
// -----------------------------------------------------------------------------

static_assert(sizeof(uint64_t) == sizeof(double), "portatibility issue");

inline double pun_itod(uint64_t value)
{
    return (union { uint64_t i; double d; }) { .i = value }.d;
}

inline uint64_t pun_dtoi(double value)
{
    return (union { uint64_t i; double d; }) { .d = value }.i;
}


static_assert(sizeof(uint64_t) == sizeof(void *), "portatibility issue");

inline void * pun_itop(uint64_t value)
{
    return (union { uint64_t i; void *p; }) { .i = value }.p;
}

inline uint64_t pun_ptoi(void * value)
{
    return (union { uint64_t i; void *p; }) { .p = value }.i;
}

