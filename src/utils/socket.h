/* socket.h
   Rémi Attab (remi.attab@gmail.com), 06 Apr 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once


// -----------------------------------------------------------------------------
// socket
// -----------------------------------------------------------------------------

int socket_stream_connect(const char *host, const char *port);
int socket_stream_listen(const char *port);
int socket_stream_accept(int fd);

bool socket_send(int fd, size_t len, const void *data);
ssize_t socket_recv(int fd, size_t len, void *data);

// -----------------------------------------------------------------------------
// hostname
// -----------------------------------------------------------------------------

bool hostname(char *buffer, size_t len);
