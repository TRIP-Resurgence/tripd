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
 * \brief API endpoints
 */

#ifndef _API_H
#define _API_H

#include <db/trib.h>

#include <sys/types.h>

typedef int(*endpoint_handler_t)(int, char *, char *, ssize_t, trib_t *);

typedef struct {
    const char *endpoint;
    endpoint_handler_t handler;
} endpoint_t;

extern const endpoint_t api[];

#endif /* _API_H */

