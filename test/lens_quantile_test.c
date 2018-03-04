/* lens_gauge_test.c
   Marina C., January 22, 2018
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"

// -----------------------------------------------------------------------------
// open/close
// -----------------------------------------------------------------------------

optics_test_head(lens_quantile_open_close_test)
{
    struct optics *optics = optics_create(test_name);
    const char *lens_name = "my_quantile";

    for (size_t i = 0; i < 3; ++i) {
        struct optics_lens *lens = optics_quantile_alloc(optics, lens_name, 0.99, 0, 0.05);
        if (!lens) optics_abort();

        assert_int_equal(optics_lens_type(lens), optics_quantile);
        assert_string_equal(optics_lens_name(lens), lens_name);

        assert_null(optics_quantile_alloc(optics, lens_name, 0.99, 0, 0.05));
        optics_lens_close(lens);
        assert_null(optics_quantile_alloc(optics, lens_name, 0.99, 0, 0.05));

        assert_non_null(lens = optics_lens_get(optics, lens_name));
        optics_lens_free(lens);
    }

    optics_close(optics);
}
optics_test_tail()

// -----------------------------------------------------------------------------
// update ST
// -----------------------------------------------------------------------------

optics_test_head(lens_quantile_update_read_test)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_quantile_alloc(optics, "my_quantile", 0.90, 50, 0.05);

    optics_epoch_t epoch = optics_epoch(optics);
    
    for(int i = 0; i < 1000; i++){
        for (int j = 0; j < 100; j++){
            optics_quantile_update(lens, j);
        }
    }
    
    double value = 0;
    assert_int_equal(optics_quantile_read(lens, epoch, &value), optics_ok);
    assert_float_equal(value, 90, 1);

    optics_lens_close(lens);
    optics_close(optics);
}
optics_test_tail()

// -----------------------------------------------------------------------------
// update MT
// -----------------------------------------------------------------------------

struct mt_test
{
    struct optics *optics;
    struct optics_lens *lens;
    size_t workers;

    atomic_size_t done;
};

double mt_test_read_lens(struct mt_test *test)
{
    optics_epoch_t epoch = optics_epoch(test->optics);

    double value;
    assert_int_equal(optics_quantile_read(test->lens, epoch, &value), optics_ok); // why does compiler want this to be int when its enum?

    return value;
}

void run_mt_test(size_t id, void *ctx)
{
    struct mt_test *test = ctx;
    enum { iterations = 10 * 10 };

    if (id) { 
        for (size_t i = 0; i < iterations; ++i){
            for (size_t j = 0; j < 100; ++j)
                optics_quantile_update(test->lens, j);
	}

	atomic_fetch_add_explicit(&test->done, 1, memory_order_release); 
    }
    else { 
        size_t done;
        double result = 0;
        size_t writers = test->workers - 1;

        do {
            done = atomic_load_explicit(&test->done, memory_order_acquire);
        } while (done < writers);
	
        result = mt_test_read_lens(test);

	//makes compiler angry:
	// optics_assert(assert_float_equal(result, 90, 1), "%g too far from 90", result);
	// is there a better way? I'd like to keep the error msg somehow
        assert_float_equal(result, 90, 1);
    }
}

optics_test_head(lens_quantile_update_read_mt_test)
{
    assert_mt();
    struct optics *optics = optics_create(test_name);
    struct optics_lens *lens = optics_quantile_alloc(optics, "my_quantile", 0.90, 50, 0.05);

    struct mt_test data = {
        .optics = optics,
        .lens = lens,
        .workers = cpus(),
    };
    run_threads(run_mt_test, &data, data.workers);

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
        cmocka_unit_test(lens_quantile_open_close_test),
        cmocka_unit_test(lens_quantile_update_read_test),
        cmocka_unit_test(lens_quantile_update_read_mt_test)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
