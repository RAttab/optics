/* backend_prometheus_test.c
   Rémi Attab (remi.attab@gmail.com), 10 Jun 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"
#include "utils/crest/crest.h"


// -----------------------------------------------------------------------------
// basics
// -----------------------------------------------------------------------------

optics_test_head(backend_prometheus_with_source_test)
{
    enum { port = 64123 };
    const char *path = "/metrics/prometheus";

    struct optics *optics = optics_create(test_name);
    optics_set_prefix(optics, ".-optics");
    optics_set_source(optics, ".-source");

    struct optics_lens *counter = optics_counter_alloc(optics, "counter");
    struct optics_lens *gauge = optics_gauge_alloc(optics, "gauge");
    struct optics_lens *dist = optics_dist_alloc(optics, "dist");
    const double buckets[] = {1, 2, 3};
    struct optics_lens *histo = optics_histo_alloc(optics, "histo", buckets, 3);

    struct crest *crest = crest_new();
    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_set_host(poller, "host");
    optics_dump_prometheus(poller, crest);
    crest_bind(crest, port);

    for (size_t it = 0; it < 10; ++it) {

        optics_counter_inc(counter, 1);
        optics_gauge_set(gauge, 1.0);
        for (size_t i = 0; i < 100; ++i) optics_dist_record(dist, i);
        for (size_t i = 0; i < 80; ++i) optics_histo_inc(histo, i % 4);
        if (!optics_poller_poll(poller)) optics_abort();

        char body[4096];
        snprintf(body, sizeof(body),
                "# TYPE __optics_dist summary\n"
                "__optics_dist{source=\".-source\",quantile=\"0.5\"} 50\n"
                "__optics_dist{source=\".-source\",quantile=\"0.9\"} 90\n"
                "__optics_dist{source=\".-source\",quantile=\"0.99\"} 99\n"
                "__optics_dist_count{source=\".-source\"} %lu\n"
                "# TYPE __optics_gauge gauge\n"
                "__optics_gauge{source=\".-source\"} 1\n"
                "# TYPE __optics_histo histogram\n"
                "__optics_histo_bucket{source=\".-source\",le=\"1\"} %lu\n"
                "__optics_histo_bucket{source=\".-source\",le=\"+Inf\"} %lu\n"
                "__optics_histo_bucket{source=\".-source\",le=\"2\"} %lu\n"
                "__optics_histo_bucket{source=\".-source\",le=\"3\"} %lu\n"
                "__optics_histo_count{source=\".-source\"} %lu\n"
                "# TYPE __optics_counter counter\n"
                "__optics_counter{source=\".-source\"} %lu\n"
                "\n",
                100*(it+1), // dist
                20* (it+1), 20 * (it+ 1), 20 * (it+1), 20 * (it+1), 80 * (it+1), // histo
                (it+1)); // counter

        assert_http_body(port, "GET", path, 200, body);
    }

    crest_free(crest);
    optics_poller_free(poller);
    optics_lens_close(dist);
    optics_lens_close(gauge);
    optics_lens_close(counter);
    optics_close(optics);
}
optics_test_tail()

optics_test_head(backend_prometheus_without_source_test)
{
    enum { port = 64123 };
    const char *path = "/metrics/prometheus";

    struct optics *optics = optics_create(test_name);
    optics_set_prefix(optics, ".-optics");

    struct optics_lens *counter = optics_counter_alloc(optics, "counter");
    struct optics_lens *gauge = optics_gauge_alloc(optics, "gauge");
    struct optics_lens *dist = optics_dist_alloc(optics, "dist");
    const double buckets[] = {1, 2, 3};
    struct optics_lens *histo = optics_histo_alloc(optics, "histo", buckets, 3);

    struct crest *crest = crest_new();
    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_set_host(poller, "host");
    optics_dump_prometheus(poller, crest);
    crest_bind(crest, port);

    for (size_t it = 0; it < 10; ++it) {

        optics_counter_inc(counter, 1);
        optics_gauge_set(gauge, 1.0);
        for (size_t i = 0; i < 100; ++i) optics_dist_record(dist, i);
        for (size_t i = 0; i < 80; ++i) optics_histo_inc(histo, i % 4);
        if (!optics_poller_poll(poller)) optics_abort();

        char body[4096];
        snprintf(body, sizeof(body),
                "# TYPE __optics_counter counter\n"
                "__optics_counter %lu\n"
                "# TYPE __optics_dist summary\n"
                "__optics_dist{quantile=\"0.5\"} 50\n"
                "__optics_dist{quantile=\"0.9\"} 90\n"
                "__optics_dist{quantile=\"0.99\"} 99\n"
                "__optics_dist_count %lu\n"
                "# TYPE __optics_histo histogram\n"
                "__optics_histo_bucket{le=\"1\"} %lu\n"
                "__optics_histo_bucket{le=\"+Inf\"} %lu\n"
                "__optics_histo_bucket{le=\"2\"} %lu\n"
                "__optics_histo_bucket{le=\"3\"} %lu\n"
                "__optics_histo_count %lu\n"
                "# TYPE __optics_gauge gauge\n"
                "__optics_gauge 1\n"
                "\n",
                (it+1), // counters
                100*(it+1), // dist
                20*(it+1), 20*(it+1), 20*(it+1), 20*(it+1), 80*(it+1) //histo
            );

        assert_http_body(port, "GET", path, 200, body);
    }

    crest_free(crest);
    optics_poller_free(poller);
    optics_lens_close(dist);
    optics_lens_close(gauge);
    optics_lens_close(counter);
    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(backend_prometheus_with_source_test),
        cmocka_unit_test(backend_prometheus_without_source_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
