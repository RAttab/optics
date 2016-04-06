/* backend_carbon_test.c
   Rémi Attab (remi.attab@gmail.com), 09 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"

#include "utils/socket.h"
#include "utils/time.h"


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


// -----------------------------------------------------------------------------
// external
// -----------------------------------------------------------------------------

optics_test_head(external_test)
{

    // When turned on the test will take longer so you can have fun starting and
    // stopping carbon. Leave off by default.
    enum { test_disconnects = true };

    if (!test_disconnects) assert_carbon();

    struct optics *optics = optics_create(test_name);
    optics_set_prefix(optics, "optics.tests");

    struct optics_lens *lens = optics_dist_alloc(optics, "blah");

    struct optics_poller *poller = optics_poller_alloc();
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
        cmocka_unit_test(external_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
