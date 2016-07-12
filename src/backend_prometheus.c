/* backend_prometheus.c
   Rémi Attab (remi.attab@gmail.com), 09 Jun 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "optics.h"
#include "utils/errors.h"
#include "utils/lock.h"
#include "utils/htable.h"
#include "utils/buffer.h"
#include "utils/type_pun.h"
#include "utils/crest/crest.h"

#include <string.h>


// -----------------------------------------------------------------------------
// prometheus
// -----------------------------------------------------------------------------

struct metric
{
    enum optics_lens_type type;
    union optics_poll_value value;
    char *host;
};

struct prometheus
{
    struct slock lock;
    struct htable current;

    struct htable build;
};

static void free_table(struct htable *table)
{
    struct htable_bucket *bucket = htable_next(table, NULL);
    for (; bucket; bucket = htable_next(table, bucket)) {
        struct metric *metric = pun_itop(bucket->value);
        free(metric->host);
        free(metric);
    }

    htable_reset(table);
}

static void swap_tables(struct prometheus *prometheus)
{
    struct htable to_delete;

    {
        slock_lock(&prometheus->lock);

        to_delete = prometheus->current;
        prometheus->current = prometheus->build;

        slock_unlock(&prometheus->lock);
    }

    free_table(&to_delete);
    prometheus->build = (struct htable) {0};
}

static int64_t counter_value(struct prometheus *prometheus, const char *key) {
    int64_t result = 0;

    slock_lock(&prometheus->lock);

    struct htable_ret ret = htable_get(&prometheus->current, key);
    if (ret.ok) {
        struct metric *old = pun_itop(ret.value);
        if (old->type == optics_counter) result = old->value.counter;
    }

    slock_unlock(&prometheus->lock);

    return result;

}

static int64_t dist_count_value(struct prometheus *prometheus, const char *key) {
    int64_t result = 0;

    slock_lock(&prometheus->lock);

    struct htable_ret ret = htable_get(&prometheus->current, key);
    if (ret.ok) {
        struct metric *old = pun_itop(ret.value);
        if (old->type == optics_dist) result = old->value.dist.n;
    }

    slock_unlock(&prometheus->lock);

    return result;

}

static bool is_valid_char(char c)
{
    return
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        (c == '_' || c == ':');
}

// Prometheus is very picky about which characters are allowed in a metric
// name. Until we fix the optics interface to better support both carbon and
// prometheus, we need to do some ugly character replacement instead.
//
// Note: prometheus doesn't support unicode characters in keys.
static void fix_key(struct optics_key *key)
{
    for (char *c = key->data; *c != '\0'; ++c) {
        if (!is_valid_char(*c)) *c = '_';
    }
}

static void record(struct prometheus *prometheus, const struct optics_poll *poll)
{
    struct metric *metric = calloc(1, sizeof(*metric));
    optics_assert_alloc(metric);
    metric->type = poll->type;
    metric->value = poll->value;
    metric->host = strndup(poll->host, optics_name_max_len);

    struct optics_key key = {0};
    optics_key_push(&key, poll->prefix);
    optics_key_push(&key, poll->key->data);
    fix_key(&key);

    switch (metric->type) {
    case optics_counter:
        metric->value.counter += counter_value(prometheus, key.data);
        break;
    case optics_dist:
        metric->value.dist.n += dist_count_value(prometheus, key.data);
        break;
    case optics_gauge: default: break;
    }

    struct htable_ret ret = htable_put(&prometheus->build, key.data, pun_ptoi(metric));
    if (!ret.ok) {
        optics_fail("unable to add duplicate key '%s' for host '%s'", key.data, poll->host);
        optics_abort();
    }
}

static void print_metric(struct buffer *buffer, const char *key, const struct metric *metric)
{
    switch (metric->type) {
    case optics_counter:
        buffer_printf(buffer, "# TYPE %s counter\n%s{host=\"%s\"} %ld\n",
                key, key, metric->host, metric->value.counter);
        break;

    case optics_gauge:
        buffer_printf(buffer, "# TYPE %s gauge\n%s{host=\"%s\"} %g\n",
                key, key, metric->host, metric->value.gauge);
        break;

    case optics_dist:
        buffer_printf(buffer,
                "# TYPE %s summary\n"
                "%s{host=\"%s\", quantile=\"0.5\"} %g\n"
                "%s{host=\"%s\", quantile=\"0.9\"} %g\n"
                "%s{host=\"%s\", quantile=\"0.99\"} %g\n"
                "%s_count{host=\"%s\"} %lu\n",
                key,
                key, metric->host, metric->value.dist.p50,
                key, metric->host, metric->value.dist.p90,
                key, metric->host, metric->value.dist.p99,
                key, metric->host, metric->value.dist.n);
        break;

    default:
        optics_fail("unknown lens type '%d'", metric->type);
        optics_abort();
    }
}


// -----------------------------------------------------------------------------
// callbacks
// -----------------------------------------------------------------------------

static enum crest_result
prometheus_get(void *ctx, struct crest_req *req, struct crest_resp *resp)
{
    (void) req;
    struct prometheus *prometheus = ctx;

    struct buffer buffer = {0};
    {
        slock_lock(&prometheus->lock);

        struct htable_bucket *bucket = htable_next(&prometheus->current, NULL);
        for (; bucket; bucket = htable_next(&prometheus->current, bucket)) {
            const struct metric *metric = pun_itop(bucket->value);
            print_metric(&buffer, bucket->key, metric);
        }

        slock_unlock(&prometheus->lock);
    }

    buffer_put(&buffer, '\n');

    crest_resp_add_header(resp, "content-type", "text/plain");
    crest_resp_write(resp, buffer.data, buffer.len);

    buffer_reset(&buffer);
    return crest_ok;
}

static void prometheus_dump(
        void *ctx, enum optics_poll_type type, const struct optics_poll *poll)
{
    struct prometheus *prometheus = ctx;

    switch (type) {
    case optics_poll_begin: free_table(&prometheus->build); break;
    case optics_poll_done: swap_tables(prometheus); break;
    case optics_poll_metric: record(prometheus, poll); break;
    default:
        optics_warn("unknown poll type '%d'", type);
        break;
    }
}

static void prometheus_free(void *ctx)
{
    struct prometheus *prometheus = ctx;

    if (!slock_try_lock(&prometheus->lock)) {
        optics_fail("concurrent request during free");
        optics_abort();
    }

    free_table(&prometheus->current);
    free_table(&prometheus->build);
    free(prometheus);
}


// -----------------------------------------------------------------------------
// register
// -----------------------------------------------------------------------------

void optics_dump_prometheus(struct optics_poller *poller, struct crest *crest)
{
    struct prometheus *prometheus = calloc(1, sizeof(*prometheus));
    optics_assert_alloc(prometheus);

    crest_add(crest, (struct crest_res) {
                .path = "/metrics/prometheus",
                .context = prometheus,
                .get = prometheus_get
            });

    optics_poller_backend(poller, prometheus, &prometheus_dump, &prometheus_free);
}
