/* req.h
   Rémi Attab (remi.attab@gmail.com), 08 Jun 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

// -----------------------------------------------------------------------------
// req
// -----------------------------------------------------------------------------

struct crest_req
{
    struct MHD_Connection *conn;

    const char *url;
    const char *method;
    const char *version;

    const char *data;
    size_t data_len;
    size_t data_pos;

    struct path *path;
};

static void crest_req_free(struct crest_req *req)
{
    if (req->path) path_free(req->path);
}

#define is_method(value, method)                        \
    (strncmp((method), (value), sizeof(method)) == 0)

enum crest_method crest_req_get_method(struct crest_req *req)
{
    if (is_method(req->method, "GET")) return crest_get;
    if (is_method(req->method, "POST")) return crest_post;
    if (is_method(req->method, "PUT")) return crest_put;
    if (is_method(req->method, "DELETE")) return crest_delete;
    return crest_unsupported;
}

#undef is_method

const char *crest_req_get_uri(struct crest_req *req)
{
    return req->url;
}

static const struct path *crest_req_get_path(struct crest_req *req)
{
    if (!req->path) req->path = path_new(req->url);
    return req->path;
}

size_t crest_req_get_path_tokens(struct crest_req *req)
{
    const struct path *path = crest_req_get_path(req);
    return path_tokens(path);
}

const char *crest_req_get_path_token(struct crest_req *req, size_t i)
{
    const struct path *path = crest_req_get_path(req);
    return path_token(path, i);
}

const char *crest_req_get_header(struct crest_req *req, const char *key)
{
    return MHD_lookup_connection_value(req->conn, MHD_HEADER_KIND, key);
}

size_t crest_req_read(struct crest_req *req, void *dest, size_t max)
{
    size_t leftover = req->data_len - req->data_pos;
    size_t len = max > leftover ? leftover : max;

    memcpy(dest, req->data, len);
    return len;
}
