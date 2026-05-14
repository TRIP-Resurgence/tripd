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

typedef struct {
    uint16_t    id      : 16;
    uint8_t     qr      : 1;
    uint8_t     opcode  : 4;
    uint8_t     aa      : 1;
    uint8_t     tc      : 1;
    uint8_t     rd      : 1;
    uint8_t     ra      : 1;
    uint8_t     z       : 3;
    uint8_t     rcode   : 4;
    uint16_t    qdcount : 16;
    uint16_t    ancount : 16;
    uint16_t    nscount : 16;
    uint16_t    arcount : 16;
} dns_hdr_t;

enum rcode_e {
    RCODE_NO_ERROR = 0,
    RCODE_FORMAT_ERROR,
    RCODE_SERVER_FAIL,
    RCODE_NAME_ERROR,
    RCODE_NOT_IMPLEMENTED,
    RCODE_REFUSED
};

#endif /* _DNS_H */



