/*

    trip: Modern TRIP LS implementation
    Copyright (C) 2026 arf20 (Ángel Ruiz Fernandez)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    server.c: implement http server

*/

/** \file
 * Implements HTTP server
 */

#include "server.h"

#include "http_status.h"
#include "api.h"

#include <logging/logging.h>
#include <netinet/in.h>
#include <util/util.h>

#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define _COMPONENT_ "api"


typedef struct {
    pthread_t thread;
    int fd;
    int run;
} server_t;

static server_t g_server;

static void
handle_request(int fd, const struct sockaddr_in6 *sa)
{
    static char buf[4096];
    ssize_t res = recv(fd, buf, 4096, 0);
    if (res == 0)
        goto end; /* connection closed */
    else if (res < 0)
        goto sock_error;

    int avail;
    ioctl(fd, FIONREAD, &avail);
    if (res == 4096 && avail) {
        SOCK_TRY_SEND(send(fd, STATUS_413, sizeof(STATUS_413), 0), goto sock_error);
        goto end;
    }

    char *body = strstr(buf, "\r\n\r\n") + 4;

    char *headers = strstr(buf, "\r\n");
    *headers = '\0';
    headers += 2;


    char *method = strtok(buf, " ");
    char *endpoint = strtok(NULL, " ");
    char *ver = strtok(NULL, " ");

    DEBUG("%s %s %s", sockaddr6_str(sa), method, endpoint);

    if (strcmp(ver, "HTTP/1.1") != 0) {
        SOCK_TRY_SEND(send(fd, STATUS_505, sizeof(STATUS_505), 0), goto sock_error);
        goto end;
    }

    for (int i = 0; api[i].endpoint; i++) {
        if (strncmp(endpoint, api[i].endpoint, strlen(api[i].endpoint))) {
            api[i].handler(fd, buf, res - (body - buf));
        }
    }

end:
    close(fd);
    return;

sock_error:
    ERROR("recv(): %s", strerror(errno));
    return;
}

static void *
server_loop(void *arg)
{
    server_t *server = arg;
    while (server->run) {
        struct sockaddr_in6 csa;
        socklen_t slen = sizeof(csa);
        int cfd = accept(server->fd, (struct sockaddr*)&csa, &slen);
        if (cfd < 0) {
            if (!server->run)
                ERROR("could not accept() client: %s", strerror(errno));
            return NULL;
        }

        handle_request(cfd, &csa);
    }

    return NULL;
}

int
server_run(const struct sockaddr_in6 *listen_addr)
{
    if (g_server.thread)
        return -1;

    int fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        ERROR("could not create listen socket: %s", strerror(errno));
        goto error;
    }

    if (bind(fd, (const struct sockaddr*)listen_addr,
        sizeof(struct sockaddr_in6)) < 0)
    {
        ERROR("could not bind() listen socket: %s", strerror(errno));
        goto error;
    }

    if (listen(fd, SOMAXCONN) < 0) {
        ERROR("could not listen() listen socket: %s", strerror(errno));
        goto error;
    }

    g_server.fd = fd;
    g_server.run = 1;

    pthread_create(&g_server.thread, NULL, &server_loop, &g_server);

    DEBUG("started session manager, listening at [%s]:%d",
        sockaddr_str((struct sockaddr *)listen_addr),
        ntohs(listen_addr->sin6_port));

    return 0;

error:
    return -1;
}

void
server_stop()
{
    g_server.run = 0;
    shutdown(g_server.fd, SHUT_RDWR);
    pthread_join(g_server.thread, NULL);
}

