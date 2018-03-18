/* backend_carbon_test.c
   Rémi Attab (remi.attab@gmail.com), 09 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"

#include "utils/time.h"
#include "utils/socket.h"
#include "utils/htable.h"
#include "utils/buffer.h"

#include <unistd.h>
#include <pthread.h>


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

static const char *carbon_host = "127.0.0.1";
static const char *carbon_port = "2003";

void assert_carbon()
{
    if (socket_stream_connect(carbon_host, carbon_port) > 0) return;

    printf("carbon host is not present\n");
    skip();
}

struct carbon
{
    int listen_fd;
    pthread_t thread;

    struct buffer buffer;
};

struct carbon * carbon_start(const char *port);
void carbon_stop(struct carbon *carbon);
void carbon_parse(struct carbon *carbon, struct htable *table);


// -----------------------------------------------------------------------------
// internal
// -----------------------------------------------------------------------------

optics_test_head(backend_carbon_internal_with_source_test)
{
    const char *port = "12345";
    struct carbon *carbon = carbon_start(port);

    struct optics *optics = optics_create(test_name);
    optics_set_prefix(optics, "prefix");
    optics_set_source(optics, "source");

    struct optics_lens *counter = optics_counter_alloc(optics, "counter");
    struct optics_lens *gauge = optics_gauge_alloc(optics, "gauge");
    struct optics_lens *dist = optics_dist_alloc(optics, "dist");
    const double buckets[] = {1, 2, 3};
    struct optics_lens *histo = optics_histo_alloc(optics, "histo", buckets, 3);
    struct optics_lens *quantile = optics_quantile_alloc(optics, "quantile", 0.9, 50, 0.05);

    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_set_host(poller, "host");
    optics_dump_carbon(poller, "127.0.0.1", port);

    for (size_t it = 0; it < 10; ++it) {
        optics_counter_inc(counter, 1);
        optics_gauge_set(gauge, 1.0);
        for (size_t i = 0; i < 100; ++i) optics_dist_record(dist, i);
        for (size_t i = 0; i < 100; ++i) optics_histo_inc(histo, i % 5);

        if (!optics_poller_poll(poller)) optics_abort();

        // sketchy way to wait for carbon to stop reading our input so we can
        // read the result without issues.
        nsleep(1 * 1000 * 1000);

        struct htable result = {0};
        carbon_parse(carbon, &result);
        assert_htable_equal(&result, 0,
                make_kv("prefix.host.source.counter", 1),
                make_kv("prefix.host.source.gauge", 1),
                make_kv("prefix.host.source.dist.p50", 50),
                make_kv("prefix.host.source.dist.p90", 90),
                make_kv("prefix.host.source.dist.p99", 99),
                make_kv("prefix.host.source.dist.max", 99),
                make_kv("prefix.host.source.dist.count", 100),
                make_kv("prefix.host.source.histo.bucket_1_2", 20),
                make_kv("prefix.host.source.histo.bucket_2_3", 20),
                make_kv("prefix.host.source.histo.below", 20),
                make_kv("prefix.host.source.histo.above", 40),
                make_kv("prefix.host.source.quantile", 50));

        htable_reset(&result);
    }

    optics_poller_free(poller);
    optics_lens_close(counter);
    optics_lens_close(gauge);
    optics_lens_close(dist);
    optics_lens_close(histo);
    optics_lens_close(quantile);
    optics_close(optics);
    carbon_stop(carbon);
}
optics_test_tail()


optics_test_head(backend_carbon_internal_without_source_test)
{
    const char *port = "12345";
    struct carbon *carbon = carbon_start(port);

    struct optics *optics = optics_create(test_name);
    optics_set_prefix(optics, "prefix");

    struct optics_lens *counter = optics_counter_alloc(optics, "counter");
    struct optics_lens *gauge = optics_gauge_alloc(optics, "gauge");
    struct optics_lens *dist = optics_dist_alloc(optics, "dist");
    const double buckets[] = {1, 2, 3};
    struct optics_lens *histo = optics_histo_alloc(optics, "histo", buckets, 3);
    struct optics_lens *quantile = optics_quantile_alloc(optics, "quantile", 0.9, 50, 0);

    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_set_host(poller, "host");
    optics_dump_carbon(poller, "127.0.0.1", port);

    for (size_t it = 0; it < 10; ++it) {
        optics_counter_inc(counter, 1);
        optics_gauge_set(gauge, 1.0);
        for (size_t i = 0; i < 100; ++i) optics_dist_record(dist, i);
        for (size_t i = 0; i < 100; ++i) optics_histo_inc(histo, i % 5);
        for (int i = 0; i < 100; ++i){
            optics_quantile_update(quantile, i); 
        }

        if (!optics_poller_poll(poller)) optics_abort();

        // sketchy way to wait for carbon to stop reading our input so we can
        // read the result without issues.
        nsleep(1 * 1000 * 1000);

        struct htable result = {0};
        carbon_parse(carbon, &result);
        assert_htable_equal(&result, 0,
                make_kv("prefix.host.counter", 1),
                make_kv("prefix.host.gauge", 1),
                make_kv("prefix.host.dist.p50", 50),
                make_kv("prefix.host.dist.p90", 90),
                make_kv("prefix.host.dist.p99", 99),
                make_kv("prefix.host.dist.max", 99),
                make_kv("prefix.host.dist.count", 100),
                make_kv("prefix.host.histo.bucket_1_2", 20),
                make_kv("prefix.host.histo.bucket_2_3", 20),
                make_kv("prefix.host.histo.below", 20),
                make_kv("prefix.host.histo.above", 40),
                make_kv("prefix.host.quantile", 90));

        htable_reset(&result);
    }

    optics_poller_free(poller);
    optics_lens_close(counter);
    optics_lens_close(gauge);
    optics_lens_close(dist);
    optics_lens_close(histo);
    optics_lens_close(quantile);
    optics_close(optics);
    carbon_stop(carbon);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// external
// -----------------------------------------------------------------------------

optics_test_head(backend_carbon_external_with_source_test)
{

    // When turned on the test will take longer so you can have fun starting and
    // stopping carbon. Leave off by default.
    enum { test_disconnects = false };

    if (!test_disconnects) assert_carbon();

    struct optics *optics = optics_create(test_name);
    optics_set_prefix(optics, "prefix");
    optics_set_source(optics, "source");

    struct optics_lens *lens = optics_dist_alloc(optics, "dist");

    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_set_host(poller, "host");
    optics_dump_carbon(poller, "127.0.0.1", "2003");

    size_t iterations = test_disconnects ? 10 * 1000 : 100;

    for (size_t t = 0; t < iterations; ++t) {
        for (size_t i = 0; i < 10; ++i)
            optics_dist_record(lens, i);

        if (!optics_poller_poll(poller)) optics_abort();

        if (test_disconnects) nsleep(1000000);
    }

    optics_poller_free(poller);
    optics_lens_close(lens);
    optics_close(optics);
}
optics_test_tail()


optics_test_head(backend_carbon_external_without_source_test)
{

    // When turned on the test will take longer so you can have fun starting and
    // stopping carbon. Leave off by default.
    enum { test_disconnects = false };

    if (!test_disconnects) assert_carbon();

    struct optics *optics = optics_create(test_name);
    optics_set_prefix(optics, "prefix");

    struct optics_lens *lens = optics_dist_alloc(optics, "blah");

    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_set_host(poller, "host");
    optics_dump_carbon(poller, "127.0.0.1", "2003");

    size_t iterations = test_disconnects ? 10 * 1000 : 100;

    for (size_t t = 0; t < iterations; ++t) {
        for (size_t i = 0; i < 10; ++i)
            optics_dist_record(lens, i);

        if (!optics_poller_poll(poller)) optics_abort();

        if (test_disconnects) nsleep(1000000);
    }

    optics_poller_free(poller);
    optics_lens_close(lens);
    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(backend_carbon_internal_with_source_test),
        cmocka_unit_test(backend_carbon_internal_without_source_test),
        cmocka_unit_test(backend_carbon_external_with_source_test),
        cmocka_unit_test(backend_carbon_external_without_source_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}


// -----------------------------------------------------------------------------
// carbon
// -----------------------------------------------------------------------------

void * carbon_run(void *ctx)
{
    struct carbon *carbon = ctx;

    int fd = socket_stream_accept(carbon->listen_fd);
    if (fd < 0) optics_abort();

    ssize_t len;
    char buffer[8192];

    while ((len = socket_recv(fd, sizeof(buffer), buffer)) >= 0)
        buffer_write(&carbon->buffer, buffer, len);

    close(fd);
    return NULL;
}

void carbon_parse(struct carbon *carbon, struct htable *table)
{
    carbon->buffer.data[carbon->buffer.len] = '\0';

    char *saveptr = NULL, *line = NULL;
    line = strtok_r(carbon->buffer.data, "\n", &saveptr);

    while (line) {
        char key[optics_name_max_len];
        double value;
        optics_ts_t ts;
        sscanf(line, "%[^ ] %lf %lu", key, &value, &ts);

        if (!htable_put(table, key, pun_dtoi(value)).ok) {
            optics_fail("duplicate key detected '%s'", key);
            optics_abort();
        }

        line = strtok_r(NULL, "\n", &saveptr);
    }

    buffer_reset(&carbon->buffer);
}

struct carbon * carbon_start(const char *port)
{
    struct carbon *carbon = calloc(1, sizeof(*carbon));

    carbon->listen_fd = socket_stream_listen(port);
    if (carbon->listen_fd < 0) optics_abort();


    int err = pthread_create(&carbon->thread, NULL, &carbon_run, carbon);
    if (err) {
        optics_fail_ierrno(err, "unable to start carbon thread");
        optics_abort();
    }

    return carbon;
}

void carbon_stop(struct carbon *carbon)
{
    close(carbon->listen_fd);

    void *ret = NULL;
    int err = pthread_join(carbon->thread, &ret);
    if (err) {
        optics_fail_ierrno(err, "unable to join carbon thread");
        optics_abort();
    }

    free(carbon);
}
