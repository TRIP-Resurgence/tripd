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
#include "db/trib.h"
#include "enum/dns.h"
#include "protocol/protocol.h"

#include <logging/logging.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
#include <util/util.h>

#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>

#define _COMPONENT_ "enum"


static const char *
service(uint16_t app_proto)
{
    switch (app_proto) {
        case APP_PROTO_SIP: return "E2U+sip";
        case APP_PROTO_IAX2: return "E2U+iax";
        default: return NULL;
    }
}

static void
handle_request(enum_t *en, void *buf, size_t len, const struct sockaddr_in6 *sa,
    socklen_t salen, trib_t *trib)
{
    dns_hdr_t hdr;
    char sendbuf[4096];
    size_t sendsize = 0;

    if (dns_parse_hdr(buf, len, &hdr) < 0) {
        ERROR("dns header too short");
        return;
    }

    if (hdr.flags.qr != 0) {
        DEBUG("not a query");
        sendsize = dns_serialize_error(sendbuf, sizeof(sendbuf), &hdr,
            buf, len, RCODE_FORMAT_ERROR);
    }

    DEBUG("query[%ld] from %s id %d opcode %d qcount %d",
        len, sockaddr6_str((struct sockaddr_in6*)&sa), hdr.id, hdr.flags.opcode,
        hdr.qdcount);

    if (hdr.flags.opcode != 0) {
        sendsize = dns_serialize_error(sendbuf, sizeof(sendbuf), &hdr,
            buf, len, RCODE_NOT_IMPLEMENTED);
    }

    int zonelen = zonelen = strlen(en->zone);

    char *qptr = buf + sizeof(dns_hdr_t);
    for (int i = 0; i < hdr.qdcount; i++) {
        dns_question_t q;
        qptr += dns_parse_question(qptr, len, &q);

        if (q.qtype != TYPE_NAPTR && q.qtype != QTYPE_ALL)
            continue;

        if (q.qclass != CLASS_IN && q.qtype != QCLASS_ANY)
            continue;

        int qlen = strlen(q.qname);
        char *zone = NULL;
        if (qlen >= zonelen)
            zone = q.qname + (qlen - zonelen);
        if (!zone || (strcmp(zone, en->zone) != 0)) {
            sendsize = dns_serialize_error(sendbuf, sizeof(sendbuf), &hdr,
                buf, len, RCODE_NAME_ERROR);
            break;
        }

        char num[256];
        int tlen = qlen - zonelen, numlen = tlen / 2;
        for (int i = 0; i < numlen; i++) {
            num[numlen - i - 1] = q.qname[2 * i];
        }
        num[numlen] = '\0';

        DEBUG(" question %s type %d class %d -> num %s",
            q.qname, q.qtype, q.qclass, num);

        /* lookup query */
        const entry_t *e = trib_table_lookup(&trib->loc_trib, 0, 0, num);
        if (!e) {
            sendsize = dns_serialize_error(sendbuf, sizeof(sendbuf), &hdr,
                buf, len, RCODE_NAME_ERROR);
            DEBUG("not found");
            break;
        }

        /* construct answer */
        char rr[4096], rdata[1024], regex[512];

        size_t regexlen = snprintf(regex, sizeof(regex),
            "!^.*$!sip:%s@%s!", num, e->attrs.nexthop);

        size_t rdlength = dns_serialize_rdata_naptr(rdata, sizeof(rdata),
            100, 10, "u", service(e->app_proto), regex);
        size_t rrsize = dns_serialize_rr(rr, sizeof(rr), q.qname, TYPE_NAPTR,
            CLASS_IN, 1, rdlength, rdata);
        sendsize = dns_serialize_answer(sendbuf, sizeof(sendbuf), &hdr,
            buf, len, rr, rrsize);

        DEBUG(" answer %s", regex);
    }


    if (sendto(en->fd, sendbuf, sendsize, 0, (struct sockaddr*)sa, salen)
        != sendsize)
    {
        ERROR("sendto(): %s", strerror(errno));
    }
}

static void *
enum_loop(void *arg)
{
    enum_t *en = arg;
    char buf[4096];
    struct sockaddr_in6 csa;
    socklen_t csa_len = sizeof(csa);

    while (en->run) {
        int res = recvfrom(en->fd, buf, 4096, 0, (struct sockaddr*)&csa, 
            &csa_len);
        if (res <= 0 && en->run) {
            ERROR("recvfrom(): %s", strerror(errno));
            continue;
        }

        handle_request(en, buf, res, &csa, csa_len, en->trib);
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
    s->zone = strdup(zone);
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

void
enum_destroy(enum_t *en)
{
    free(en->zone);
    free(en);
}

