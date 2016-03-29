/* backend_carbon.c
   Rémi Attab (remi.attab@gmail.com), 26 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "optics.h"
#include "utils/errors.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>


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
    assert(carbon->fd <= 0);

    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *head;
    int err = getaddrinfo(carbon->host, carbon->port, &hints, &head);
    if (err) {
        optics_warn("unable to resolve carbon host '%s:%s': %s",
                carbon->host, carbon->port, gai_strerror(err));
        return false;
    }

    for (struct addrinfo *addr = head; addr; addr = addr->ai_next) {

        carbon->fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (carbon->fd == -1) continue;

        if (!connect(carbon->fd, addr->ai_addr, addr->ai_addrlen))
            break;

        close(carbon->fd);
        carbon->fd = -1;
    }

    freeaddrinfo(head);

    if (carbon->fd > 0) return true;

    optics_warn_errno("unable to connect carbon socket for host '%s:%s'",
            carbon->host, carbon->port);
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

static void carbon_dump(void *ctx, optics_ts_t ts, const char *key, double value)
{
    struct carbon *carbon = ctx;

    char buffer[512];
    int ret = snprintf(buffer, sizeof(buffer), "%s %g %lu\n", key, value, ts);
    if (ret < 0 || (size_t) ret >= sizeof(buffer)) {
        optics_warn("skipping overly long carbon message: '%s'", buffer);
        return;
    }

    carbon_send(carbon, buffer, ret, ts);
}

static void carbon_free(void *ctx)
{
    struct carbon *carbon = ctx;
    close(carbon->fd);
    free(carbon);
}


// -----------------------------------------------------------------------------
// register
// -----------------------------------------------------------------------------


void optics_dump_carbon(struct optics_poller *poller, const char *host, const char *port)
{
    struct carbon *carbon = calloc(1, sizeof(struct carbon));
    carbon->host = strdup(host);
    carbon->port = strdup(port);

    optics_poller_backend(poller, carbon, &carbon_dump, &carbon_free);
}
