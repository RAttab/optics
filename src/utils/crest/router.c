/* router.c
   Rémi Attab (remi.attab@gmail.com), 08 Jun 2016
   FreeBSD-style copyright and disclaimer apply
*/

// -----------------------------------------------------------------------------
// router
// -----------------------------------------------------------------------------

struct routing_entry;

struct router
{
    struct crest_res *star;
    struct crest_res *exact;
    struct router *wildcards;

    size_t len, cap;
    struct routing_entry *consts;

};

struct routing_entry
{
    char *value;
    struct router router;
};

static void router_free(struct router *router)
{
    if (router->star) free(router->star);
    if (router->exact) free(router->exact);
    if (router->wildcards) {
        router_free(router->wildcards);
        free(router->wildcards);
    }

    for (size_t i = 0; i < router->len; ++i) {
        free(router->consts[i].value);
        router_free(&router->consts[i].router);
    }
    free(router->consts);
}


static struct router *router_append(struct router *router, const char *key)
{
    if (router->len == router->cap) {
        router->cap = router->cap ? router->cap * 2 : 1;
        router->consts = realloc(
                router->consts, router->cap * sizeof(router->consts[0]));
        memset(router->consts + router->len, 0,
                (router->cap - router->len) * sizeof(router->consts[0]));
    }

    router->len++;
    router->consts[router->len - 1].value = strdup(key);
    return &router->consts[router->len - 1].router;
}

static bool router_add(
        struct router *router,
        const struct path *path,
        struct crest_res *res)
{
    if (path_empty(path)) {
        if (router->exact) return false;
        router->exact = res;
        return true;
    }

    const char *head = path_head(path);
    if (path_token_type(head) ==  token_star) {
        if (router->star) return false;
        router->star = res;
        return true;
    }

    struct path tail = path_tail(path);
    if (path_token_type(head) == token_wildcard) {
        if (!router->wildcards)
            router->wildcards = calloc(1, sizeof(*router->wildcards));
        return router_add(router->wildcards, &tail, res);
    }

    for (size_t i = 0; i < router->len; ++i) {
        if (strcmp(router->consts[i].value, head)) continue;
        return router_add(&router->consts[i].router, &tail, res);
    }

    struct router *sub = router_append(router, head);
    return router_add(sub, &tail, res);
}

static const struct crest_res *router_route(
        const struct router *router,
        const struct path *path)
{
    if (path_empty(path)) {
        if (router->exact) return router->exact;
        if (router->star) return router->star;
        return NULL;
    }

    const char *head = path_head(path);
    struct path tail = path_tail(path);

    for (size_t i = 0; i < router->len; ++i) {
        if (strcmp(router->consts[i].value, head)) continue;
        const struct crest_res *res = router_route(&router->consts[i].router, &tail);
        if (res) return res;
        break;
    }

    if (router->wildcards) {
        const struct crest_res *res = router_route(router->wildcards, &tail);
        if (res) return res;
    }

    if (router->star) return router->star;
    return NULL;
}

