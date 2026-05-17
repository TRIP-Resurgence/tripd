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

*/

/** \file
 * \brief DNS protocol header
 */

#ifndef _DNS_H
#define _DNS_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#define DNS_FLAG_QR(x)      (((x) >> 15) & 1)
#define DNS_FLAG_OPCODE(x)  (((x) >> 11) & 0b1111)
#define DNS_FLAG_AA(x)      (((x) >> 10) & 1)
#define DNS_FLAG_TC(x)      (((x) >> 9) & 1)
#define DNS_FLAG_RD(x)      (((x) >> 8) & 1)
#define DNS_FLAG_RA(x)      (((x) >> 7) & 1)
#define DNS_FLAG_Z(x)       (((x) >> 4) & 1)
#define DNS_FLAG_RCODE(x)   ((x) & 0b1111)

#define DNS_SET_FLAG_QR(x)      (((x) << 15) &  0b0111111111111111)
#define DNS_SET_FLAG_OPCODE(x)  (((x) << 11) &  0b1000011111111111)
#define DNS_SET_FLAG_AA(x)      (((x) << 10) &  0b1111101111111111)
#define DNS_SET_FLAG_TC(x)      (((x) << 9) &   0b1111110111111111)
#define DNS_SET_FLAG_RD(x)      (((x) << 8) &   0b1111111011111111)
#define DNS_SET_FLAG_RA(x)      (((x) << 7) &   0b1111111101111111)
#define DNS_SET_FLAG_Z(x)       (((x) << 4) &   0b1111111110001111)
#define DNS_SET_FLAG_RCODE(x)   ((x) &          0b1111111111110000)


typedef struct __attribute__((packed)) {
    uint8_t     rcode   : 4;
    uint8_t     z       : 3;
    uint8_t     ra      : 1;
    uint8_t     rd      : 1;
    uint8_t     tc      : 1;
    uint8_t     aa      : 1;
    uint8_t     opcode  : 4;
    uint8_t     qr      : 1;
} dns_flags_t;

typedef struct __attribute__((packed)) {
    uint16_t    id;
    dns_flags_t flags;
    uint16_t    qdcount;
    uint16_t    ancount;
    uint16_t    nscount;
    uint16_t    arcount;
} dns_hdr_t;

enum rcode_e {
    RCODE_NO_ERROR = 0,
    RCODE_FORMAT_ERROR,
    RCODE_SERVER_FAIL,
    RCODE_NAME_ERROR,
    RCODE_NOT_IMPLEMENTED,
    RCODE_REFUSED
};

typedef struct {
    char     qname[256];
    uint16_t qtype;
    uint16_t qclass;
} dns_question_t;

enum qtype_e {
    TYPE_NAPTR = 35,
    QTYPE_ALL = 255
};

enum qclass_e {
    CLASS_IN = 1,
    QCLASS_ANY = 256
};


ssize_t dns_parse_hdr(void *buf, size_t len, dns_hdr_t *out);

size_t dns_parse_question(void *buf, size_t len, dns_question_t *q);

ssize_t dns_serialize_error(void *buf, size_t len, const dns_hdr_t *recvhdr,
    void *recv, size_t recv_size, int error);

ssize_t dns_serialize_rr(void *buf, size_t len, const char *qto,
    uint16_t type, uint16_t class, uint32_t ttl, uint16_t rdlength, 
    void *rdata);

ssize_t dns_serialize_answer(void *buf, size_t len, const dns_hdr_t *recvhdr,
    void *recv, size_t recv_size, void *rr, size_t rrsize);

#endif /* _DNS_H */

