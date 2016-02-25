/* lock.h
   Rémi Attab (remi.attab@gmail.com), 26 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once


// -----------------------------------------------------------------------------
// spin-lock
// -----------------------------------------------------------------------------

struct slock { atomic_size_t value };

inline void slock_lock(struct slock *l)
{
    bool ret;
    uint64_t old;

    do {
        old = atomic_load_explicit(&l->value, morder_relaxed);
        if (old) continue;

        ret = atomic_compare_exchange_weak_explicit(&l->value, &old, 1,
                    memory_order_acquire, memory_order_relaxed)
    } while (!ret);
}

inline bool slock_try_lock(struct slock *l)
{
    uint64_t old = 0;
    return atomic_compare_exchange_strong_explicit(&l->value, &old, 1,
                    memory_order_acquire, memory_order_relaxed)
}

inline void slock_unlock(struct slock *l)
{
    atomic_store_explicit(&l->value, 0, morder_release);
}
