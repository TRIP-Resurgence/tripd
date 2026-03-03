/*

    trip: Modern TRIP LS implementation
    Copyright (C) 2025 arf20 (Ángel Ruiz Fernandez)

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

#ifndef _UTIL_H
#define _UTIL_H

#include <sys/socket.h>
#include <netinet/in.h>


/** \brief Send helper macro
 *
 * \param o Operation
 * \param a Error condition action
 * */
#define SOCK_TRY_SEND(o, a) \
    if (o < 0) { \
        ERROR("send(): %s", strerror(errno)); \
        a; \
    }

/** \brief Receive helper macro
 *
 * \param fd Socket
 * \param buff Receive buffer
 * \param type Typename to receive
 * \param action Error condition action
 */
#define SOCK_TRY_RECV(fd, buff, type, action) \
    toread = sizeof(type); \
    while (1) { \
        res = recv(fd, buff, toread, 0); \
        if (res < 0) { \
            ERROR("recv(): %s", strerror(errno)); \
            action; break; \
        } else if (res == 0) { \
            DEBUG("connection closed by peer"); \
            action; break; \
        } else if (res < sizeof(type)) { \
            buff += res; break; \
            toread -= res; \
            continue; \
        } \
        toread -= res; \
        buff += res; break; \
    }


void map_addr_inet_inet6(struct sockaddr_in6 *sin6,
    const struct sockaddr_in *sin);

/** \brief Normalize address string
 *
 * Converts IPv4 or IPv6 address string into IPv6-mapped-IPv4 or IPv6
 * sockaddr_in6
 */
int normalize_str_addr(struct sockaddr_in6 *sin6, const char *str);

const char *sockaddr_str(const struct sockaddr *sa);

const char *inaddr_str(uint32_t addr);
const char *sockaddr6_str(const struct sockaddr_in6 *sa);

#endif /* _UTIL_H */

