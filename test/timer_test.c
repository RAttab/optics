/* timer_test.c
   Rémi Attab (remi.attab@gmail.com), 07 Apr 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"
#include "utils/time.h"


// -----------------------------------------------------------------------------
// basics
// -----------------------------------------------------------------------------

#define assert_timer(duration, scale, exp, eps)         \
    do {                                                \
        optics_timer_t t0;                              \
        optics_timer_start(&t0);                        \
                                                        \
        nsleep(duration);                               \
                                                        \
        double diff = optics_timer_elapsed(&t0, scale); \
        assert_float_equal(diff, exp, eps);             \
    } while (false)


optics_test_head(basics_test)
{
    // unfortunately, nsleep is really innacurate a lower resolution.
    const size_t sleep = 1000 * 1000;
    assert_timer(sleep, optics_sec, 1e-3, 2e-4);
    assert_timer(sleep, optics_msec, 1.0, 0.2);
    assert_timer(sleep, optics_usec, 1e3, 2e2);
    assert_timer(sleep, optics_nsec, 1e6, 2e5);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// main
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(basics_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
