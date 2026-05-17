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

#include <logging/logging.h>

#include <string.h>
#include <arpa/inet.h>

#define _COMPONENT_ "enum"


typedef struct {
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t rdlength;
    char     rdata[];
} dns_rr_naptr_fields_t;

typedef struct {
    uint16_t order;
    uint16_t preference;
} dns_naptr_fields_t;


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

size_t
dns_section_size(void *sec, size_t len, size_t rrc)
{
    size_t s = 0;

    char *ptr = sec;

    for (size_t i = 0; i < rrc; i++) {
        while (*ptr && (void*)ptr < sec + len - 4) {
            ptr += *ptr + 1;
        }
        ptr += 4;
    }
    ptr++;

    return ptr - (char*)sec;
}


ssize_t
dns_serialize_error(void *buf, size_t len, const dns_hdr_t *recvhdr, void *recv,
    size_t recv_size, int error)
{
    if (len < recv_size)
        return -1;
    
    dns_hdr_t *sendhdr = buf;
    
    /* copy query */
    memcpy(buf, recv, recv_size);

    /* make answer */
    dns_flags_t flags = recvhdr->flags;
    flags.qr = 1; /* response */
    flags.aa = 1;
    flags.tc = 0;
    flags.ra = 0;
    flags.rcode = error;
    *(uint16_t*)&sendhdr->flags = htons(*(uint16_t*)&flags);

    return recv_size; /* same size */
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
    *dst++ = 0; /* terminator */

    size_t rrlen = (dst - (uint8_t*)buf) + sizeof(dns_rr_naptr_fields_t) + rdlength;
    if (len < rrlen)
        return -1;

    dns_rr_naptr_fields_t *rrf = (dns_rr_naptr_fields_t*)dst;
    rrf->type = htons(type);
    rrf->class = htons(class);
    rrf->ttl = htonl(ttl);
    //rrf->order = htons(10);
    //rrf->preference = htons(10);
    rrf->rdlength = htons(rdlength);

    memcpy(&rrf->rdata, rdata, rdlength);

    return rrlen;
}

ssize_t
dns_serialize_answer(void *buf, size_t len, const dns_hdr_t *recvhdr,
    void *recv, size_t recv_size, void *rr, size_t rrsize)
{
    if (len < sizeof(dns_hdr_t) + rrsize)
        return -1;

    dns_hdr_t *sendhdr = buf;

    /* copy header */
    memcpy(buf, recv, sizeof(dns_hdr_t));

    /* set flags and answer count */
    dns_flags_t flags = recvhdr->flags;
    flags.qr = 1; /* response */
    flags.aa = 1;
    flags.tc = 0;
    flags.ra = 0;
    flags.rcode = RCODE_NO_ERROR;
    *(uint16_t*)&sendhdr->flags = htons(*(uint16_t*)&flags);
    sendhdr->ancount = htons(1);

    /* copy question section */
    size_t qsecsize = dns_section_size(recv + sizeof(dns_hdr_t),
            recv_size - sizeof(dns_hdr_t), recvhdr->qdcount);
    DEBUG("q seciton size %ld", qsecsize);
    memcpy(buf + sizeof(dns_hdr_t), recv + sizeof(dns_hdr_t), qsecsize);

    /* copy rr into answer section */
    memcpy(buf + sizeof(dns_hdr_t) + qsecsize, rr, rrsize);

    /* copy additional section */
    memcpy(buf + sizeof(dns_hdr_t) + qsecsize + rrsize,
        recv + sizeof(dns_hdr_t) + qsecsize,
        recv_size - sizeof(dns_hdr_t) - qsecsize);

    return recv_size + rrsize;
}

