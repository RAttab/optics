/* test.h
   Rémi Attab (remi.attab@gmail.com), 03 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

#include "optics_priv.h"
#include "utils/errors.h"
#include "utils/log.h"
#include "utils/rng.h"
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

void assert_mt();

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

// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

struct optics *optics_create_idx_at(
        const char *base, size_t index, optics_ts_t ts);


// -----------------------------------------------------------------------------
// http
// -----------------------------------------------------------------------------

struct http_client;
struct http_client * http_connect(unsigned port);
void http_close(struct http_client *client);

void http_req(struct http_client *, const char *method, const char *path, const char *body);
bool http_assert_resp(struct http_client *, unsigned exp_code, const char *exp_body);


#define assert_http_code(port, method, path, exp)               \
    do {                                                        \
        struct http_client *client = http_connect(port);        \
        http_req(client, method, path, NULL);                   \
        assert_true(http_assert_resp(client, exp, NULL));       \
        http_close(client);                                     \
    } while (false)

#define assert_http_body(port, method, path, exp_code, exp_data)        \
    do {                                                                \
        struct http_client *client = http_connect(port);                \
        http_req(client, method, path, NULL);                           \
        assert_true(http_assert_resp(client, exp_code, exp_data));      \
        http_close(client);                                             \
    } while (false)

#define assert_http_post(port, method, path, body, exp)         \
    do {                                                        \
        struct http_client *client = http_connect(port);        \
        http_req(client, method, path, body);                   \
        assert_true(http_assert_resp(client, exp, NULL));       \
        http_close(client);                                     \
    } while (false)

