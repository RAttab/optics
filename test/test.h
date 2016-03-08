/* test.h
   Rémi Attab (remi.attab@gmail.com), 03 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

#include "optics.h"
#include "poller.h"
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

bool optics_assert_float_equal(double a, double b, double epsilon);

#define assert_float_equal(a, b, epsilon)                       \
    assert_true(optics_assert_float_equal(a, b, epsilon))


// -----------------------------------------------------------------------------
// set
// -----------------------------------------------------------------------------

struct kv
{
    const char *key;
    double value;
};

struct optics_set
{
    size_t n;
    size_t cap;
    struct kv *entries;
};

void optics_set_reset(struct optics_set *);
bool optics_set_test(struct optics_set *, const char *key);
bool optics_set_put(struct optics_set *, const char *key, double value);
void optics_set_diff(
        struct optics_set *a, struct optics_set *b, struct optics_set *result);

size_t optics_set_print(struct optics_set *, char *dest, size_t max);
bool optics_set_assert_equal(
        struct optics_set *, const struct kv *kv, size_t kv_len, double epsilon);

#define make_kv(k, v) {k, v}

#define assert_set_equal(set, eps, ...)                                 \
    do {                                                                \
        struct kv exp[] = { __VA_ARGS__ };                              \
        size_t len = sizeof(exp) / sizeof(struct kv);                   \
        assert_true(optics_set_assert_equal(set, exp, len, eps));       \
    } while (false)
