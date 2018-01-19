/* backend_rest.c
   Rémi Attab (remi.attab@gmail.com), 09 Jun 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "optics.h"
#include "utils/errors.h"
#include "utils/lock.h"
#include "utils/buffer.h"
#include "utils/crest/crest.h"

#include <string.h>
#include <inttypes.h>


// -----------------------------------------------------------------------------
// metrics
// -----------------------------------------------------------------------------

struct metric
{
    char *key;
    enum optics_lens_type type;
    union optics_poll_value value;
};

struct metrics
{
    size_t len;
    size_t cap;
    struct metric data[];
};

enum { metrics_init_cap = 128 };

static void metrics_free(struct metrics *metrics)
{
    if (!metrics) return;

    for (size_t i = 0; i < metrics->len; ++i)
        free(metrics->data[i].key);

    free(metrics);
}

static struct metrics *metrics_append(struct metrics *metrics, const struct optics_poll *poll)
{
    if (!metrics) {
        metrics = malloc(sizeof(struct metrics) + sizeof(struct metric) * metrics_init_cap);
        optics_assert_alloc(metrics);
        metrics->len = 0;
        metrics->cap = metrics_init_cap;
    }

    if (metrics->len == metrics->cap) {
        metrics->cap *= 2;
        metrics = realloc(metrics, sizeof(struct metrics) + sizeof(struct metric) * metrics->cap);
        optics_assert_alloc(metrics);
    }

    struct optics_key key = {0};
    optics_key_push(&key, poll->prefix);
    optics_key_push(&key, poll->host);
    optics_key_push(&key, poll->key);

    metrics->data[metrics->len] = (struct metric) {
        .key = strndup(key.data, optics_name_max_len),
        .type = poll->type,
        .value = poll->value,
    };
    metrics->len++;

    return metrics;
}

static int metrics_cmp(const void *a, const void *b)
{
    const struct metric *lhs = a;
    const struct metric *rhs = b;
    return strncmp(lhs->key, rhs->key, optics_name_max_len);
}

static void metrics_sort(struct metrics *metrics)
{
    qsort(metrics->data, metrics->len, sizeof(*metrics->data), metrics_cmp);
}


// -----------------------------------------------------------------------------
// rest
// -----------------------------------------------------------------------------

struct rest
{
    struct optics_poller *poller;

    struct slock lock;
    struct metrics *current;

    struct metrics *build;
};

static void swap_tables(struct rest *rest)
{
    struct metrics *to_delete;
    if (rest->build) metrics_sort(rest->build);

    {
        slock_lock(&rest->lock);

        to_delete = rest->current;
        rest->current = rest->build;

        slock_unlock(&rest->lock);
    }

    metrics_free(to_delete);
    rest->build = NULL;
}

static void write_counter(struct buffer *buffer, const struct metric *metric)
{
    switch (metric->type) {

    case optics_counter:
        buffer_printf(buffer, "\"%s\":%" PRIu64, metric->key, metric->value.counter);
        break;

    case optics_gauge:
        buffer_printf(buffer, "\"%s\":%g", metric->key, metric->value.gauge);
        break;

    case optics_dist:
        buffer_printf(buffer,
                "\"%s\":{\"p50\":%g,\"p90\":%g,\"p99\":%g,\"max\":%g,\"count\":%zu}",
                metric->key,
                metric->value.dist.p50,
                metric->value.dist.p90,
                metric->value.dist.p99,
                metric->value.dist.max,
                metric->value.dist.n);
        break;

    case optics_histo:
    {
        const struct optics_histo *histo = &metric->value.histo;

        buffer_printf(buffer, "\"%s\":{\"below\":%zu,\"above\":%zu",
                metric->key, histo->above, histo->below);

        for (size_t i = 0; i < histo->buckets_len - 1; ++i) {
            buffer_printf(buffer, ",\"bucket_%3g-%3g\":%zu",
                    histo->buckets[i], histo->buckets[i+1], histo->counts[i]);
        }

        buffer_put(buffer, '}');
        break;
    }

    case optics_quantile:
    {
	buffer_printf(buffer, "\"%s\":%g", metric->key, metric->value.quantile);
        break;
    }

    default:
        optics_fail("unknown lens type '%d'", metric->type);
        break;
    }
}


// -----------------------------------------------------------------------------
// callbacks
// -----------------------------------------------------------------------------


static enum crest_result
rest_get(void *ctx, struct crest_req *req, struct crest_resp *resp)
{
    (void) req;
    struct rest *rest = ctx;

    struct buffer buffer = {0};
    buffer_put(&buffer, '{');

    {
        slock_lock(&rest->lock);

        if (rest->current) {
            for (size_t i = 0; i < rest->current->len; ++i) {
                if (i > 0) buffer_put(&buffer, ',');
                write_counter(&buffer, &rest->current->data[i]);
            }
        }

        slock_unlock(&rest->lock);
    }

    buffer_put(&buffer, '}');

    crest_resp_add_header(resp, "content-type", "text/plain");
    crest_resp_write(resp, buffer.data, buffer.len);

    buffer_reset(&buffer);
    return crest_ok;
}

static void rest_dump(
        void *ctx, enum optics_poll_type type, const struct optics_poll *poll)
{
    struct rest *rest = ctx;

    switch (type) {
    case optics_poll_begin: metrics_free(rest->build); rest->build = NULL; break;
    case optics_poll_done: swap_tables(rest); break;
    case optics_poll_metric: rest->build = metrics_append(rest->build, poll); break;
    default:
        optics_warn("unknown poll type '%d'", type);
        break;
    }
}

static void rest_free(void *ctx)
{
    struct rest *rest = ctx;

    if (!slock_try_lock(&rest->lock)) {
        optics_fail("concurrent request during free");
        optics_abort();
    }

    metrics_free(rest->current);
    metrics_free(rest->build);
    free(rest);
}


// -----------------------------------------------------------------------------
// register
// -----------------------------------------------------------------------------

void optics_dump_rest(struct optics_poller *poller, struct crest *crest)
{
    struct rest *rest = calloc(1, sizeof(*rest));
    optics_assert_alloc(rest);

    crest_add(crest, (struct crest_res) {
                .path = "/metrics/json",
                .context = rest,
                .get = rest_get
            });

    optics_poller_backend(poller, rest, &rest_dump, &rest_free);
}
