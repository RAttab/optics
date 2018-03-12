/* test_http.c
   Rémi Attab (remi.attab@gmail.com), 10 Jun 2016
   FreeBSD-style copyright and disclaimer apply
*/


#include "utils/socket.h"
#include "utils/buffer.h"
#include "utils/time.h"

#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>


// -----------------------------------------------------------------------------
// http
// -----------------------------------------------------------------------------

struct http_client
{
    int fd;
};

struct http_client * http_connect(unsigned port)
{
    struct http_client *client = calloc(1, sizeof(*client));

    char port_str[32];
    snprintf(port_str, sizeof(port_str), "%u",port);

    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };
    struct addrinfo *head;
    if (getaddrinfo("127.0.0.1", port_str, &hints, &head) < -1) {
        optics_fail_errno("getaddrinfo");
        optics_abort();
    }

    client->fd = -1;
    for (struct addrinfo *addr = head; addr; addr = addr->ai_next) {
        client->fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (client->fd < 0) continue;

        int ret = connect(client->fd, addr->ai_addr, addr->ai_addrlen);
        if (ret < 0) {
            close(client->fd);
            continue;
        }
    }

    if (client->fd < 0) optics_fail_errno("socket-connect");

    freeaddrinfo(head);
    return client;
}

void http_close(struct http_client *client)
{
    close(client->fd);
    free(client);
}

void http_req(struct http_client *client, const char *method, const char *path, const char *body)
{
    struct buffer buffer = {0};
    buffer_printf(&buffer, "%s %s HTTP/1.1\r\n", method, path);

    if (body) {
        buffer_printf(&buffer,
                "Content-type: text/plain\r\n"
                "Content-length: %lu\r\n"
                "\r\n"
                "%s",
                strlen(body), body);
    }
    else buffer_write(&buffer, "\r\n", 2);

    ssize_t sent = send(client->fd, buffer.data, buffer.len, 0);
    if (sent != (ssize_t) buffer.len) {
        optics_fail_errno("send");
        optics_abort();
    }

    buffer_reset(&buffer);
}

bool http_assert_resp(struct http_client *client, unsigned exp_code, const char *exp_body)
{
    // MHD can split the data into multiple packets and our parser is too dumb
    // to handle it properly so instead we just pause to make sure we receive
    // everything before we process it. This pause also has to account for
    // running the test under valgrind so it's pretty large considering.
    nsleep(10 * 1000 * 1000);

    char buffer[4096];
    ssize_t n = recv(client->fd, buffer, sizeof(buffer), 0);
    if (n < 0) {
        optics_fail_errno("recv");
        optics_abort();
    }

    buffer[n] = '\0';

    char *i = buffer;
    char *p = strstr(i, "\r\n");
    *p = '\0';

    unsigned code = 0;
    sscanf(i, "HTTP/1.1 %u", &code);
    i = p + 2;

    // consume all the headers.
    while ((p = strstr(i, "\r\n"))) i = p + 2;

    bool result = true;
    if (code != exp_code) {
        printf("FAIL(code): %d != %d\n%s\n", code, exp_code, buffer);
        result = false;
    }

    if (exp_body && strcmp(i, exp_body)) {
        printf("FAIL(body): %s != %s\n%s\n", i, exp_body, buffer);
        result = false;
    }

    return result;
}
