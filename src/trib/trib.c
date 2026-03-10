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

    trib.c: Telephony Routing Information Bases

*/

/** \file */

#include "trib.h"

#include <stdlib.h>

static trib_t g_trib = { 0 };


trib_t *
trib_new()
{
    trib_t *t = &g_trib;

    t->local_routes.capacity = 256;
    t->local_routes.size = 0;
    t->local_routes.table = malloc(t->local_routes.capacity * sizeof(route_t));

    return t;
}


void
trib_destroy(trib_t *trib)
{
    free(trib->local_routes.table);
}

