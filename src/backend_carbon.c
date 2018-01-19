/* backend_carbon.c
   Rémi Attab (remi.attab@gmail.com), 26 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "optics.h"
#include "utils/errors.h"
#include "utils/socket.h"
#include "utils/buffer.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/socket.h>


// -----------------------------------------------------------------------------
// carbon
// -----------------------------------------------------------------------------

struct carbon
{
    const char *host;
    const char *port;

    int fd;
    uint64_t last_attempt;
};

static bool carbon_connect(struct carbon *carbon)
{
    optics_assert(carbon->fd <= 0,
            "attempting to connect to carbon while already connected");

    carbon->fd = socket_stream_connect(carbon->host, carbon->port);
    if (carbon->fd > 0) return true;

    optics_perror(&optics_errno);
    return false;
}

static void carbon_send(
        struct carbon *carbon, const char *data, size_t len, optics_ts_t ts)
{
    if (carbon->fd <= 0) {
        if (carbon->last_attempt == ts) return;
        carbon->last_attempt = ts;
        if (!carbon_connect(carbon)) return;
    }

    ssize_t ret = send(carbon->fd, data, len, MSG_NOSIGNAL);
    if (ret > 0 && (size_t) ret == len) return;
    if (ret >= 0) {
        optics_warn("unexpected partial message send: %lu '%s'", len, data);
        return;
    }

    optics_warn_errno("unable to send to carbon host '%s:%s'",
            carbon->host, carbon->port);
    close(carbon->fd);
    carbon->fd = -1;
}


// -----------------------------------------------------------------------------
// callbacks
// -----------------------------------------------------------------------------

struct dump_ctx
{
    struct carbon *carbon;
    const struct optics_poll *poll;
};

static bool carbon_dump_normalized(
        void *ctx_, optics_ts_t ts, const char *key, double value)
{
    struct dump_ctx *ctx = ctx_;
    struct buffer buffer = {0};

    buffer_printf(&buffer, "%s.%s", ctx->poll->prefix, ctx->poll->host);
    buffer_printf(&buffer, ".%s %g %lu\n", key, value, ts);

    carbon_send(ctx->carbon, buffer.data, buffer.len, ts);
    buffer_reset(&buffer);

    return true;
}

static void carbon_dump(
        void *ctx_, enum optics_poll_type type, const struct optics_poll *poll)
{
    if (type != optics_poll_metric) return;

    struct dump_ctx ctx = { .carbon = ctx_, .poll = poll };
    (void) optics_poll_normalize(poll, carbon_dump_normalized, &ctx);
}

static void carbon_free(void *ctx)
{
    struct carbon *carbon = ctx;
    close(carbon->fd);
    free((void *) carbon->host);
    free((void *) carbon->port);
    free(carbon);
}


// -----------------------------------------------------------------------------
// register
// -----------------------------------------------------------------------------


void optics_dump_carbon(struct optics_poller *poller, const char *host, const char *port)
{
    struct carbon *carbon = calloc(1, sizeof(*carbon));
    optics_assert_alloc(carbon);
    carbon->host = strdup(host);
    carbon->port = strdup(port);

    optics_poller_backend(poller, carbon, &carbon_dump, &carbon_free);
}
