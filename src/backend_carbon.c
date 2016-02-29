/* backend_carbon.c
   Rémi Attab (remi.attab@gmail.com), 26 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/


// -----------------------------------------------------------------------------
// carbon
// -----------------------------------------------------------------------------

struct carbon
{
    const char *host;
    const char *port;

    int fd;
    time_t last_attempt;
};

bool connect(struct carbon *carbon)
{
    assert(carbon->fd <= 0);

    struct addrinfo hints = {0};
    hits.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *head;
    int err = getaddrinfo(carbon->host, carbon->port, &hints, &head);
    if (err) {
        optics_warn("unable to resolve carbon host '%s:%s': %s",
                carbon->host, carbon->port, gai_strerror(err));
        return false;
    }

    struct addrinfo *addr = head;
    while (addr) {
        carbon->fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (carbon->fd == -1) {
            optics_warn_errno("unable to create carbon socket for host '%s:%s'",
                    carbon->host, carbon->port);

            addr = addr->ai_next;
            continue;
        }

        int ret = connect(carbon->fd, addr->ai_addr, addr->ai_addrlen);
        if (ret == 1) {
            optics_warn_errno("unable to connect to carbon host '%s:%s'",
                    carbon->host, carbon->port);

            close(carbon->fd);
            carbon->fd = -1;

            addr = addr->ai_next;
            continue;
        }

        break;
    }

    freeaddrinfo(head);
    return carbon->fd > 0;
}

static void send(struct carbon *carbon, const uint8_t *data, size_t len, time_t now)
{
    if (carbon->fd <= 0) {
        if (carbon->last_attempt == now) return;
        carbon->last_attempt  = now;
        if (!connect(carbon)) return;
    }

    ssize_t ret = send(carbon->fd, data, len, 0);
    if (ret == len) return;
    if (ret >= 0) {
        optics_warn("unexpected partial message send: %lu '%s'", len, data);
        return;
    }

    optics_warn_errno("unable to send to carbon host '%s:%s'",
            carbon->host, carbon->port);
    close(carbon->fd);
}


// -----------------------------------------------------------------------------
// callbacks
// -----------------------------------------------------------------------------

static void dump(void *ctx, time_t ts, const char *key, double value)
{
    struct carbon *carbon = ctx;

    char buffer[512];
    size_t ret = snprintf(buffer, sizeof(buffer), "%s %g %d", key, value, ts);
    if (ret >= sizeof(buffer)) {
        optics_warn("skipping overly long carbon message: '%s'", buffer);
        return;
    }

    send(carbon, buffer, sizeof(buffer));
}

static void free(void *ctx)
{
    struct carbon *carbon = ctx;
    close(carbon->fd);
    free(carbon);
}


// -----------------------------------------------------------------------------
// register
// -----------------------------------------------------------------------------


bool optics_dump_carbon(struct optics_poller *poller, const char *host, const char *port)
{
    struct carbon *carbon = calloc(1, sizeof(struct carbon));
    carbon->host = host;
    carbon->port = port;

    optics_poller_backend(poller, carbon, &dump, &free);
    return true;
}
