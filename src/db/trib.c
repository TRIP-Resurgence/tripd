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
#include "db/pib.h"

#define _COMPONENT_ "db"

#include <logging/logging.h>

#include <stdlib.h>
#include <string.h>

static trib_t g_trib = { 0 };


entry_t *
entry_new(uint16_t af, uint16_t app_proto, const char *prefix,
    const char *nexthop, uint32_t seq, time_t time, uint32_t local_pref,
    uint32_t metric)
{
    entry_t *e = malloc(sizeof(entry_t));
    e->af = af;
    e->app_proto = app_proto;
    e->prefix = strdup(prefix);
    e->nexthop = strdup(nexthop);
    e->seq = seq;
    e->time = time;
    e->local_pref = local_pref;
    e->metric = metric;
    e->withdrawn = 0;
    return e;
}

entry_t *
entry_clone(const entry_t *entry)
{
    entry_t *ne = malloc(sizeof(entry_t));
    memcpy(ne, entry, sizeof(entry_t));
    if (entry->itad_path_size) {
        ne->itad_path = malloc(sizeof(uint32_t) * entry->itad_path_size);
        memcpy(ne->itad_path, entry->itad_path, sizeof(uint32_t)
            * entry->itad_path_size);
    }
    ne->prefix = strdup(entry->prefix);
    ne->nexthop = strdup(entry->nexthop);
    return ne;
}

void
entry_destroy(entry_t *entry)
{
    free(entry->prefix);
    free(entry->nexthop);
    free(entry);
}

static void
table_init(table_t *t)
{
    t->capacity = 256;
    t->size = 0;
    t->table = malloc(t->capacity * sizeof(entry_t*));
}

static void
table_copy(table_t *dst, table_t *src)
{
    for (size_t i = 0; i < src->size; i++)
        trib_table_insert(dst, entry_clone(src->table[i]));
}

void
trib_table_deinit(table_t *t)
{
    for (size_t i = 0; i < t->size; i++)
        entry_destroy(t->table[i]);
    free(t->table);
}

