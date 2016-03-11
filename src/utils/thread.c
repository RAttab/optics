/* thread.c
   Rémi Attab (remi.attab@gmail.com), 20 Sep 2015
   FreeBSD-style copyright and disclaimer apply
*/

// -----------------------------------------------------------------------------
// cpus
// -----------------------------------------------------------------------------

size_t cpus()
{
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count != -1) return count;

    optics_fail_errno("unable to call sysconf to get cpu count");
    optics_abort();
}


// -----------------------------------------------------------------------------
// tid
// -----------------------------------------------------------------------------

static atomic_size_t tid_counter = 1;
static __thread size_t tid_store = 0;

size_t tid()
{
    if (!tid_store)
        tid_store = atomic_fetch_add_explicit(&tid_counter, 1, memory_order_relaxed);
    return tid_store;
}


// -----------------------------------------------------------------------------
// run_threads
// -----------------------------------------------------------------------------


struct run_thread_data
{
    size_t id;
    pthread_t tid;

    void *ctx;
    void (*fn) (size_t, void *);
};

void *run_thread_shim(void *args)
{
    struct run_thread_data *thread_data = args;
    thread_data->fn(thread_data->id, thread_data->ctx);
    return NULL;
}

void run_threads(void (*fn) (size_t, void *), void *ctx, size_t n)
{
    if (!n) n = cpus();
    optics_assert(n >= 2, "too few cpus detected: %lu < 2", n);

    struct run_thread_data thread[n];

    size_t set_len = cpus();
    cpu_set_t set[CPU_ALLOC_SIZE(set_len)];

    for (size_t i = 0; i < n; ++i) {
        thread[i].id = i;
        thread[i].fn = fn;
        thread[i].ctx = ctx;

        pthread_attr_t attr;
        int err = pthread_attr_init(&attr);
        if (err) {
            optics_fail_ierrno(err, "unable to initiate thread '%lu' attribute", i);
            optics_abort();
        }

        CPU_ZERO_S(set_len, set);
        CPU_SET_S(i % set_len, set_len, set);
        pthread_attr_setaffinity_np(&attr, set_len, set);

        err = pthread_create(&thread[i].tid, &attr, run_thread_shim, &thread[i]);
        if (err) {
            optics_fail_ierrno(err, "unable to create test thread '%lu'", i);
            optics_abort();
        }

        err = pthread_attr_destroy(&attr);
        if (err) {
            optics_fail_ierrno(err, "unable to destroy thread '%lu' attribute", i);
            optics_abort();
        }
    }

    for (size_t i = 0; i < n; ++i) {
        int ret = pthread_join(thread[i].tid, NULL);
        if (ret == -1) {
            optics_fail_errno("unable to join test thread '%lu'", i);
            optics_abort();
        }
    }
}
