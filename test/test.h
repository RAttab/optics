/* test.h
   Rémi Attab (remi.attab@gmail.com), 03 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

#include "optics.h"
#include "utils/thread.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <string.h>

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

#define assert_float_equal(a, b, epsilon)                       \
    do {                                                        \
        if (fabs(a - b) <= epsilon) break;                      \
        printf("fabs(%g - %g) <= %g\n",                         \
                (double) a, (double) b , (double) epsilon);     \
        assert_true(false);                                     \
    } while(false)
