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

    util.c: misc utilities

*/

#include "util.h"

#include <string.h>

void
map_addr_inet_inet6(struct sockaddr_in6 *sin6, const struct sockaddr_in *sin)
{
    memset(&sin6->sin6_addr.s6_addr[0], 0x00, 10);  /* 80 0s */
    memset(&sin6->sin6_addr.s6_addr[10], 0xff, 2);  /* 16 1s */
    memcpy(&sin6->sin6_addr.s6_addr[12],            /* 32 IPv4 addr */
        &sin->sin_addr.s_addr,
        sizeof(in_addr_t));
}