trib_t *
trib_new(uint32_t local_itad)
{
    if (g_trib.local_routes.table)
        return NULL;

    trib_t *t = &g_trib;

    t->local_itad = local_itad;

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


void
trib_table_insert(table_t *table, entry_t *entry)
{
    if (table->capacity < table->size + 1) {
        table->capacity *= 2;
        table->table = realloc(table->table,
            table->capacity * sizeof(entry_t));
    }

    table->table[table->size++] = entry;
}


/** \brief Compare two entries with selection algorithm
 *
 * 1. Highest degree of preference (local pref)
 * 2. Originated by local LS first
 * 3. Shortest ITAD-path
 * 4. Highest MED
 * 5. eTRIP over iTRIP (external peer route before internal peer route)
 * 6. Oldest route
 * 7. Highest LS ID
 *
 * \return e1 < e2
 */
static int
entry_compare(const entry_t *e1, const entry_t *e2, uint32_t itad)
{
    if (e1->local_pref != e2->local_pref)
        return e1->local_pref < e2->local_pref;
    if (e1->type != e2->type)
        return e1->type < e2->type;
    if (e1->itad_path_size != e2->itad_path_size)
        return e1->itad_path_size < e2->itad_path_size;
    if (e1->metric != e2->metric)
        return e1->metric < e2->metric;
    if ((e1->learn_itad == itad) != (e2->learn_itad == itad))
        return e2->learn_itad == itad;
    if (e1->time != e2->time)
        return e1->time > e2->time;
    if (e1->learn_lsid != e2->learn_lsid)
        return e1->learn_lsid < e2->learn_lsid;
    INFO("identical route preference for %s", e1->prefix);
    return 0;
}

static entry_t **
table_find(table_t *t, uint16_t af, const char *prefix)
{
    for (size_t i = 0; i < t->size; i++)
        if (t->table[i]->af == af && strcmp(t->table[i]->prefix, prefix) == 0)
            return &t->table[i];
    return NULL;
}

/** \brief Execute route selection from table into another
 *
 * A route that exists in t2 that doesn't exist in t1 is pushed into t1,
 * a route that exists in both t1 and t2 for the same destination are compared
 * with the route selection algorithm and either kept or replaced by the t2 route
 *
 * \param t1 Base table
 * \param t2 Comparing table
 */
static void
table_select_into(table_t *t1, table_t *t2, uint32_t itad)
{
    for (size_t i = 0; i < t2->size; i++) {
        entry_t **match = table_find(t1, t2->table[i]->af, t2->table[i]->prefix);
        if (!match) {
            trib_table_insert(t1, entry_clone(t2->table[i]));
            continue;
        }

        if (entry_compare(*match, t2->table[i], itad))
            *match = entry_clone(t2->table[i]);
    }
}

/** \brief Apply information reduction and route aggregation */
static void
optimize_table(table_t *dst, table_t *src)
{
    for (size_t i = 0; i < src->size; i++) {
        /* TODO: */
        trib_table_insert(dst, entry_clone(src->table[i]));
    }
}

/** \brief Copy routes applying a policy */
static void
apply_policy(table_t *dst, const table_t *src, uint32_t local_itad,
    const routemap_t *policy)
{
    for (size_t i = 0; i < src->size; i++) {
        entry_t *e = entry_clone(src->table[i]);

        const routemap_statement_t *s =
            routemap_match(policy, src->table[i]->prefix);
        if (!s)
            continue;   /* default deny */

        for (size_t j = 0; j < s->actions_size; j++) {
            switch (s->actions[j].attribute) {
            case ROUTEMAP_SET_LOCALPREF:
            case ROUTEMAP_SET_METRIC:
                e->local_pref = s->actions[j].value;
                break;
            case ROUTEMAP_SET_NEXTHOP:
                e->nexthop = strdup(s->actions[j].valstr2);
                break;
            case ROUTEMAP_SET_ITADPATH_PREPEND:
                e->itad_path_size = s->actions[j].value + e->itad_path_size;
                e->itad_path = realloc(e->itad_path,
                    e->itad_path_size * sizeof(uint32_t));
                for (size_t k = 0; k < s->actions[j].value; k++)
                    e->itad_path[k] = local_itad;
                break;
            }
        }

        trib_table_insert(dst, e);
    }
}


void
trib_update(trib_t *trib)
{
    trib->ext_trib.size = 0;
    trib->loc_trib.size = 0;
    
    /* Phase 2a: local routes and external Ext-TRIBs-in to Ext-TRIB */
    table_t scratch;
    table_init(&scratch);

    table_select_into(&trib->ext_trib, &trib->local_routes, trib->local_itad);
    for (size_t i = 0; i < trib->adj_tribs_size; i++) {
        if (trib->adj_tribs_in[i].peer_itad != trib->local_itad) {
            scratch.size = 0;
            /* apply input policy if applicable */
            if (trib->adj_tribs_in[i].routemap) {
                scratch.size = 0;
                apply_policy(&scratch, &trib->adj_tribs_in[i], trib->local_itad,
                    trib->adj_tribs_in[i].routemap);
                table_select_into(&trib->ext_trib, &scratch, trib->local_itad);
            } else
                table_select_into(&trib->ext_trib, &trib->adj_tribs_in[i],
                    trib->local_itad);
        }
    }

    /* Phase 2b: Ext-TRIB and internal Ext-TRIBs-in to Loc-TRIB */
    table_select_into(&trib->loc_trib, &trib->ext_trib, trib->local_itad);
    for (size_t i = 0; i < trib->adj_tribs_size; i++)
        if (trib->adj_tribs_in[i].peer_itad == trib->local_itad)
            table_select_into(&trib->loc_trib, &trib->adj_tribs_in[i],
                trib->local_itad);

    /* Phase 3: Loc-TRIB to Ext-TRIBs-Out */
    optimize_table(&scratch, &trib->loc_trib); /* optimize table */

    for (size_t i = 0; i < trib->adj_tribs_size; i++) {
        trib->adj_tribs_out[i].size = 0;
        /* apply output policy if applicable */
        if (trib->adj_tribs_out[i].routemap)
            apply_policy(&trib->adj_tribs_out[i], &scratch, trib->local_itad,
                trib->adj_tribs_out[i].routemap);
        else
            table_copy(&trib->adj_tribs_out[i], &scratch);
    }

    trib_table_deinit(&scratch);
}

