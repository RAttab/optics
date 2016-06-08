/* crest_path.h
   Rémi Attab (remi.attab@gmail.com), 08 Jun 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

// -----------------------------------------------------------------------------
// path
// -----------------------------------------------------------------------------

enum token_type
{
    token_star,
    token_wildcard,
    token_const,
};

struct path
{
    char *raw;
    size_t i, n;
    const char **items;
};

static struct path *path_new(const char *raw)
{
    struct path *path = calloc(1, sizeof(*path));

    enum { max_path_len = 1024 };
    size_t raw_len = strnlen(raw, max_path_len);
    path->raw = strndup(raw, max_path_len);

    for (size_t i = 0; i < raw_len; ++i) {
        if (raw[i] == '?') break;
        if (raw[i] == '/') continue;
        if (i && raw[i - 1] != '/') continue;
        path->n++;
    }

    if (path->n) path->items = malloc(path->n * sizeof(path->items[0]));

    size_t j = 0;
    for (size_t i = 0; i < raw_len; ++i) {
        if (raw[i] == '?') { path->raw[i] = '\0'; break; }
        if (raw[i] == '/') { path->raw[i] = '\0'; continue; }
        if (i && raw[i - 1] != '/') continue;
        path->items[j] = &path->raw[i];
        j++;
    }

    return path;
}

static void path_free(struct path *path)
{
    free(path->items);
    free(path->raw);
    free(path);
}

static bool path_empty(const struct path *path)
{
    return path->i == path->n;
}

static const char *path_head(const struct path *path)
{
    if (path->i == path->n) return NULL;
    return path->items[path->i];
}

static struct path path_tail(const struct path *path)
{
    size_t i = path->i < path->n ? path->i + 1 : path->n;
    return (struct path) { .i = i, .n = path->n, .items = path->items };
}

static size_t path_tokens(const struct path *path)
{
    return path->n;
}

static const char *path_token(const struct path *path, size_t i)
{
    return i < path->n ? path->items[i] : NULL;
}

static enum token_type path_token_type(const char *item)
{
    switch (item[0]) {
    case '*': return token_star;
    case ':': return token_wildcard;
    default: return token_const;
    }
}

