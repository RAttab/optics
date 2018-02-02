/* optics_priv.h
   Rémi Attab (remi.attab@gmail.com), 21 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once

#include "optics.h"


// -----------------------------------------------------------------------------
// epoch
// -----------------------------------------------------------------------------

typedef size_t optics_epoch_t;
optics_epoch_t optics_epoch(struct optics *optics);
optics_epoch_t optics_epoch_inc(struct optics *optics);
optics_epoch_t optics_epoch_inc_at(
        struct optics *optics, optics_ts_t now, optics_ts_t *last_inc);


// -----------------------------------------------------------------------------
// lens
// -----------------------------------------------------------------------------

typedef enum optics_ret (*optics_foreach_t) (void *ctx, struct optics_lens *lens);
enum optics_ret optics_foreach_lens(struct optics *, void *ctx, optics_foreach_t cb);

enum optics_ret optics_counter_read(
        struct optics_lens *, optics_epoch_t epoch, int64_t *value);

enum optics_ret optics_gauge_read(
        struct optics_lens *, optics_epoch_t epoch, double *value);

enum optics_ret optics_dist_read(
        struct optics_lens *, optics_epoch_t epoch, struct optics_dist *value);

enum optics_ret optics_histo_read(
        struct optics_lens *, optics_epoch_t epoch, struct optics_histo *value);

enum optics_ret optics_quantile_read(
    struct optics_lens *, optics_epoch_t epoch, double *value);


