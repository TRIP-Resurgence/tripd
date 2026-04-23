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
#include "protocol/protocol.h"

#define _COMPONENT_ "db"

#include <logging/logging.h>

#include <stdlib.h>
#include <string.h>

#define INIT_TABLE_CAPACITY 256
#define INIT_ADJ_TRIBS_CAPACITY 32

static trib_t g_trib = { 0 };


entry_t *
entry_clone(const entry_t *entry)
{
    entry_t *ne = malloc(sizeof(entry_t));
    /* shallow copy */
    *ne = *entry;
    /* deep copy */
    ne->attrs = entry->attrs;
    ne->prefix = strdup(entry->prefix);
    ne->attrs.nexthop = strdup(entry->attrs.nexthop);
    if (ATTR_IS_USED_ADVERTPATH(entry->attrs.use)) {
        ne->attrs.advertpath = malloc(sizeof(uint32_t)
            * entry->attrs.advertpath_size);
        memcpy(ne->attrs.advertpath, entry->attrs.advertpath, sizeof(uint32_t)
            * entry->attrs.advertpath_size);
    } else ne->attrs.advertpath = NULL;
    if (ATTR_IS_USED_ROUTEDPATH(entry->attrs.use)) {
        ne->attrs.routedpath = malloc(sizeof(uint32_t)
            * entry->attrs.routedpath_size);
        memcpy(ne->attrs.routedpath, entry->attrs.routedpath, sizeof(uint32_t)
            * entry->attrs.routedpath_size);
    } else ne->attrs.routedpath = NULL;
    if (ATTR_IS_USED_COMMUNITIES(entry->attrs.use)) {
        ne->attrs.communities = malloc(sizeof(uint32_t)
            * entry->attrs.communities_size);
        memcpy(ne->attrs.communities, entry->attrs.communities,
            sizeof(community_t) * entry->attrs.communities_size);
    } else ne->attrs.communities = NULL;
    return ne;
}

void
entry_destroy(entry_t *entry)
{
    free(entry->prefix);
    free(entry->attrs.nexthop);
    free(entry->attrs.advertpath);
    free(entry->attrs.routedpath);
    free(entry);
}

static void
table_init(table_t *t)
{
    t->capacity = INIT_TABLE_CAPACITY;
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

    t->adj_tribs_capacity = INIT_ADJ_TRIBS_CAPACITY;
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
            table->capacity * sizeof(entry_t*));
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
 * \return 1 if e2 preferred over e1, 0 otherwise
 */
static int
entry_compare(const entry_t *e1, const entry_t *e2, uint32_t itad)
{
    if (e1->attrs.local_pref != e2->attrs.local_pref)
        return e1->attrs.local_pref < e2->attrs.local_pref;
    if (e1->type != e2->type)
        return e1->type < e2->type;
    if (e1->attrs.routedpath_size != e2->attrs.routedpath_size)
        return e1->attrs.routedpath_size < e2->attrs.routedpath_size;
    if (e1->attrs.metric != e2->attrs.metric)
        return e1->attrs.metric < e2->attrs.metric;
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

        /* replace entry */
        if (entry_compare(*match, t2->table[i], itad)) {
            entry_destroy(*match);
            *match = entry_clone(t2->table[i]);
        }
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
        const routemap_statement_t *s =
            routemap_match(policy, src->table[i]->prefix);
        if (!s)
            continue;   /* default deny */
        
        entry_t *e = entry_clone(src->table[i]);

        for (size_t j = 0; j < s->actions_size; j++) {
            switch (s->actions[j].attribute) {
            case ROUTEMAP_SET_LOCALPREF:
            case ROUTEMAP_SET_METRIC:
                e->attrs.local_pref = s->actions[j].value;
                break;
            case ROUTEMAP_SET_NEXTHOP:
                free(e->attrs.nexthop);
                e->attrs.nexthop = strdup(s->actions[j].valstr2);
                break;
            case ROUTEMAP_SET_ITADPATH_PREPEND:
                e->attrs.routedpath_size = s->actions[j].value
                    + e->attrs.routedpath_size;
                e->attrs.routedpath = realloc(e->attrs.routedpath,
                    e->attrs.routedpath_size * sizeof(uint32_t));
                for (size_t k = 0; k < s->actions[j].value; k++)
                    e->attrs.routedpath[k] = local_itad;
                break;
            }
        }

        trib_table_insert(dst, e);
    }
}


void
trib_update_adj_out(trib_t *trib, table_t *adj_trib_out)
{
    adj_trib_out->size = 0;
    /* apply output policy if applicable */
    if (adj_trib_out->routemap)
        apply_policy(adj_trib_out, &trib->optimized_loc_trib, trib->local_itad,
            adj_trib_out->routemap);
    else
        table_copy(adj_trib_out, &trib->optimized_loc_trib);
}

