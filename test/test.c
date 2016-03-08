/* test.c
   Rémi Attab (remi.attab@gmail.com), 03 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"


// -----------------------------------------------------------------------------
// assert
// -----------------------------------------------------------------------------

bool optics_assert_float_equal(double a, double b, double epsilon)
{
    if (fabs(a - b) <= epsilon) return true;

    printf("fabs(%g - %g) <= %g\n", a, b, epsilon);
    return false;
}


// -----------------------------------------------------------------------------
// set
// -----------------------------------------------------------------------------

void optics_set_reset(struct optics_set *set)
{
    for (size_t i = 0; i < set->n; ++i)
        free((char *) set->entries[i].key);
    set->n = 0;
}

struct kv *optics_set_get(struct optics_set *set, const char *key)
{
    for (size_t i = 0; i < set->n; ++i) {
        int result = strncmp(key, set->entries[i].key, optics_name_max_len);
        if (!result) return &set->entries[i];;
        if (result < 0) return NULL;
    }

    return NULL;
}

bool optics_set_put(struct optics_set *set, const char *key, double value)
{
    if (!set->cap) {
        set->cap = 8;
        set->entries = calloc(set->cap, sizeof(struct kv));
    }

    if (set->n == set->cap) {
        set->cap *= 2;
        set->entries = realloc(set->entries, set->cap * sizeof(struct kv));
    }

    size_t i;
    for (i = 0; i < set->n; ++i) {
        int result = strncmp(key, set->entries[i].key, optics_name_max_len);
        if (!result) return false;
        if (result < 0) break;
    }

    memmove(&set->entries[i + 1], &set->entries[i], (set->n - i) * sizeof(struct kv));

    key = strndup(key, optics_name_max_len);
    set->entries[i] = (struct kv) { .key = key, .value = value };
    set->n++;

    return true;
}

void optics_set_diff(
        struct optics_set *a, struct optics_set *b, struct optics_set *result)
{
    for (size_t i = 0; i < a->n; i++) {
        struct kv *kv = &a->entries[i];

        if (optics_set_get(b, kv->key)) continue;
        optics_set_put(result, kv->key, kv->value);
    }
}

size_t optics_set_print(struct optics_set *set, char *dest, size_t max)
{
    size_t dest_i = snprintf(dest, max, "%lu:[ ", set->n);

    for (size_t i = 0; i < set->n; ++i) {
        dest_i += snprintf(dest + dest_i, max - dest_i, "{%s, %f} ",
                set->entries[i].key, set->entries[i].value);
    }

    return dest_i + snprintf(dest + dest_i, max - dest_i, "]");
}

bool optics_set_assert_equal(
        struct optics_set *a, const struct kv *kv, size_t kv_len, double epsilon)
{
    struct optics_set b = {0};
    for (size_t i = 0; i < kv_len; ++i)
        optics_set_put(&b, kv[i].key, kv[i].value);

    char buffer[1024];
    bool result = true;

    struct optics_set diff = {0};
    optics_set_diff(a, &b, &diff);
    if (diff.n) {
        result = false;
        optics_set_print(&diff, buffer, sizeof(buffer));
        printf("extra: %s\n", buffer);
    }

    optics_set_reset(&diff);
    optics_set_diff(&b, a, &diff);
    if (diff.n) {
        result = false;
        optics_set_print(&diff, buffer, sizeof(buffer));
        printf("missing: %s\n", buffer);
    }

    for (size_t i = 0; i < a->n; ++i) {
        struct kv *kv = optics_set_get(&b, a->entries[i].key);
        if (!kv) continue;

        if (!optics_assert_float_equal(a->entries[i].value, kv->value, epsilon)) {
            printf("  -> %s\n", kv->key);
            result = false;
        }
    }

    return result;
}
