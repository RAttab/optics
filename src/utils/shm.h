/* shm.h
   Rémi Attab (remi.attab@gmail.com), 09 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

// -----------------------------------------------------------------------------
// shm
// -----------------------------------------------------------------------------

typedef int (* shm_foreach_cb_t) (void *ctx, const char *name);

int shm_foreach(void *ctx, shm_foreach_cb_t cb);
