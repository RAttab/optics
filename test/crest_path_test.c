/* crest_path_test.c
   Rémi Attab (remi.attab@gmail.com), 08 Jun 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "utils/crest/crest.h"
#include "utils/crest/path.c"



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
    if (path->n != exp_len) result = false;
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

