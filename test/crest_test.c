/* crest_test.c
   Rémi Attab (remi.attab@gmail.com), 08 Jun 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"
#include "utils/time.h"
#include "utils/errors.h"
#include "utils/crest/crest.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>

#include <cmocka.h>


// -----------------------------------------------------------------------------
// implementation
// -----------------------------------------------------------------------------

#include "utils/crest/path.c"
#include "utils/crest/router.c"


// -----------------------------------------------------------------------------
// test - path
// -----------------------------------------------------------------------------

static void path_print(const struct path *path)
{
    printf("{ ");
    for (size_t i = 0; i < path->n; ++i)
        printf("'%s' ", path->items[i]);
    printf("}");
}

bool check_path_(const char *str, const char *exp[], size_t exp_len)
{
    struct path *path = path_new(str);

    bool result = true;
    if (path_tokens(path) != exp_len) result = false;
    else {
        for (size_t i = 0; i < exp_len; ++i) {
            result = result && !strcmp(path_token(path, i), exp[i]);
        }

        const char *head = path_head(path);
        struct path tail = path_tail(path);
        for (size_t i = 0; i < exp_len; ++i) {
            result = result && !strcmp(head, exp[i]);
            head = path_head(&tail);
            tail = path_tail(&tail);
        }

        result = result && path_empty(&tail);
    }

    if (!result) {
        printf("FAIL: %s != ", str);
        path_print(path);
        printf("\n");
    }

    path_free(path);
    return result;
}

#define check_path(str, ...)                                            \
    do {                                                                \
        const char* exp[] = { __VA_ARGS__ };                            \
        assert_true(check_path_(str, exp, sizeof(exp) / sizeof(exp[0]))); \
    } while (false)

void path_test(void **state)
{
    (void) state;

    check_path("");
    check_path("/");
    check_path("//");

    check_path("a", "a");
    check_path("/a", "a");
    check_path("a/", "a");
    check_path("/a/", "a");

    check_path("blah", "blah");
    check_path("/blah", "blah");
    check_path("blah/", "blah");
    check_path("/blah/", "blah");

    check_path("a/b", "a", "b");
    check_path("a//b", "a", "b");
    check_path("a///b", "a", "b");
    check_path("/a/b", "a", "b");
    check_path("//a/b", "a", "b");
    check_path("a/b/", "a", "b");
    check_path("a/b//", "a", "b");

    check_path(":", ":");
    check_path(":a", ":a");
    check_path("/:a/:b", ":a", ":b");
    check_path("/a/:b/c", "a", ":b", "c");
    check_path("/a/*", "a", "*");
}

// -----------------------------------------------------------------------------
// test - router
// -----------------------------------------------------------------------------

bool check_router_add_(struct router *router, struct crest_res *res)
{
    struct path *path = path_new(res->path);
    bool result = router_add(router, path, res);
    path_free(path);
    return result;
}

#define check_router_add(router, name, path_)           \
    struct crest_res *name = malloc(sizeof(*name));     \
    do {                                                \
        *name = (struct crest_res) { .path = path_ };   \
        assert_true(check_router_add_(&router, name));  \
    } while(false)

#define fail_router_add(router, path_)                          \
    do {                                                        \
        struct crest_res blah = { .path = path_ };              \
        assert_false(check_router_add_(&router, &blah));        \
    } while (false)

bool check_router_route_(
        struct router *router, const char *raw_path, struct crest_res *exp)
{
    struct path *path = path_new(raw_path);
    const struct crest_res *result = router_route(router, path);

    path_free(path);
    return result == exp;
}

#define check_router_route(router, path, exp)                   \
    assert_true(check_router_route_(&router, path, exp))

void router_test(void **state)
{
    (void) state;
    struct router router = {0};

    check_router_add(router, r_a, "/a");
    check_router_add(router, r_a_b, "/a/b");
    check_router_add(router, r_a_b_c, "/a/b/c");
    check_router_add(router, r_a_d_e, "/a/d/e");
    check_router_add(router, r_b_d_e, "/b/d/e");
    fail_router_add(router, "/a");
    fail_router_add(router, "/a/b");

    check_router_add(router, r_s, "/*");
    check_router_add(router, r_a_s, "/a/*");
    check_router_add(router, r_c_s, "/c/*");
    fail_router_add(router, "/*");
    fail_router_add(router, "/a/*");

    check_router_add(router, r_w, "/:");
    check_router_add(router, r_a_w, "/a/:");
    check_router_add(router, r_a_w_c, "/a/:/c");
    check_router_add(router, r_w_w_e, "/:/:/e");
    fail_router_add(router, "/:");
    fail_router_add(router, "/a/:");

    check_router_route(router, "/a", r_a);
    check_router_route(router, "/a/b", r_a_b);
    check_router_route(router, "/a/b/c", r_a_b_c);
    check_router_route(router, "/a/b/c/d", r_a_s);

    check_router_route(router, "/a/c", r_a_w);
    check_router_route(router, "/a/c/d", r_a_s);
    check_router_route(router, "/a/c/c", r_a_w_c);

    check_router_route(router, "/a/d/e", r_a_d_e);
    check_router_route(router, "/b/d/e", r_b_d_e);
    check_router_route(router, "/c/d/e", r_c_s);
    check_router_route(router, "/d/d/e", r_w_w_e);

    check_router_route(router, "/blah", r_w);
    check_router_route(router, "/blah/bleh", r_s);

    router_free(&router);
}


// -----------------------------------------------------------------------------
// test - integration
// -----------------------------------------------------------------------------

struct test_svc
{
    bool exists;

    size_t puts;
    enum crest_result put_result;
    const char *put_data;

    size_t posts;
    enum crest_result post_result;
    const char *post_data;

    size_t deletes;
    enum crest_result delete_result;

    size_t gets;
    enum crest_result get_result;
    const char *get_data;
};

bool test_svc_exists(void *ctx, struct crest_req *req)
{
    (void) req;

    return ((struct test_svc *) ctx)->exists;
}

enum crest_result test_svc_put(void *ctx, struct crest_req *req, struct crest_resp *resp)
{
    (void) req, (void) resp;

    struct test_svc *svc = ctx;
    svc->puts++;

    if (svc->put_data) {
        char buffer[4096] = { 0 };
        crest_req_read(req, &buffer, sizeof(buffer));
        if (strcmp(buffer, svc->put_data)) {
            printf("FAIL(put.body): %s != %s\n", buffer, svc->put_data);
            assert_true(false);
        }
    }

    return svc->put_result;
}

enum crest_result test_svc_post(void *ctx, struct crest_req *req, struct crest_resp *resp)
{
    (void) resp;

    struct test_svc *svc = ctx;
    svc->posts++;

    char buffer[4096] = { 0 };
    crest_req_read(req, &buffer, sizeof(buffer));
    if (strcmp(buffer, svc->post_data)) {
        printf("FAIL(post.body): %s != %s\n", buffer, svc->post_data);
        assert_true(false);
    }

    return svc->post_result;
}

enum crest_result test_svc_delete(void *ctx, struct crest_req *req, struct crest_resp *resp)
{
    (void) req, (void) resp;

    struct test_svc *svc = ctx;
    svc->deletes++;
    return svc->delete_result;
}

enum crest_result test_svc_get(void *ctx, struct crest_req *req, struct crest_resp *resp)
{
    (void) req;

    struct test_svc *svc = ctx;
    svc->gets++;
    if (svc->get_result != crest_ok) return svc->get_result;

    crest_resp_write(resp, svc->get_data, strlen(svc->get_data));
    return crest_ok;
}


void microhttpd_test(void **state)
{
    (void) state;
    enum { port = 40123 };

    struct crest *crest = crest_new();

    struct test_svc s_get_new = {
        .exists = false,
        .get_result = crest_ok,
        .get_data = "hello"
    };
    crest_add(crest, (struct crest_res) {
                .path = "/test/get/new",
                .context = &s_get_new,
                .exists = test_svc_exists,
                .get = test_svc_get });

    struct test_svc s_get_old = {
        .exists = true,
        .get_result = crest_ok,
        .get_data = "hello"
    };
    crest_add(crest, (struct crest_res) {
                .path = "/test/get/old",
                .context = &s_get_old,
                .exists = test_svc_exists,
                .get = test_svc_get });

    struct test_svc s_put_new = { .exists = false, .put_result = crest_ok };
    crest_add(crest, (struct crest_res) {
                .path = "/test/put/new",
                .context = &s_put_new,
                .exists = test_svc_exists,
                .put = test_svc_put });

    struct test_svc s_put_old = { .exists = true, .put_result = crest_ok };
    crest_add(crest, (struct crest_res) {
                .path = "/test/put/old",
                .context = &s_put_old,
                .exists = test_svc_exists,
                .put = test_svc_put });

    struct test_svc s_put_body = {
        .exists = false,
        .put_result = crest_ok,
        .put_data = "hello" };
    crest_add(crest, (struct crest_res) {
                .path = "/test/put/body",
                .context = &s_put_body,
                .exists = test_svc_exists,
                .put = test_svc_put });


    struct test_svc s_post_new = {
        .exists = false,
        .post_result = crest_ok,
        .post_data = "hello" };
    crest_add(crest, (struct crest_res) {
                .path = "/test/post/new",
                .context = &s_post_new,
                .exists = test_svc_exists,
                .post = test_svc_post });

    struct test_svc s_post_old = {
        .exists = true,
        .post_result = crest_ok,
        .post_data = "hello" };
    crest_add(crest, (struct crest_res) {
                .path = "/test/post/old",
                .context = &s_post_old,
                .exists = test_svc_exists,
                .post = test_svc_post });

    struct test_svc s_delete_new = { .exists = false, .delete_result = crest_ok };
    crest_add(crest, (struct crest_res) {
                .path = "/test/delete/new",
                .context = &s_delete_new,
                .exists = test_svc_exists,
                .delete = test_svc_delete });

    struct test_svc s_delete_old = { .exists = true, .delete_result = crest_ok };
    crest_add(crest, (struct crest_res) {
                .path = "/test/delete/old",
                .context = &s_delete_old,
                .exists = test_svc_exists,
                .delete = test_svc_delete });

    if (!crest_bind(crest, port)) optics_abort();

    assert_http_code(port, "GET", "/test/get/new", 404);
    assert_http_body(port, "GET", "/test/get/old", 200, s_get_old.get_data);

    assert_http_code(port, "PUT", "/test/put/new", 201);
    assert_http_code(port, "PUT", "/test/put/old", 409);
    assert_http_post(port, "PUT", "/test/put/body", s_put_body.put_data, 201);

    assert_http_post(port, "POST", "/test/post/new", s_post_new.post_data, 201);
    assert_http_post(port, "POST", "/test/post/old", s_post_old.post_data, 204);

    assert_http_code(port, "DELETE", "/test/delete/new", 404);
    assert_http_code(port, "DELETE", "/test/delete/old", 204);

    crest_free(crest);
}


// -----------------------------------------------------------------------------
// test - main
// -----------------------------------------------------------------------------

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(path_test),
        cmocka_unit_test(router_test),
        cmocka_unit_test(microhttpd_test),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
