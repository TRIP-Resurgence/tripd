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
 * \brief Telephony Routing Information Base
 */

#ifndef _TRIB_H
#define _TRIB_H

#include <protocol/protocol.h>

#include "pib.h"

#include <stddef.h>
#include <time.h>


typedef enum {
    ENTRY_TYPE_TRIP,
    ENTRY_TYPE_CONNECTED,
    ENTRY_TYPE_STATIC
} entry_type_t;

/** \brief Route Entry */
typedef struct {
    /* route */
    uint16_t    af;             /**< Address family */
    uint16_t    app_proto;      /**< Application protocol */
    char       *prefix;         /**< Route prefix (address) */

    entry_type_t type;          /**< Route type */

    uint32_t    origin_itad;    /**< ITAD routed originated from */
    
    /* learned from */
    uint32_t    learn_itad;     /**< Peer ITAD for internal or external used for
                                    Ext-TRIB and Loc-TRIB */
    uint32_t    learn_lsid;     /**< Peer LS ID */

    uint32_t    seq;            /**< Sequence number */
    time_t      time;           /**< Learn time */

    /* attributes */
    char       *nexthop;        /**< Next hop server */

    uint32_t    local_pref;     /**< Degree of Preference */
    uint32_t    metric;         /**< MultiExitDisc */
    uint32_t   *itad_path;      /**< RoutedPath */
    size_t      itad_path_size;

    int         withdrawn;      /**< Mark as withdrawn */
} entry_t;

/** \brief Route Table */
typedef struct {
    uint32_t    peer_itad;      /**< Peer ITAD used in Adj-TRIBs */
    entry_t   **table;
    size_t      size, capacity;
    routemap_t *routemap;       /**< Insertion routemap */
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
    uint32_t    local_itad;   /**< local LS ITAD */

    table_t     loc_trib;

    table_t    *adj_tribs_in, *adj_tribs_out;
    size_t      adj_tribs_capacity, adj_tribs_size;
    
    table_t     ext_trib;

    table_t     local_routes;

    table_t     opt_trib; /* optimized Loc-TRIB */
} trib_t;


/** \brief New entry
 *
 * Not marked withdrawned
 */
entry_t *entry_new(uint16_t af, uint16_t app_proto, const char *prefix,
    const char *nexthop, uint32_t seq, time_t time, uint32_t local_pref,
    uint32_t metric);

/** \brief Destroy entry */
void entry_destroy(entry_t *entry);

/** \brief Deinitialize table */
void trib_table_deinit(table_t *t);

/** \brief Initialize TRIB structure */
trib_t *trib_new(uint32_t local_itad);
/** \brief Add and init pair of tables in Adj-TRIBs-* vector */
void trib_adj_pair_new(trib_t *trib, table_t **in, table_t **out);
/** \brief Deinit TRIB structure */
void trib_destroy(trib_t *trib);

/** \brief Add route to table */
void trib_table_insert(table_t *table, entry_t *route);

/** \brief Execute route selection
 *
 * Takes Ext-TRIBs-in and locala routes
 * Updates Ext-TRIB, Loc-TRIB and Ext-TRIBs-out
 */
void trib_update(trib_t *trib);

#endif /* _TRIB_H */

