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

/** \file
 *
 * Telephony Routing Information Base, singleton
 */

#include "trib.h"

#include <stdlib.h>
#include <string.h>

static trib_t g_trib = { 0 };


void
trib_local_add(trib_t *trib, const entry_t *entry)
{
    if (trib->local_routes.capacity < trib->local_routes.size + 1) {
        trib->local_routes.capacity *= 2;
        trib->local_routes.table = realloc(trib->local_routes.table,
            trib->local_routes.capacity * sizeof(entry_t));
    }

    memcpy(&trib->local_routes.table[trib->local_routes.size++], entry,
        sizeof(entry_t));
}

trib_t *
trib_new()
{
    if (g_trib.local_routes.table)
        return NULL;

    trib_t *t = &g_trib;

    t->local_routes.capacity = 256;
    t->local_routes.size = 0;
    t->local_routes.table = malloc(t->local_routes.capacity * sizeof(entry_t));

    return t;
}

void
trib_destroy(trib_t *trib)
{
    free(trib->local_routes.table);
    g_trib.local_routes.table = NULL;
}

