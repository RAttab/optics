/* crest.h
   Rémi Attab (remi.attab@gmail.com), 07 Jun 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

#include <stddef.h>
#include <stdbool.h>

enum crest_method
{
    crest_get,
    crest_put,
    crest_post,
    crest_delete,
    crest_unsupported,
};

struct crest_req;
enum crest_method crest_req_get_method(struct crest_req *);
const char *crest_req_get_uri(struct crest_req *req);
size_t crest_req_get_path_tokens(struct crest_req *);
const char *crest_req_get_path_token(struct crest_req *, size_t i);
const char *crest_req_get_header(struct crest_req *, const char *key);
size_t crest_req_read(struct crest_req *req, void *dest, size_t max);


struct crest_resp;
void crest_resp_add_header(struct crest_resp *, const char *key, const char *value);
void crest_resp_write(struct crest_resp *, const void *body, size_t len);

enum crest_result
{
    crest_ok,
    crest_err,
    crest_conflict,
};

typedef bool (* crest_test_cb_t) (void *, struct crest_req *);
typedef enum crest_result (* crest_resource_cb_t) (void *, struct crest_req *, struct crest_resp *);

struct crest_res
{
    const char *path;
    void *context;

    crest_test_cb_t authorized;
    crest_test_cb_t forbidden;
    crest_test_cb_t accepts;
    crest_test_cb_t exists;

    crest_resource_cb_t get;
    crest_resource_cb_t put;
    crest_resource_cb_t post;
    crest_resource_cb_t delete;
};

struct crest;
struct crest * crest_new();
void crest_free(struct crest *);
bool crest_add(struct crest *, struct crest_res res);
bool crest_bind(struct crest *, int port);

