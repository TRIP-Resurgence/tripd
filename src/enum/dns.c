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


typedef struct __attribute__((packed)) {
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t rdlength;
    char     rdata[];
} dns_rr_fields_t;

typedef struct __attribute__((packed)) {
    uint16_t order;
    uint16_t preference;
    /* <character-string> flags */
    /* <character-string> services */
    /* <character-string> refexp */
    /* <domain-name> replacement */
} dns_rdata_naptr_fields_t;


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
dns_parse_domain_name(void *buf, size_t len, char *out, size_t outlen)
{
    uint8_t *src = buf;
    char *dst = out;
    while (*src && ((void*)src < buf + len) && (dst < out + outlen)) {
        uint8_t lablen = *src;
        memcpy(dst, src + 1, lablen);
        src += lablen + 1;
        dst[lablen] = '.';
        dst += lablen + 1;
    };
    *dst = '\0';
    src++;

    return (void*)src - buf;
}

size_t
dns_parse_question(void *buf, size_t len, dns_question_t *q)
{
    void *ptr = buf +
        dns_parse_domain_name(buf, len, q->qname, sizeof(q->qname));

    q->qtype = ntohs(*(uint16_t*)ptr);
    ptr += 2;
    q->qclass = ntohs(*(uint16_t*)ptr);
    ptr += 2;

    return (void*)ptr - buf;
}

size_t
dns_q_section_size(void *sec, size_t len, size_t rrc)
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
dns_serialize_domain_name(void *buf, size_t len, const char *name)
{
    char *nextdot = NULL;
    uint8_t *dst = buf;
    while (*name && (nextdot = strchr(name, '.'))) {
        if ((void*)dst >= buf + len)
            return -1;
        *dst = nextdot - name;
        memcpy(dst + 1, name, *dst);
        name += *dst + 1;
        dst += *dst + 1;
    }
    *dst++ = 0; /* terminator */

    return (void*)dst - buf;
}

ssize_t
dns_serialize_rdata_naptr(void *buf, size_t len, uint16_t order,
    uint16_t preference, const char *flags, const char *services,
    const char *regex)
{
    dns_rdata_naptr_fields_t *fields = buf;
    fields->order = htons(order);
    fields->preference = htons(preference);

    uint8_t *ptr = buf + sizeof(dns_rdata_naptr_fields_t);

    *ptr = strlen(flags);
    memcpy(ptr + 1, flags, *ptr);
    ptr += *ptr + 1;

    *ptr = strlen(services);
    memcpy(ptr + 1, services, *ptr);
    ptr += *ptr + 1;

    *ptr = strlen(regex);
    memcpy(ptr + 1, regex, *ptr);
    ptr += *ptr + 1;

    *ptr++ = '\0';

    return (void*)ptr - buf;
}

ssize_t
dns_serialize_rr(void *buf, size_t len, const char *qto,
    uint16_t type, uint16_t class, uint32_t ttl, uint16_t rdlength, void *rdata)
{
    /* compressed domain-name */
    *(uint16_t*)buf = htons(0b1100000000001100);

    size_t rrlen = 2 + sizeof(dns_rr_fields_t) + rdlength;
    if (len < rrlen)
        return -1;

    dns_rr_fields_t *rrf = (dns_rr_fields_t*)(buf + 2);
    rrf->type = htons(type);
    rrf->class = htons(class);
    rrf->ttl = htonl(ttl);
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
    sendhdr->nscount = htons(0);
    sendhdr->arcount = htons(0);

    /* copy question section */
    size_t qsecsize = dns_q_section_size(recv + sizeof(dns_hdr_t),
            recv_size - sizeof(dns_hdr_t), recvhdr->qdcount);
    memcpy(buf + sizeof(dns_hdr_t), recv + sizeof(dns_hdr_t), qsecsize);

    /* copy rr into answer section */
    memcpy(buf + sizeof(dns_hdr_t) + qsecsize, rr, rrsize);

    return sizeof(dns_hdr_t) + qsecsize + rrsize;
}

