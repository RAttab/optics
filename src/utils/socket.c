/* socket.c
   Rémi Attab (remi.attab@gmail.com), 06 Apr 2016
   FreeBSD-style copyright and disclaimer apply
*/


// -----------------------------------------------------------------------------
// socket
// -----------------------------------------------------------------------------

int socket_stream_connect(const char *host, const char *port)
{
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *head;
    int err = getaddrinfo(host, port, &hints, &head);
    if (err) {
        optics_fail("unable to resolve host '%s:%s': %s",
                host, port, gai_strerror(err));
        return -1;
    }

    int fd = -1;
    for (struct addrinfo *addr = head; addr; addr = addr->ai_next) {

        fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (fd == -1) continue;

        if (!connect(fd, addr->ai_addr, addr->ai_addrlen))
            break;

        close(fd);
        fd = -1;
    }

    freeaddrinfo(head);

    if (fd > 0) return fd;

    optics_fail_errno("unable to connect stream socket for host '%s:%s'", host, port);
    return -1;
}


// -----------------------------------------------------------------------------
// hostname
// -----------------------------------------------------------------------------

bool hostname(char *buffer, size_t len)
{
    if (!gethostname(buffer, len)) return true;

    optics_fail_errno("unable to get the hostname");
    return false;
}
