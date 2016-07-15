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

int socket_stream_listen(const char *port)
{
    struct addrinfo hints = {0};
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *head;
    int err = getaddrinfo(NULL, port, &hints, &head);
    if (err) {
        optics_fail("unable to resolve host '0.0.0.0:%s': %s", port, gai_strerror(err));
        return -1;
    }

    int fd = -1;
    for (struct addrinfo *addr = head; addr; addr = addr->ai_next) {

        fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (fd == -1) continue;

        if (!bind(fd, addr->ai_addr, addr->ai_addrlen)) {
            if (!listen(fd, 1)) break;
        }

        close(fd);
        fd = -1;
    }

    freeaddrinfo(head);

    if (fd > 0) return fd;

    optics_fail_errno("unable to connect stream socket for host '0.0.0.0:%s'", port);
    return -1;
}

int socket_stream_accept(int fd)
{
    socklen_t addrlen = 0;
    struct sockaddr addr = {0};

    int accept_fd = accept4(fd, &addr, &addrlen, 0);
    if (accept_fd >= 0) return accept_fd;

    optics_fail_errno("unable to accept a socket on fd '%d'", fd);
    return -1;
}


bool socket_send(int fd, size_t len, const void *data)
{
    ssize_t ret = send(fd, data, len, MSG_NOSIGNAL);
    if (ret == (ssize_t) len) return true;

    if (ret < 0) optics_fail_errno("unable to send announce packet");
    else optics_fail("unable to send full announce packet: %lu != %ld\n", len, ret);

    return false;
}

ssize_t socket_recv(int fd, size_t len, void *data)
{
    ssize_t ret = recv(fd, data, len, 0);
    if (ret > 0) return ret;

    optics_fail_errno("unable to send announce packet");
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
