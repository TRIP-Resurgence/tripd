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

    enum.c: implement enum query interface

*/

/** \file
 * Implements ENUM query interface
 */

#include "enum.h"
#include "enum/dns.h"

#include <logging/logging.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <util/util.h>

#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>

#define _COMPONENT_ "enum"


static void
make_error_msg(void *buf, uint16_t id, uint8_t opcode, int error)
{
    dns_hdr_t *sendhdr = buf;
    sendhdr->id = id;
    sendhdr->qr = 0;
    sendhdr->opcode = opcode;
    sendhdr->aa = 0;
    sendhdr->tc = 0;
    sendhdr->rd = 0;
    sendhdr->ra = 0;
    sendhdr->z= 0;
    sendhdr->rcode = error;
    sendhdr->qdcount = 0;
    sendhdr->qdcount = 0;
    sendhdr->qdcount = 0;
    sendhdr->qdcount = 0;
}

static void
handle_request(int fd, void *buf, size_t len, const struct sockaddr_in6 *sa,
    socklen_t salen, trib_t *trib)
{
    dns_hdr_t *hdr = buf;
    char sendbuf[4096];
    size_t sendsize = 0;

    if (hdr->qr != 1) {
        DEBUG("not a query");
        make_error_msg(sendbuf, hdr->id, hdr->opcode, RCODE_FORMAT_ERROR);
        sendsize = sizeof(dns_hdr_t);
    }

    DEBUG("query %d %d", hdr->id, hdr->opcode);

    if (hdr->opcode == 1 || hdr->opcode == 2) {
        make_error_msg(sendbuf, hdr->id, hdr->opcode, RCODE_NOT_IMPLEMENTED);
        sendsize = sizeof(dns_hdr_t);
    }


    if (sendto(fd, sendbuf, sendsize, 0, (struct sockaddr*)sa, salen) != sendsize) {
        DEBUG("sendto(): %s", strerror(errno));
    }
}

static void *
enum_loop(void *arg)
{
    enum_t *en = arg;
    char buf[4096];
    struct sockaddr_in6 csa;
    socklen_t csa_len;

    while (en->run) {
        int res = recvfrom(en->fd, buf, 4096, 0, (struct sockaddr*)&csa, 
            &csa_len);
        if (res <= 0) {
            ERROR("recvfrom(): %s", strerror(errno));
            continue;
        }


        handle_request(en->fd, buf, res, &csa, csa_len, en->trib);
    }

    return NULL;
}


enum_t *
enum_new(const char *zone, const struct sockaddr_in6 *listen_sa, trib_t *trib)
{
    enum_t *s = malloc(sizeof(enum_t));

    int fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
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

    s->fd = fd;
    s->run = 1;
    s->listen_sa = *listen_sa;
    s->trib = trib;

    return s;

error:
    return NULL;
}

void
enum_run(enum_t *en)
{
    pthread_create(&en->thread, NULL, &enum_loop, en);

    DEBUG("started session manager, listening at [%s]:%d",
        sockaddr_str((struct sockaddr *)&en->listen_sa),
        ntohs(en->listen_sa.sin6_port));
}

void
enum_stop(enum_t *en)
{
    en->run = 0;
    shutdown(en->fd, SHUT_RDWR);
    pthread_join(en->thread, NULL);
    close(en->fd);
}

