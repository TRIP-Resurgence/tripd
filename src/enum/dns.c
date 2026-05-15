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

    dns.c: implement dns protocol

*/

/** \file
 * Implements DNS protocol routines
 */

#include "dns.h"

#include <string.h>
#include <arpa/inet.h>


typedef struct {
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t rdlength;
    char     rdata[];
} dns_rr_fields_t;


ssize_t
dns_parse_hdr(void *buf, size_t len, dns_hdr_t *out)
{
    if (len < sizeof(dns_hdr_t))
        return -1;

    dns_hdr_t *src = buf;

    out->id = ntohs(src->id);
    *(uint16_t*)&out->flags = ntohs(*(uint16_t*)&src->flags);
    out->qdcount = ntohs(src->qdcount);
    out->ancount = ntohs(src->ancount);
    out->nscount = ntohs(src->nscount);
    out->arcount = ntohs(src->arcount);

    return sizeof(dns_hdr_t);
}

size_t
dns_parse_question(void *buf, size_t len, dns_question_t *q)
{
    uint8_t *src = buf;
    char *dst = q->qname;
    while (*src && (void*)src < buf + len - 4) {
        uint8_t lablen = *src;
        memcpy(dst, src + 1, lablen);
        src += lablen + 1;
        dst[lablen] = '.';
        dst += lablen + 1;
    };

    *dst = '\0';
    src++;
    q->qtype = ntohs(*(uint16_t*)src);
    src += 2;
    q->qclass = ntohs(*(uint16_t*)src);

    return (void*)src - buf;
}


ssize_t
dns_serialize_error(void *buf, size_t len, uint16_t id, uint8_t opcode,
    int error)
{
    if (len < sizeof(dns_hdr_t))
        return -1;
    dns_hdr_t *sendhdr = buf;
    sendhdr->id = id;
    sendhdr->flags.qr = 0;
    sendhdr->flags.opcode = opcode;
    sendhdr->flags.aa = 0;
    sendhdr->flags.tc = 0;
    sendhdr->flags.rd = 0;
    sendhdr->flags.ra = 0;
    sendhdr->flags.z= 0;
    sendhdr->flags.rcode = error;
    sendhdr->qdcount = 0;
    sendhdr->qdcount = 0;
    sendhdr->qdcount = 0;
    sendhdr->qdcount = 0;
    return sizeof(dns_hdr_t);
}


ssize_t
dns_serialize_rr(void *buf, size_t len, const char *qto,
    uint16_t type, uint16_t class, uint32_t ttl, uint16_t rdlength, void *rdata)
{
    char *nextdot = NULL;
    uint8_t *dst = buf;
    while (*qto && (nextdot = strchr(qto, '.'))) {
        if ((void*)dst >= buf + len)
            return -1;
        *dst = nextdot - qto;
        memcpy(dst + 1, qto, *dst);
        qto += *dst + 1;
        dst += *dst + 1;
    }

    size_t rrlen = (dst - (uint8_t*)buf) + sizeof(dns_rr_fields_t) + rdlength;
    if (len < rrlen)
        return -1;

    dns_rr_fields_t *rrf = (dns_rr_fields_t*)dst;
    rrf->type = type;
    rrf->class = class;
    rrf->ttl = ttl;
    rrf->rdlength = rdlength;

    memcpy(&rrf->rdata, rdata, rdlength);

    return rrlen;
}

