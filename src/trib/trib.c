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
trib_table_add(table_t *table, const entry_t *entry)
{
    if (table->capacity < table->size + 1) {
        table->capacity *= 2;
        table->table = realloc(table->table,
            table->capacity * sizeof(entry_t));
    }

    memcpy(&table->table[table->size++], entry,
        sizeof(entry_t));
}


static void
table_init(table_t *t)
{
    t->capacity = 256;
    t->size = 0;
    t->table = malloc(t->capacity * sizeof(entry_t));
}

void
trib_table_deinit(table_t *t)
{
    for (int i = 0; i < t->size; i++) {
        free(t->table[i].prefix);
        free(t->table[i].nexthop);
    }
    free(t->table);
}

trib_t *
trib_new()
{
    if (g_trib.local_routes.table)
        return NULL;

    trib_t *t = &g_trib;

    table_init(&t->local_routes);
    table_init(&t->ext_trib);
    table_init(&t->loc_trib);

    t->adj_tribs_capacity = 256;
    t->adj_tribs_size = 0;
    t->adj_tribs_in = malloc(t->adj_tribs_capacity * sizeof(table_t));
    t->adj_tribs_out = malloc(t->adj_tribs_capacity * sizeof(table_t));

    return t;
}

void
trib_adj_pair_new(trib_t *trib, table_t **in, table_t **out)
{
    if (trib->adj_tribs_capacity < trib->adj_tribs_size + 1) {
        trib->adj_tribs_capacity *= 2;
        trib->adj_tribs_in = realloc(trib->adj_tribs_in,
            trib->adj_tribs_capacity * sizeof(table_t));
        trib->adj_tribs_out = realloc(trib->adj_tribs_out,
            trib->adj_tribs_capacity * sizeof(table_t));
    }

    *in = &trib->adj_tribs_in[trib->adj_tribs_size];
    *out = &trib->adj_tribs_out[trib->adj_tribs_size];
    trib->adj_tribs_size++;

    table_init(*in);
    table_init(*out);
}

void
trib_destroy(trib_t *trib)
{
    trib_table_deinit(&trib->local_routes);
    trib_table_deinit(&trib->ext_trib);
    trib_table_deinit(&trib->loc_trib);
    free(trib->adj_tribs_in);
    free(trib->adj_tribs_out);
}

