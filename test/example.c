/* example.c
   Rémi Attab (remi.attab@gmail.com), 21 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "optics.h"

int main(void)
{
    struct optics *optics = optics_create("my_optics");
    optics_set_prefix(optics, "my_prefix");

    {
        struct optics_lens *lens = optics_gauge_alloc(optics, "my_gauge");

        optics_gauge_set(lens, 12.3);

        optics_lens_close(lens);
    }

    {
        struct optics_lens *lens = optics_counter_alloc(optics, "my_counter");

        optics_counter_inc(lens, 1);
        optics_counter_inc(lens, 10);
        optics_counter_inc(lens, -2);

        optics_lens_close(lens);
    }

    {
        struct optics_lens *lens = optics_dist_alloc(optics, "my_distribution");

        optics_dist_record(lens, 12.3);
        optics_dist_record(lens, 23.5);

        optics_lens_close(lens);
    }

    {
        struct optics_lens *lens = optics_lens_get(optics, "my_gauge");
        optics_lens_free(lens);
    }
    
    optics_close(optics);
}
