/* buffer_test.c
   Rémi Attab (remi.attab@gmail.com), 10 Jun 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"

#include "utils/buffer.h"


// -----------------------------------------------------------------------------
// put
// -----------------------------------------------------------------------------

void put_test(void **state)
{
    (void) state;
    struct buffer buffer = {0};

    enum { len = 1024 };
    const char data = 'a';

    for (size_t it = 0; it < 10; ++it) {
        for (size_t i = 0; i < len; ++i) buffer_put(&buffer, data);

        assert_int_equal(buffer.len, len);
        for (size_t i = 0; i < len; ++i) assert_true(buffer.data[i] == data);;

        buffer_reset(&buffer);
    }
}


// -----------------------------------------------------------------------------
// write
// -----------------------------------------------------------------------------

void write_test(void **state)
{
    (void) state;
    struct buffer buffer = {0};

    enum { len = 1024 };
    const char data[] = "abcdefg";

    for (size_t it = 0; it < 10; ++it) {

        for (size_t i = 0; i < len; ++i)
            buffer_write(&buffer, data, sizeof(data));

        assert_int_equal(buffer.len, len * sizeof(data));

        for (size_t i = 0; i < len; ++i) {
            for (size_t j = 0; j < sizeof(data); ++j) {
                assert_true(buffer.data[i * sizeof(data) + j] == data[j]);
            }
        }

        buffer_reset(&buffer);
    }
}


// -----------------------------------------------------------------------------
// printf
// -----------------------------------------------------------------------------

void printf_test(void **state)
{
    (void) state;
    struct buffer buffer = {0};

    enum {len = 1024};
    const char data[] = "abcdefg";
    const size_t data_len = sizeof(data) - 1;

    for (size_t it = 0; it < 10; ++it) {

        for (size_t i = 0; i < len; ++i)
            buffer_printf(&buffer, "%s", data);

        assert_int_equal(buffer.len, len * data_len);

        for (size_t i = 0; i < len; ++i) {
            for (size_t j = 0; j < data_len; ++j) {
                assert_true(buffer.data[i * data_len + j] == data[j]);
            }
        }

        buffer_reset(&buffer);
    }
}


// -----------------------------------------------------------------------------
// printf boundary
// -----------------------------------------------------------------------------

void printf_boundary_test(void **state)
{
    (void) state;
    struct buffer buffer = {0};

    enum { n = 128 }; // must match buffer_min_cap
    char data[n + 1] = {0};

    for (size_t i = 0; i < n; ++i) data[i] = 'a';
    buffer_printf(&buffer, "%s", data);

    for (size_t i = 0; i < n;  ++i) data[i] = 'b';
    buffer_printf(&buffer, "%s", data);

    for (size_t i = 0; i < n;  ++i) data[i] = 'c';
    buffer_printf(&buffer, "%s", data);

    for (size_t i = 0; i < n; ++i)
        assert_true(buffer.data[i] == 'a');

    for (size_t i = 0; i < n; ++i)
        assert_true(buffer.data[i + n] == 'b');

    for (size_t i = 0; i < n; ++i)
        assert_true(buffer.data[i + n + n] == 'c');

    buffer_reset(&buffer);
}


// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(put_test),
        cmocka_unit_test(write_test),
        cmocka_unit_test(printf_test),
        cmocka_unit_test(printf_boundary_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
