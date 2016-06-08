/* resp.h
   Rémi Attab (remi.attab@gmail.com), 08 Jun 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

#include "utils/htable.h"


// -----------------------------------------------------------------------------
// resp
// -----------------------------------------------------------------------------

struct crest_resp
{
    struct MHD_Connection *conn;

    struct htable headers;

    void *data;
    size_t data_cap;
    size_t data_len;
};

void crest_resp_free(struct crest_resp *resp)
{
    struct htable_bucket *it = htable_next(&resp->headers, NULL);
    for (; it; it = htable_next(&resp->headers, it))
        free((char *) it->value);
}

void crest_resp_add_header(struct crest_resp *resp, const char *key, const char *value)
{
    uint64_t data = (uintptr_t) strdup(value);

    if (!htable_get(&resp->headers, key).ok)
        htable_put(&resp->headers, key, data);

    else {
        struct htable_ret ret = htable_xchg(&resp->headers, key, data);
        free((char *) ret.value);
    }
}

void crest_resp_write(struct crest_resp *resp, const void *body, size_t len)
{
    if (resp->data_cap < resp->data_len + len) {
        if (!resp->data) resp->data = malloc(resp->data_cap = len);
        else {
            resp->data_cap = (resp->data_len + len) * 2;
            resp->data = realloc(resp->data, resp->data_cap);
        }
    }

    memcpy(resp->data + resp->data_len, body, len);
    resp->data_len += len;
}

static void crest_resp_send(struct crest_resp *resp, int code)
{
    struct MHD_Response *mhd_resp = MHD_create_response_from_buffer(
            resp->data_len, resp->data, MHD_RESPMEM_MUST_FREE);
    if (!mhd_resp) {
        optics_fail("unable to create response of size '%lu'", resp->data_len);
        optics_abort();
    }

    struct htable_bucket *it = htable_next(&resp->headers, NULL);
    for (; it; it = htable_next(&resp->headers, it)) {

        const char *value = (const char *) it->value;
        if (MHD_add_response_header(mhd_resp, it->key, value) == MHD_NO) {
            optics_fail("unable to add header '%s: %s'", it->key, value);
            optics_abort();
        }
    }

    if (MHD_queue_response(resp->conn, code, mhd_resp) == MHD_NO) {
        optics_fail("unable to queue response");
        optics_abort();
    }

    MHD_destroy_response(mhd_resp);
}

static void crest_resp_send_error(struct crest_resp *resp, int code, const char *reason)
{
    crest_resp_write(resp, reason, strnlen(reason, 1024));
    crest_resp_send(resp, code);
}
