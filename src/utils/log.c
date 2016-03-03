/* log.c
   Rémi Attab (remi.attab@gmail.com), 20 Sep 2015
   FreeBSD-style copyright and disclaimer apply
*/

// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

static size_t ring_size = 1UL << 14;

static size_t tick_inc()
{
    static atomic_size_t ticks = 0;
    return atomic_fetch_add_explicit(&ticks, 1, memory_order_seq_cst);
}


// -----------------------------------------------------------------------------
// ring log
// -----------------------------------------------------------------------------

struct log_msg
{
    size_t tid;
    size_t tick;
    const char *title;
    char msg[128];
};

struct log_ring
{
    struct log_ring *next;

    size_t pos;
    atomic_uintptr_t data;
};

static pthread_once_t ring_once = PTHREAD_ONCE_INIT;
static pthread_key_t ring_key;

static struct slock ring_lock;
static struct log_ring *ring_head = NULL;

static void ring_init()
{
    if (pthread_key_create(&ring_key, NULL)) {
        optics_fail_errno("unable to create pthread key");
        optics_abort();
    }
}

static struct log_ring *ring_get()
{
    pthread_once(&ring_once, &ring_init);

    struct log_ring *ring = pthread_getspecific(ring_key);
    if (ring) return ring;

    ring = calloc(1, sizeof(struct log_ring));
    ring->data = (uintptr_t) calloc(ring_size, sizeof(struct log_msg));

    if (pthread_setspecific(ring_key, ring)) {
        optics_fail_errno("unable to set log thread specific data");
        optics_abort();
    }

    {
        slock_lock(&ring_lock);

        ring->next = ring_head;
        ring_head = ring;

        slock_unlock(&ring_lock);
    }

    return ring;
}

static void ring_log(const char *title, const char *fmt, va_list args)
{
    struct log_ring *ring = ring_get();

    struct log_msg *data = (void *) atomic_load_explicit(
            &ring->data, memory_order_relaxed);
    size_t i = ring->pos++ % ring_size;

    data[i].tick = tick_inc();
    data[i].tid = optics_tid();
    data[i].title = title;
    (void) vsnprintf(data[i].msg, sizeof(data[i].msg), fmt, args);
}

static size_t ring_count()
{
    size_t n = 0;
    for (struct log_ring *ring = ring_head; ring; ring = ring->next) n++;
    return n;
}

static int ring_cmp(const void *lhs_, const void *rhs_)
{
    struct log_msg *const *lhs = lhs_;
    struct log_msg *const *rhs = rhs_;

    return ((ssize_t) (*rhs)->tick) - ((ssize_t) (*lhs)->tick);
}

static void ring_dump()
{
    slock_lock(&ring_lock);

    size_t rings_len = ring_count();
    struct log_msg **rings = alloca(rings_len * sizeof(struct log_msg *));

    size_t i = 0;
    for (struct log_ring *ring = ring_head; ring; ring = ring->next) {
        struct log_msg *new = calloc(ring_size, sizeof(struct log_msg));
        rings[i++] = (void *) atomic_exchange_explicit(
                &ring->data, (uintptr_t) new, memory_order_relaxed);
    }

    struct log_msg **msgs = alloca(rings_len * ring_size * sizeof(struct log_msg *));

    size_t k = 0;
    for (size_t i = 0; i < rings_len; ++i) {
        for (size_t j = 0; j < ring_size; ++j)
            msgs[k++] = &rings[i][j];
    }

    qsort(msgs, k, sizeof(struct log_msg *), &ring_cmp);

    for (size_t i = 0; i < k; ++i) {
        if (!msgs[i]->tick) continue;
        fprintf(stderr, "[%8lu] <%lu> %s: %s\n",
                msgs[i]->tick, msgs[i]->tid, msgs[i]->title, msgs[i]->msg);
    }

    for (size_t i = 0; i < rings_len; ++i) free(rings[i]);

    slock_unlock(&ring_lock);
}


// -----------------------------------------------------------------------------
// log
// -----------------------------------------------------------------------------

void optics_log_impl(const char *title, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    if (OPTICS_LOG_RING) ring_log(title, fmt, args);
    else {
        char *buf = alloca(1024);
        (void) vsnprintf(buf, 1024, fmt, args);

        fprintf(stderr, "[%8lu] <%lu> %s: %s\n", tick_inc(), optics_tid(), title, buf);
    }
}

void optics_log_dump()
{
    if (OPTICS_LOG_RING) ring_dump();
}
