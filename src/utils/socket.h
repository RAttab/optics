/* socket.h
   Rémi Attab (remi.attab@gmail.com), 06 Apr 2016
   FreeBSD-style copyright and disclaimer apply
*/

#pragma once


// -----------------------------------------------------------------------------
// socket
// -----------------------------------------------------------------------------

int socket_stream_connect(const char *host, const char *port);


// -----------------------------------------------------------------------------
// hostname
// -----------------------------------------------------------------------------

bool hostname(char *buffer, size_t len);
