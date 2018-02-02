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

#include <stdio.h>
#include <string.h>
#include <bsd/string.h>


// -----------------------------------------------------------------------------
// prometheus
// -----------------------------------------------------------------------------

struct metric
{
    enum optics_lens_type type;
    union optics_poll_value value;

    struct optics_key key;
    char source[optics_name_max_len];
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
    for (; bucket; bucket = htable_next(table, bucket))
        free(pun_itop(bucket->value));

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

static void merge_counter(const int64_t *old, int64_t *new)
{
    *new += *old;
}

static void merge_dist(const struct optics_dist *old, struct optics_dist *new)
{
    new->n += old->n;
}

static void merge_histo(const struct optics_histo *old, struct optics_histo *new)
{
    if (new->buckets_len != old->buckets_len) return;

    for (size_t i = 0; i < new->buckets_len; ++i) {
        if (new->buckets[i] != old->buckets[i]) return;
    }

    new->above += old->above;
    new->below += old->below;
    for (size_t i = 0; i < new->buckets_len; ++i)
        new->counts[i] += old->counts[i];
}

static void merge_metric(
        struct prometheus *prometheus, const char *key, struct metric *metric)
{
    slock_lock(&prometheus->lock);

    struct htable_ret ret = htable_get(&prometheus->current, key);
    if (!ret.ok) goto exit;

    const struct metric *old = pun_itop(ret.value);
    if (old->type != metric->type) goto exit;

    switch (metric->type) {
    case optics_dist: merge_dist(&old->value.dist, &metric->value.dist); break;
    case optics_histo: merge_histo(&old->value.histo, &metric->value.histo); break;
    case optics_counter: merge_counter(&old->value.counter, &metric->value.counter); break;
    case optics_gauge: break;
    case optics_quantile: break;
    default: break;
    }

  exit:
    slock_unlock(&prometheus->lock);
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


static struct metric * make_metric(const struct optics_poll *poll)
{
    struct metric *metric = calloc(1, sizeof(*metric));
    optics_assert_alloc(metric);

    metric->type = poll->type;
    metric->value = poll->value;
    if (poll->source) strlcpy(metric->source, poll->source, optics_name_max_len);

    optics_key_push(&metric->key, poll->prefix);
    optics_key_push(&metric->key, poll->key->data);
    fix_key(&metric->key);

    return metric;
}

// To avoid collisions, the htable key must include the source. Since our
// hash-table only supports strings, we need to build a new key with the source
// in it.
static void htable_key(struct optics_key *key, const struct optics_poll *poll)
{
    optics_key_push(key, poll->prefix);
    optics_key_push(key, poll->key->data);
    if (poll->source) optics_key_push(key, poll->source);
}

static void record(struct prometheus *prometheus, const struct optics_poll *poll)
{
    struct optics_key key = {0};
    htable_key(&key, poll);

    struct metric *metric = make_metric(poll);
    merge_metric(prometheus, key.data, metric);

    struct htable_ret ret = htable_put(&prometheus->build, key.data, pun_ptoi(metric));
    if (!ret.ok) {
        optics_fail("unable to add duplicate key '%s' for host '%s'", key.data, poll->host);
        optics_abort();
    }
}

static void print_metric(struct buffer *buffer, const char *key, const struct metric *metric)
{
    (void) key; // htable_key for why we ignore this param.

    char source_solo[optics_name_max_len + sizeof("{source=\"\"}")] = { [0] = '\0' };
    char source_first[optics_name_max_len + sizeof("source=\"\",")] = { [0] = '\0' };

    if (metric->source[0] != '\0') {
        snprintf(source_solo, sizeof(source_solo), "{source=\"%s\"}", metric->source);
        snprintf(source_first, sizeof(source_first), "source=\"%s\",", metric->source);
    }

    switch (metric->type) {
    case optics_counter:
        buffer_printf(buffer, "# TYPE %s counter\n%s%s %ld\n",
                metric->key.data, metric->key.data, source_solo, metric->value.counter);
        break;
    case optics_quantile:
	break;
    case optics_gauge:
        buffer_printf(buffer, "# TYPE %s gauge\n%s%s %g\n",
                metric->key.data, metric->key.data, source_solo, metric->value.gauge);
        break;

    case optics_dist:
        buffer_printf(buffer,
                "# TYPE %s summary\n"
                "%s{%squantile=\"0.5\"} %g\n"
                "%s{%squantile=\"0.9\"} %g\n"
                "%s{%squantile=\"0.99\"} %g\n"
                "%s_count%s %lu\n",
                metric->key.data,
                metric->key.data, source_first, metric->value.dist.p50,
                metric->key.data, source_first, metric->value.dist.p90,
                metric->key.data, source_first, metric->value.dist.p99,
                metric->key.data, source_solo, metric->value.dist.n);
        break;

    case optics_histo:
    {
        const struct optics_histo *histo = &metric->value.histo;

        buffer_printf(buffer,
                "# TYPE %s histogram\n"
                "%s_bucket{%sle=\"%.3g\"} %zu\n"
                "%s_bucket{%sle=\"+Inf\"} %zu\n",
                metric->key.data,
                metric->key.data, source_first, histo->buckets[0], histo->below,
                metric->key.data, source_first, histo->above);

        size_t count = histo->below + histo->above;
        for (size_t i = 1; i < histo->buckets_len; ++i) {
            buffer_printf(buffer, "%s_bucket{%sle=\"%.3g\"} %zu\n",
                    metric->key.data, source_first, histo->buckets[i], histo->counts[i - 1]);
            count += histo->counts[i - 1];
        }

        buffer_printf(buffer, "%s_count%s %zu\n",
                metric->key.data, source_solo, count);
        break;
    }

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
