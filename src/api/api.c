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

#include <api/http_status.h>
#include <logging/logging.h>
#include <util/util.h>

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define _COMPONENT_ "api"

void
handle_query(int fd, char *buf, ssize_t req_len)
{
    char sendbuf[4096];
    size_t sendsize = snprintf(sendbuf, 4096, "%s", STATUS_200);
    SOCK_TRY_SEND(send(fd, sendbuf, sendsize, 0), return);
}

const endpoint_t api[] = {
    { "/query", &handle_query },
    { NULL, NULL }
};

