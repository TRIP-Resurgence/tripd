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

/** \file
 * \brief Telephony Routing Information Base */

#ifndef _TRIB_H
#define _TRIB_H

#include <protocol/protocol.h>

#include <stddef.h>


/** \brief Route Entry */
typedef struct {
    int     af;
    int     app_proto;
    char   *prefix;
    char   *nexthop;
} entry_t;

/** \brief Route Table */
typedef struct {
    entry_t    *table;
    size_t      size;
    size_t      capacity;
    uint32_t    itad;           /**< internal or external */
} table_t;

/** \brief Telephony Routing Information Base
 *
 * Collection of route tables
 *
 * ```
 *                         Loc-TRIB
 *                             ^
 *                             |
 *                     Decision Process
 *                      ^      ^      |
 *                      |      |      |
 *             Adj-TRIBs-In    |      V
 *            (Internal LSs)   |   Adj-TRIBs-Out
 *                             |
 *                             |
 *                             |
 *                          Ext-TRIB
 *                         ^        ^
 *                         |        |
 *                Adj-TRIB-In      Local Routes
 *            (External Peers)
 * ```
 */
typedef struct {
    table_t     loc_trib;

    table_t    *adj_tribs_in, *adj_tribs_out;
    size_t      adj_tribs_capacity, adj_tribs_size;
    
    table_t     ext_trib;

    table_t     local_routes;
} trib_t;


void trib_table_add(table_t *table, const entry_t *route);

void trib_table_deinit(table_t *t);

trib_t *trib_new();
void trib_adj_pair_new(trib_t *trib, table_t **in, table_t **out);
void trib_destroy(trib_t *trib);


#endif /* _TRIB_H */

