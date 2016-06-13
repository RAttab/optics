/* crest.c
   Rémi Attab (remi.attab@gmail.com), 07 Jun 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "crest.h"
#include "utils/errors.h"

#include <microhttpd.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>


// -----------------------------------------------------------------------------
// implementation
// -----------------------------------------------------------------------------

#include "path.c"
#include "req.c"
#include "resp.c"
#include "router.c"


static int microhttpd_cb(
        void *ctx,
        struct MHD_Connection *conn,
        const char *url,
        const char *method,
        const char *version,
        const char *data,
        size_t *data_len,
        void **args);

static void microhttpd_logger_cb(void *ctx, const char *fmt, va_list args);

static void microhttpd_panic_cb(
        void *ctx, const char *file, unsigned int line, const char *reason);



// -----------------------------------------------------------------------------
// crest
// -----------------------------------------------------------------------------

struct crest
{
    struct router router;
    struct MHD_Daemon *mhd_daemon;

    bool started;
};

struct crest *crest_new()
{
    struct crest *crest = calloc(1, sizeof(struct crest));
    optics_assert_alloc(crest);
    return crest;
}

void crest_free(struct crest *crest)
{
    if (crest->started) MHD_stop_daemon(crest->mhd_daemon);
    router_free(&crest->router);
    free(crest);
}

bool crest_bind(struct crest *crest, int port)
{
    int flags = 0;
    flags |= MHD_USE_SELECT_INTERNALLY;
    flags |= MHD_USE_EPOLL_LINUX_ONLY;

    crest->mhd_daemon = MHD_start_daemon(
            flags, port, NULL, NULL, microhttpd_cb, crest,
            MHD_OPTION_EXTERNAL_LOGGER, microhttpd_logger_cb, crest,
            MHD_OPTION_END);

    if (!crest->mhd_daemon) {
        optics_fail("unable to start MHD daemon");
        return false;
    }

    MHD_set_panic_func(microhttpd_panic_cb, crest);

    crest->started = true;
    return true;
}

bool crest_add(struct crest *crest, struct crest_res raw_res)
{
    if (crest->started) return false;

    struct crest_res *res = malloc(sizeof(*res));
    optics_assert_alloc(res);
    memcpy(res, &raw_res, sizeof(*res));

    struct path *path = path_new(res->path);
    bool ret = router_add(&crest->router, path, res);

    path_free(path);
    if (!ret) free(res);

    return ret;
}

static void crest_call_cb(
        struct crest_req *req,
        struct crest_resp *resp,
        const struct crest_res *res,
        crest_resource_cb_t cb,
        int code)
{
    enum crest_result result = cb(res->context, req, resp);
    switch (result) {
    case crest_ok:       crest_resp_send(resp, code); return;
    case crest_err:      crest_resp_send_error(resp, 500, NULL); return;
    case crest_conflict: crest_resp_send_error(resp, 409, NULL); return;
    default:
        optics_fail("unknown crest callback return value: %d", result);
        optics_abort();
    }
}

static void crest_process(
        struct crest *crest, struct crest_req *req, struct crest_resp *resp)
{
    const struct path *path = crest_req_get_path(req);
    const struct crest_res *res = router_route(&crest->router, path);
    if (!res) {
        crest_resp_send_error(resp, 404, "unknown resource");
        return;
    }

    crest_resource_cb_t cb = NULL;
    enum crest_method method = crest_req_get_method(req);
    switch (method) {
    case crest_get: cb = res->get; break;
    case crest_put: cb = res->put; break;
    case crest_post: cb = res->post; break;
    case crest_delete: cb = res->delete; break;
    case crest_unsupported: break;
    default:
        optics_fail("unknown crest method value: %d", method);
        optics_abort();
    };
    if (!cb) {
        crest_resp_send_error(resp, 501, NULL);
        return;
    }

    if (res->authorized && !res->authorized(res->context, req)) {
        crest_resp_send_error(resp, 401, NULL);
        return;
    }

    if (res->forbidden && !res->forbidden(res->context, req)) {
        crest_resp_send_error(resp, 403, NULL);
        return;
    }

    if (res->accepts && !res->accepts(res->context, req)) {
        crest_resp_send_error(resp, 406, NULL);
        return;
    }

    if (!res->exists || res->exists(res->context, req)) {
        switch (method) {
        case crest_get: crest_call_cb(req, resp, res, cb, 200); return;
        case crest_post: crest_call_cb(req, resp, res, cb, 204); return;
        case crest_delete: crest_call_cb(req, resp, res, cb, 204); return;
        case crest_put:
            crest_resp_send_error(resp, 409, "resource already exists");
            return;
        case crest_unsupported:
        default:
            optics_fail("unknown crest method value: %d", method);
            optics_abort();
        }
    }
    else {
        switch(method) {
        case crest_put: crest_call_cb(req, resp, res, cb, 201); return;
        case crest_post: crest_call_cb(req, resp, res, cb, 201); return;
        case crest_get:
        case crest_delete:
            crest_resp_send_error(resp, 404, "resource doesn't exist");
            return;
        case crest_unsupported:
        default:
            optics_fail("unknown crest method value: %d", method);
            optics_abort();
        }
    }
}


// -----------------------------------------------------------------------------
// callbacks
// -----------------------------------------------------------------------------

struct body
{
    size_t cap;
    size_t pos;

    char data[];
};

static ssize_t get_content_length(struct MHD_Connection *conn)
{
    const char *value = MHD_lookup_connection_value(
            conn, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_LENGTH);
    if (!value) return 0;

    size_t len = strtoul(value, NULL, 10);
    if (len == ULONG_MAX && errno == ERANGE) {
        optics_warn_errno("invalid content-length header: %s", value);
        return -1;
    }

    return len;
}

static void body_append(struct body *body, const char *data, size_t *len)
{
    if (body->pos + *len > body->cap) {
        optics_warn("too many bytes to read: %lu + %lu > %lu",
                *len, body->pos, body->cap);

        *len = body->cap - body->pos;
    }

    memcpy(body->data + body->pos, data, *len);
    body->pos += *len;

    *len = 0;
}

/* \todo Should really be using the microhttpd MHD post processor.
 */
static int microhttpd_cb(
        void *ctx,
        struct MHD_Connection *conn,
        const char *url,
        const char *method,
        const char *version,
        const char *data,
        size_t *data_len,
        void **args)
{
    struct body *body = *args;

    if (!body) {
        ssize_t content_length = get_content_length(conn);
        if (content_length < 0) MHD_NO;
        if (content_length > 0) {
            *args = body = calloc(1, sizeof(*body) + content_length);
            optics_assert_alloc(body);

            body->cap = content_length;
            if (!*data_len) return MHD_YES;
        }
    }

    if (body && *data_len) {
        body_append(body, data, data_len);
        return MHD_YES;
    }

    struct crest_resp resp = { .conn = conn };
    struct crest_req req = {
        .conn = conn,
        .url = url,
        .method = method,
        .version = version,
        .data = body ? body->data : NULL,
        .data_len = body ? body->cap : 0
    };

    crest_process(ctx, &req, &resp);

    crest_resp_free(&resp);
    crest_req_free(&req);
    if (body) free(body);

    return MHD_YES;
}

static void microhttpd_logger_cb(void *ctx, const char *fmt, va_list args)
{
    (void) ctx;
    optics_warn_va(fmt, args);
}

static void microhttpd_panic_cb(
        void *ctx, const char *file, unsigned int line, const char *reason)
{
    (void) ctx;
    optics_fail("MHD %s:%d: %s", file, line, reason);
    optics_abort();
}
