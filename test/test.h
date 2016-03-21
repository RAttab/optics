/* test.h
   Rémi Attab (remi.attab@gmail.com), 03 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

#include "optics_priv.h"
#include "poller.h"
#include "utils/thread.h"
#include "utils/htable.h"
#include "utils/type_pun.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <string.h>
#include <stdatomic.h>

#include <cmocka.h>


// -----------------------------------------------------------------------------
// test wrappers
// -----------------------------------------------------------------------------

#define optics_test_head(name)                          \
    void name(optics_unused void **test_state)          \
    {                                                   \
        optics_unused const char * test_name = #name;   \
        optics_unlink_all();                            \
        do

#define optics_test_tail()                      \
        while (false);                          \
    }


// -----------------------------------------------------------------------------
// asserts
// -----------------------------------------------------------------------------

bool assert_float_equal_impl(double a, double b, double epsilon);

#define assert_float_equal(a, b, epsilon)                       \
    assert_true(assert_float_equal_impl(a, b, epsilon))


bool assert_htable_equal_impl(
        struct htable *,
        const struct htable_bucket *buckets,
        size_t buckets_len,
        double epsilon);

#define make_kv(k, v) {k, pun_dtoi(v)}

#define assert_htable_equal(set, eps, ...)                          \
    do {                                                                \
        struct htable_bucket exp[] = { __VA_ARGS__ };                   \
        size_t len = sizeof(exp) / sizeof(struct htable_bucket);        \
        assert_true(assert_htable_equal_impl(set, exp, len, eps));  \
    } while (false)
