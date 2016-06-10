/* resp.h
   Rémi Attab (remi.attab@gmail.com), 08 Jun 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

#include "utils/htable.h"
#include "utils/buffer.h"


// -----------------------------------------------------------------------------
// resp
// -----------------------------------------------------------------------------

struct crest_resp
{
    struct MHD_Connection *conn;

    struct htable headers;
    struct buffer body;
};

void crest_resp_free(struct crest_resp *resp)
{
    struct htable_bucket *it = htable_next(&resp->headers, NULL);
    for (; it; it = htable_next(&resp->headers, it))
        free((char *) it->value);
    htable_reset(&resp->headers);

    buffer_reset(&resp->body);
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
    buffer_write(&resp->body, body, len);
}

static void crest_resp_send(struct crest_resp *resp, int code)
{
    struct MHD_Response *mhd_resp = MHD_create_response_from_buffer(
            resp->body.len, resp->body.data, MHD_RESPMEM_MUST_FREE);
    if (!mhd_resp) {
        optics_fail("unable to create response of size '%zu'", resp->body.len);
        optics_abort();
    }

    // MHD_create_response_from_buffer takes ownership of the buffer's data so
    // we should not free it.
    resp->body = (struct buffer) {0};

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
