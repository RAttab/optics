/* region_test.c
   Rémi Attab (remi.attab@gmail.com), 03 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"
#include "utils/time.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>


// -----------------------------------------------------------------------------
// open/close
// -----------------------------------------------------------------------------

optics_test_head(region_open_close_test)
{
    for (size_t i = 0; i < 3; ++i) {
        assert_null(optics_open(test_name));

        struct optics *writer = optics_create(test_name);
        if (!writer) optics_abort();

        struct optics *reader = optics_open(test_name);
        if (!reader) optics_abort();

        optics_close(reader);
        optics_close(writer);
    }
}
optics_test_tail()


// -----------------------------------------------------------------------------
// header
// -----------------------------------------------------------------------------

static void region_write_header(const char *name, size_t offset)
{
    char shm[256];
    snprintf(shm, sizeof(shm), "optics.%s", name);

    int fd = shm_open(shm, O_RDWR, 0);
    if (fd < 0) {
        optics_fail_errno("unable to open shm '%s'", shm);
        optics_abort();
    }

    const uint8_t value = 0xFF;
    ssize_t ret = pwrite(fd, &value, sizeof(value), offset);
    if (ret < 0) {
        optics_fail_errno("unable to write header at offset '%zu'", offset);
        optics_abort();
    }

    if (ret != sizeof(value)) {
        optics_fail("unable to write header at offset '%zu': %zd != %zu",
                offset, ret, sizeof(value));
        optics_abort();
    }

    close(fd);
}

optics_test_head(region_header_test)
{
    for (size_t i = 0; i < sizeof(uint64_t) * 2; ++i) {
        struct optics *optics = optics_create(test_name);

        region_write_header(test_name, i);
        assert_null(optics_open(test_name));

        optics_close(optics);
    }
}
optics_test_tail()


// -----------------------------------------------------------------------------
// unlink
// -----------------------------------------------------------------------------

void optics_crash(void) {}

optics_test_head(region_unlink_test)
{
    assert_false(optics_unlink(test_name));

    struct optics *o1;
    assert_non_null(o1 = optics_create(test_name));

    optics_crash();

    struct optics *o2;
    assert_non_null(o2 = optics_create(test_name));

    optics_crash();
    assert_true(optics_unlink(test_name));

    assert_null(optics_open(test_name));

    optics_close(o2);
    optics_close(o1);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// prefix
// -----------------------------------------------------------------------------

optics_test_head(region_prefix_test)
{
    struct optics *writer = optics_create(test_name);
    assert_string_equal(optics_get_prefix(writer), test_name);

    const char *new_prefix = "blah";
    optics_set_prefix(writer, new_prefix);;
    assert_string_equal(optics_get_prefix(writer), new_prefix);

    struct optics *reader = optics_open(test_name);
    assert_string_equal(optics_get_prefix(reader), new_prefix);

    optics_close(reader);
    optics_close(writer);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// epoch
// -----------------------------------------------------------------------------

optics_test_head(region_epoch_test)
{
    struct optics *writer = optics_create(test_name);
    struct optics *reader = optics_open(test_name);

    for (size_t i = 0; i < 5; ++i) {
        optics_epoch_t epoch = optics_epoch(writer);
        assert_false(epoch & ~1);
        assert_int_equal(optics_epoch(writer), epoch);

        assert_int_equal(optics_epoch_inc(reader), epoch);
        assert_int_not_equal(epoch, optics_epoch(writer));
    }

    optics_close(reader);
    optics_close(writer);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// alloc st
// -----------------------------------------------------------------------------

enum optics_ret check_lens_cb(void *ctx, struct optics_lens *lens)
{
    struct optics *optics = ctx;

    struct optics_dist dist = {0};
    optics_dist_read(lens, optics_epoch(optics), &dist);
    optics_dist_record(lens, 1);

    return optics_ok;
}

optics_test_head(region_alloc_st_test)
{
    struct optics *optics = optics_create(test_name);

    enum { n = 1000 };
    struct optics_lens *lens[n];

    for (size_t attempt = 0; attempt < 5; ++attempt) {

        for (size_t i = 0; i < n; ++i) {
            char key[128];
            snprintf(key, sizeof(key), "key-%lu", i);

            lens[i] = optics_dist_alloc(optics, key);
            if (!lens[i]) optics_abort();
        }

        for (size_t i = 0; i < n; ++i)
            assert_true(optics_dist_record(lens[i], i));

        optics_epoch_t epoch = optics_epoch(optics);
        for (size_t i = 0; i < n; ++i) {
            struct optics_dist value = {0};
            assert_int_equal(optics_dist_read(lens[i], epoch, &value), optics_ok);
            assert_int_equal(value.n, 1);
            assert_int_equal(value.max, i);
        }

        assert_int_equal(optics_foreach_lens(optics, optics, check_lens_cb), optics_ok);

        for (size_t i = 0; i < n; ++i)
            assert_true(optics_lens_free(lens[i]));

        optics_epoch_inc(optics);
    }

    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// alloc mt
// -----------------------------------------------------------------------------

struct alloc_test
{
    struct optics *optics;
    atomic_uint_fast8_t done;
};

void run_alloc_test(size_t id, void *ctx)
{
    struct alloc_test *data = ctx;

    if (!id) {
        for (size_t run = 0; run < 10; ++run) {
            (void) optics_epoch_inc(data->optics);

            // Without this the id == 0 thread had a tendency to finish before
            // any other thread came up online (ie. there's nothing to do until
            // the other threads start working).
            nsleep(1 * 1000);

            if (optics_foreach_lens(data->optics, data->optics, check_lens_cb) == optics_err)
                optics_abort();
        }

        atomic_store(&data->done, true);
    }
    else {
        enum { n = 1000 };
        struct optics_lens *lens[n];

        while (!atomic_load(&data->done)) {
            for (size_t i = 0; i < n; ++i) {
                char key[128];
                snprintf(key, sizeof(key), "key-%lu-%lu", id, i);

                lens[i] = optics_dist_alloc(data->optics, key);
                if (!lens[i]) optics_abort();
            }

            for (size_t i = 0; i < n; ++i)
                if (!optics_dist_record(lens[i], i)) optics_abort();

            for (size_t i = 0; i < n; ++i)
                if (!optics_lens_free(lens[i])) optics_abort();
        }
    }
}

optics_test_head(region_alloc_mt_test)
{
    assert_mt();
    struct optics *optics = optics_create(test_name);

    struct alloc_test data = { .optics = optics };
    run_threads(run_alloc_test, &data, 0);

    optics_close(optics);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(region_open_close_test),
        cmocka_unit_test(region_header_test),
        cmocka_unit_test(region_unlink_test),
        cmocka_unit_test(region_prefix_test),
        cmocka_unit_test(region_epoch_test),
        cmocka_unit_test(region_alloc_st_test),
        cmocka_unit_test(region_alloc_mt_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
