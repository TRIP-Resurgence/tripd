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
#include <functions/locator.h>

#include "pib.h"

#include <stddef.h>
#include <time.h>

#define ATTR_USED_NEXTHOP               0b1
#define ATTR_USED_ADVERTPATH            0b10
#define ATTR_USED_ROUTEDPATH            0b100
#define ATTR_USED_LOCALPREF             0b1000
#define ATTR_USED_METRIC                0b10000
#define ATTR_USED_COMMUNITIES           0b100000

#define ATTR_IS_USED_NEXTHOP(x)         (((x) >> 0) & 1)
#define ATTR_IS_USED_ADVERTPATH(x)      (((x) >> 1) & 1)
#define ATTR_IS_USED_ROUTEDPATH(x)      (((x) >> 2) & 1)
#define ATTR_IS_USED_LOCALPREF(x)       (((x) >> 3) & 1)
#define ATTR_IS_USED_METRIC(x)          (((x) >> 4) & 1)
#define ATTR_IS_USED_COMMUNITIES(x)     (((x) >> 5) & 1)


/** \brief Groupable attributes that are related to a route */
typedef struct {
    uint32_t    use;            /**< Bitfield flags specified attributes */
    int         withdrawn;      /**< WithdrawnRoutes or ReachableRoutes */
    uint32_t    nextitad;       /**< ITAD of next hop */
    char       *nexthop;        /**< Next hop server */
    uint32_t   *advertpath;     /**< AdvertisementPath */
    size_t      advertpath_size;
    uint32_t   *routedpath;     /**< RoutedPath */
    size_t      routedpath_size;
    int         atomicaggregate;/**< AtomicAggregate */
    uint32_t    local_pref;     /**< Degree of Preference */
    uint32_t    metric;         /**< MultiExitDisc */
    community_t*communities;    /**< Communities */
    size_t      communities_size;
    int         convertedroute; /**< ConvertedRoute, used when =1 */
} entry_attrs_t;

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
    const peer_t *learn_peer;   /**< Peer */

    int32_t     seq;            /**< Sequence number */
    time_t      time;           /**< Learn time */

    entry_attrs_t attrs;        /**< Attributes */
    
    int         sent;           /**< Route has been UPDATE'd to peer */
} entry_t;

/** \brief Route Table */
typedef struct {
    uint32_t    peer_itad, peer_id;      /**< Peer ITAD used in Adj-TRIBs */
    entry_t   **table;
    size_t      size, capacity;
    routemap_t *routemap;       /**< Insertion routemap */
} table_t;


typedef struct {
    entry_attrs_t attrs;        /**< Common attributes of group */
    entry_t   **entries;        /**< Array of references to entries on a table*/
    size_t      size, capacity;
} entry_group_t;

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

    table_t     loc_trib, optimized_loc_trib;

    table_t   **adj_tribs_in, **adj_tribs_out; /**< Owned by session */
    size_t      adj_tribs_capacity, adj_tribs_size;
    
    table_t     ext_trib;

    table_t     local_routes;
} trib_t;


/** \brief Clone entry */
entry_t *entry_clone(const entry_t *entry);
/** \brief Destroy entry */
void entry_destroy(entry_t *entry);

/** \brief Deinitialize table */
void trib_table_deinit(table_t *t);

/** \brief Initialize TRIB structure */
trib_t *trib_new(uint32_t local_itad);
/** \brief Add and init pair of tables owned by caller */
void trib_adj_pair_add(trib_t *trib, table_t *in, table_t *out);
void trib_adj_pair_remove(trib_t *trib, table_t *in, table_t *out);
/** \brief Deinit TRIB structure */
void trib_destroy(trib_t *trib);

/** \brief Find exact route in table */
entry_t **trib_table_find(table_t *t, uint16_t af, const char *prefix);

/** \brief Lookup address in table
 * \param table Table 
 * \param af Address family
 * \param app_proto Optional (=0) app_proto
 * \param address Address */
const entry_t *trib_table_lookup(const table_t *table, uint16_t af,
    uint16_t app_proto, const char *address);

/** \brief Add route to table */
void trib_table_insert(table_t *table, entry_t *route);

void trib_table_insert_or_replace(table_t *table, entry_t *route);

/** \brief Destroy all entries and clear table */
void trib_table_clear(table_t *table);


/** \brief Update local routes when ITAD is defined */
void trib_update_local(trib_t *trib);

/** \brief Update an Adj-TRIB-Out
 *
 * For use when a new peer connets and we have to UPDATE it without
 * triggering a full update
 */
void trib_update_adj_out(trib_t *trib, table_t *adj_trib_out);

/** \brief Execute route selection
 *
 * Takes Ext-TRIBs-in and locala routes
 * Updates Ext-TRIB, Loc-TRIB, optimized Loc-TRIB and Ext-TRIBs-out
 */
void trib_update_full(trib_t *trib);

/** \brief Return array of new entry references to new
 *
 * Allocates array of references and assigns it to new_ents_out
 *
 * \param table Table with new entries
 * \param new_ents_out Where to put entry references
 * \return Number of new entries
 */
size_t get_new_entries(const table_t *table, entry_t ***new_ents_out);

/** \brief Group array of entry references by attributes
 * 
 * \param entries Input array
 * \param entries_size Input array size
 * \param groups_out Output entry reference groups
 * \return Number of groups
 */
size_t group_entries_by_attrs(entry_t **entries, size_t entries_size,
    entry_group_t **groups_out);

#endif /* _TRIB_H */

