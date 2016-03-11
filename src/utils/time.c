/* time.c
   Rémi Attab (remi.attab@gmail.com), 03 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/


// -----------------------------------------------------------------------------
// time
// -----------------------------------------------------------------------------

bool optics_nsleep(uint64_t nanos)
{
    struct timespec ts;
    optics_now(&ts);

    ts.tv_nsec += nanos;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec += ts.tv_nsec / 1000000000;
        ts.tv_nsec = ts.tv_nsec % 1000000000;
    }

    while (true) {
        int ret = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL);
        if (!ret) return true;
        if (errno == EINTR) continue;

        optics_fail_errno("unable to sleep via clock_nanosleep");
        return false;
    }
}

void optics_yield()
{
    sched_yield();
}

uint64_t optics_rdtsc()
{
    uint64_t msb, lsb;

#ifdef __amd64
    __asm__ __volatile__ ("rdtsc" : "=a" (lsb), "=d" (msb));
#else
#error "Read your platform's perf counter here"
#endif

    return msb << 32 | lsb;
}
