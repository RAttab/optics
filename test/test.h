/* test.h
   Rémi Attab (remi.attab@gmail.com), 03 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

#include "optics.h"

#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>

#include <cmocka.h>


// -----------------------------------------------------------------------------
// test wrappers
// -----------------------------------------------------------------------------

#define optics_test_head(name)                  \
    void name(optics_unused void **test_state)  \
    {                                           \
        const char *test_name = #name;          \
        optics_unlink(test_name);               \
        do

#define optics_test_tail()                      \
        while (false);                          \
    }


// -----------------------------------------------------------------------------
// asserts
// -----------------------------------------------------------------------------

inline void assert_float_equal(double a, double b, double epsilon)
{
    assert_true(fabs(a - b) <= epsilon);
}