void
trib_update_full(trib_t *trib)
{
    trib->ext_trib.size = 0;
    trib->loc_trib.size = 0;
    
    /* Phase 2a: local routes and external Ext-TRIBs-in to Ext-TRIB */
    table_t scratch; /* temporary working table */
    table_init(&scratch);

    table_select_into(&trib->ext_trib, &trib->local_routes, trib->local_itad);
    for (size_t i = 0; i < trib->adj_tribs_size; i++) {
        if (trib->adj_tribs_in[i].peer_itad != trib->local_itad) {
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
    /* optimize table */
    optimize_table(&trib->optimized_loc_trib, &trib->loc_trib);

    for (size_t i = 0; i < trib->adj_tribs_size; i++)
        trib_update_adj_out(trib, &trib->adj_tribs_out[i]);

    trib_table_deinit(&scratch);
}


size_t
get_new_entries(table_t *table, entry_t ***new_ents_out)
{
    size_t new_ents_size = 0, new_ents_capacity = INIT_TABLE_CAPACITY;
    entry_t **new_ents = malloc(sizeof(entry_t*) * new_ents_capacity);

    for (size_t i = 0; i < table->size; i++) {
        if (table->table[i]->sent)
            continue;

        if (new_ents_size + 1 > new_ents_capacity) {
            new_ents_capacity *= 2;
            new_ents = realloc(new_ents, sizeof(entry_t*) * new_ents_capacity);
        }

        new_ents[new_ents_size++] = table->table[i];
    }

    *new_ents_out = new_ents;
    return new_ents_size;
}

/** \brief Deep compare entry attributes */
static int
attrs_equals(const entry_attrs_t *a1, const entry_attrs_t *a2)
{
    /* group all withdrawn together */
    if (a1->withdrawn && a2->withdrawn)
        return 1;
    if (a1->withdrawn || a2->withdrawn)
        return 0;

    if (!ATTR_IS_USED_NEXTHOP(a1->use) || !ATTR_IS_USED_NEXTHOP(a2->use)) {
        ERROR("reachable route compared without nexthop");
        return 0;
    }

    return
        (strcmp(a1->nexthop, a2->nexthop) == 0) &&
        ((!ATTR_IS_USED_ADVERTPATH(a1->use) || !ATTR_IS_USED_ADVERTPATH(a2->use)) ||
            (a1->advertpath_size == a2->advertpath_size &&
                memcmp(a1->advertpath, a2->advertpath,
                    sizeof(uint32_t) * a1->advertpath_size))) &&
        ((!ATTR_IS_USED_ROUTEDPATH(a1->use) || !ATTR_IS_USED_ROUTEDPATH(a2->use)) ||
            (a1->routedpath_size == a2->routedpath_size &&
                memcmp(a1->routedpath, a2->routedpath,
                    sizeof(uint32_t) * a1->routedpath_size))) &&
        ((!ATTR_IS_USED_LOCALPREF(a1->use) || !ATTR_IS_USED_LOCALPREF(a2->use)) ||
            (a1->local_pref == a2->local_pref)) &&
        ((!ATTR_IS_USED_METRIC(a1->use) || !ATTR_IS_USED_METRIC(a2->use)) ||
            (a1->metric == a2->metric)) &&
        ((!ATTR_IS_USED_COMMUNITIES(a1->use) || !ATTR_IS_USED_COMMUNITIES(a2->use)) ||
            (a1->communities_size == a2->communities_size &&
                memcmp(a1->communities, a2->communities,
                    sizeof(uint32_t) * a1->communities_size)));
}

static entry_group_t *
is_entry_in_group(const entry_t *e, entry_group_t *groups,
    size_t groups_size)
{
    for (size_t i = 0; i < groups_size; i++)
        if (attrs_equals(&e->attrs, &groups[i].attrs))
            return &groups[i];
    return NULL;
}

static void
group_init(entry_group_t *g, const entry_attrs_t *attrs)
{
    g->capacity = INIT_TABLE_CAPACITY;
    g->size = 0;
    g->entries = malloc(sizeof(entry_t*) * g->capacity);
    g->attrs = *attrs;
}

static void
group_insert(entry_group_t *g, entry_t *e)
{
    if (g->size + 1 > g->capacity) {
        g->capacity *= 2;
        g->entries = realloc(g->entries,
            sizeof(entry_t*) * g->capacity);
    }

    g->entries[g->size++] = e;
}

size_t
group_entries_by_attrs(entry_t **entries, size_t entry_size,
    entry_group_t **groups_out)
{

    size_t groups_size = 0, groups_capacity = entry_size;
    entry_group_t *groups = malloc(sizeof(entry_group_t) * groups_capacity);

    for (size_t i = 0; i < entry_size; i++) {
        entry_group_t *ingroup = is_entry_in_group(entries[i], groups,
            groups_size);

        if (!ingroup) {
            if (groups_size + 1 > groups_capacity) {
                groups_capacity *= 2;
                groups = realloc(groups, sizeof(entry_group_t) * groups_capacity);
            }

            ingroup = &groups[groups_size++];
            group_init(ingroup, &entries[i]->attrs);
        }

        group_insert(ingroup, entries[i]);
    }


    *groups_out = groups;
    return groups_size;
}

