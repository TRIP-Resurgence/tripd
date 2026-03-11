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

#ifndef _TRIB_H
#define _TRIB_H

#include <protocol/protocol.h>

#include <stddef.h>


typedef struct {
    int af;
    int app_proto;
    char *prefix;
    char *nexthop;
} entry_t;

typedef struct {
    entry_t *table;
    size_t size;
    size_t capacity;
} route_table_t;

typedef struct {
    route_table_t local_routes;
} trib_t;


void trib_local_add(trib_t *trib, const entry_t *route);

trib_t *trib_new();
void trib_destroy(trib_t *trib);


#endif /* _TRIB_H */

