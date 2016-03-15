/* shm.h
   Rémi Attab (remi.attab@gmail.com), 09 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

// -----------------------------------------------------------------------------
// shm
// -----------------------------------------------------------------------------

enum shm_ret
{
    shm_ok = 0,
    shm_break = 1,
    shm_err = -1
};

typedef enum shm_ret (* shm_foreach_cb_t) (void *ctx, const char *name);

enum shm_ret shm_foreach(void *ctx, shm_foreach_cb_t cb);
