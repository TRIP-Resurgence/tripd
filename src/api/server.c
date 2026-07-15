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
#include <stdlib.h>

#define _COMPONENT_ "api"


static void
handle_request(int fd, const struct sockaddr_in6 *sa, trib_t *trib)
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
        DEBUG("request %s too large", sockaddr6_str(sa));
        goto end;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);

    char *body = strstr(buf, "\r\n\r\n") + 4;

    char *headers = strstr(buf, "\r\n");
    *headers = '\0';
    headers += 2;


    char *method = strtok(buf, " ");
    char *endpoint = strtok(NULL, " ");
    char *ver = strtok(NULL, " ");

    DEBUG("request %s %s %s", sockaddr6_str(sa), method, endpoint);

    if (strcmp(ver, "HTTP/1.1") != 0) {
        SOCK_TRY_SEND(send(fd, STATUS_505, sizeof(STATUS_505), 0), goto sock_error);
        DEBUG(" -> response 505");
        goto end;
    }

    for (int i = 0; api[i].endpoint; i++) {
        int len = strlen(api[i].endpoint);
        if (strncmp(endpoint, api[i].endpoint, len) == 0) {
            res = api[i].handler(fd, endpoint + len, buf,
                res - (body - buf), trib);
            if (res < 0) {
                DEBUG(" -> socket error");
            } else {
                clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
                DEBUG(" -> response %d in %fus", res,
                    (1000000.0 * (double)(end.tv_sec - start.tv_sec))
                    + (0.001 * (double)(end.tv_nsec - start.tv_nsec)));
            }
            goto end;
        }
    }

    SOCK_TRY_SEND(send(fd, STATUS_404, sizeof(STATUS_404), 0), goto sock_error);
    DEBUG(" -> response 404");

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
            if (server->run)
                ERROR("could not accept() client: %s", strerror(errno));
            return NULL;
        }

        handle_request(cfd, &csa, server->trib);
    }

    return NULL;
}


server_t *
server_new(const struct sockaddr_in6 *listen_sa, trib_t *trib)
{
    server_t *s = malloc(sizeof(server_t));

    int fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        ERROR("could not create listen socket: %s", strerror(errno));
        goto error;
    }

    if (bind(fd, (const struct sockaddr*)listen_sa,
        sizeof(struct sockaddr_in6)) < 0)
    {
        ERROR("could not bind() listen socket: %s", strerror(errno));
        goto error;
    }

    if (listen(fd, SOMAXCONN) < 0) {
        ERROR("could not listen() listen socket: %s", strerror(errno));
        goto error;
    }

    s->fd = fd;
    s->run = 1;
    s->listen_sa = *listen_sa;
    s->trib = trib;

    return s;

error:
    return NULL;
}

void
server_run(server_t *server)
{
    pthread_create(&server->thread, NULL, &server_loop, server);

    DEBUG("started session manager, listening at [%s]:%d",
        sockaddr_str((struct sockaddr *)&server->listen_sa),
        ntohs(server->listen_sa.sin6_port));
}

void
server_stop(server_t *server)
{
    server->run = 0;
    shutdown(server->fd, SHUT_RDWR);
    pthread_join(server->thread, NULL);
    close(server->fd);
}

void
server_destroy(server_t *server)
{
    free(server);
}

