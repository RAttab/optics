/* example.c
   Rémi Attab (remi.attab@gmail.com), 21 Mar 2016
   Marina C., 23 February 2018
   FreeBSD-style copyright and disclaimer apply
*/

#include "optics.h"

int main(void)
{
    // Creates a new optics region which will be made immediately available for
    // polling by the optics daemon. The provided sttring will be the name of
    // the region and should be globally unique (eg. the process pid).
    struct optics *optics = optics_create("my_optics");

    // Acts as the prefix for all keys.
    optics_set_prefix(optics, "my_prefix");

    // If multiple instances of a service are running on the same host, then a
    // source identifier must be provided to avoid collisions on the recorded
    // metrics. Otherwise, this option is optional.
    optics_set_source(optics, "my_source");

    // Gauges will emit the same value until it is changed or removed.
    {
        struct optics_lens *lens = optics_gauge_alloc(optics, "my_gauge");

        optics_gauge_set(lens, 12.3);

        optics_lens_close(lens);
    }

    // optics_lens_free removes the lens entirely.
    {
        struct optics_lens *lens = optics_lens_get(optics, "my_gauge");
        optics_lens_free(lens);
    }

    // Counters are used to calculate the rate of events per second.
    {
        struct optics_lens *lens = optics_counter_alloc(optics, "my_counter");

        optics_counter_inc(lens, 1);
        optics_counter_inc(lens, 10);
        optics_counter_inc(lens, -2);

        optics_lens_close(lens);
    }

    // the streaming quantile lens is good for estimating the desired quantile value.
    {
        struct optics_lens *lens = optics_quantile_alloc(optics, "my_quantile", 0.90, 50, 0.05);

        for(int i = 0; i < 1000; i++){
            for (int j = 0; j < 100; j++){
                optics_quantile_update(lens, j);
            }
        }

	// shouldn't there be a check here to read it / show how to read it
	// and what it could be used for?
	optics_lens_close(lens);
    }


    // Distributions are used the calculate quantile approximations over a set
    // of recorded values.
    {
        struct optics_lens *lens = optics_dist_alloc(optics, "my_distribution");

        optics_dist_record(lens, 12.3);
        optics_dist_record(lens, 23.5);

        optics_lens_close(lens);
    }

    // optics_timer_start and optics_timer_elapsed can be used to record the
    // latency of operations. The second argument to optics_timer_elapsed
    // represents the scale of the recorded values.
    //
    // Note that the accuracy of the timer may suffer if the recorded latency
    // falls below a micro-second.
    {
        struct optics_lens *lens = optics_lens_get(optics, "my_distribution");

        optics_timer_t t0;
        optics_timer_start(&t0);

        // ...

        optics_dist_record(lens, optics_timer_elapsed(&t0, optics_usec));

        optics_lens_close(lens);
    }

    // optics_key can be used to facilitate the construction of complex keys.
    {
        struct optics_key key = {0};
        optics_key_push(&key, "foo");

        // foo.bar
        size_t old = optics_key_push(&key, "bar");
        struct optics_lens *bar = optics_gauge_alloc(optics, key.data);
        optics_key_pop(&key, old);

        // foo.baz
        optics_key_push(&key, "baz");
        struct optics_lens *baz = optics_gauge_alloc(optics, key.data);

        optics_lens_close(bar);
        optics_lens_close(baz);
    }
    
    optics_close(optics);
}
