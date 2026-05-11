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

    api.c: implement api endpoints

*/

/** \file
 * Implements API endpoints
 */

#include "api.h"
#include "protocol/protocol.h"

#include <api/http_status.h>
#include <logging/logging.h>
#include <util/util.h>

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define _COMPONENT_ "api"

int
handle_query(int fd, char *query, char *buf, ssize_t req_len, trib_t *trib)
{
    char sendbuf[4096], body[4096];

    const entry_t *e = trib_table_lookup(&trib->loc_trib, AF_E164, 0, query);
    if (!e) {
        SOCK_TRY_SEND(send(fd, STATUS_404, sizeof(STATUS_404), 0), return -1);
        return 404;
    }

    size_t bodysize = snprintf(body, 4096, "%s\r\n", e->attrs.nexthop);

    size_t sendsize = snprintf(sendbuf, 4096, "%sContent-Length: %ld\r\n\r\n%s",
        STATUS_200, bodysize, body);

    SOCK_TRY_SEND(send(fd, sendbuf, sendsize, 0), return -1);
    return 200;
}

const endpoint_t api[] = {
    { "/query/", &handle_query },
    { NULL, NULL }
};

