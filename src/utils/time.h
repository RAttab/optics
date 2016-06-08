/* time.h
   Rémi Attab (remi.attab@gmail.com), 03 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

#include "compiler.h"
#include "errors.h"

#include <time.h>
#include <stdint.h>
#include <stdbool.h>


// -----------------------------------------------------------------------------
// time
// -----------------------------------------------------------------------------

// intentionally duplicates the optics.h typedef of the same name. Turns out
// that gcc and clang won't complain if you typedef the same name to the same
// type but will if they difer. Which is perfect since we want to make sure they
// line up without having to include optics.h in this header or vice-versa.
typedef uint64_t optics_ts_t;

optics_ts_t clock_wall();
optics_ts_t clock_rdtsc();

inline void clock_monotonic(struct timespec *ts)
{
    if (optics_unlikely(clock_gettime(CLOCK_MONOTONIC, ts) == -1)) {
        optics_fail_errno("unable to read monotonic clock");
        optics_abort();
    }
}


// -----------------------------------------------------------------------------
// sleep
// -----------------------------------------------------------------------------

bool nsleep(uint64_t nanos);
void yield();
