/* type_pun.h
   Rémi Attab (remi.attab@gmail.com), 10 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once


// -----------------------------------------------------------------------------
// float - int
// -----------------------------------------------------------------------------

inline double pun_itod(uint64_t value)
{
    return (union { uint64_t i; double d; }) { .i = value }.d;
}

inline uint64_t pun_dtoi(double value)
{
    return (union { uint64_t i; double d; }) { .d = value }.i;
}
