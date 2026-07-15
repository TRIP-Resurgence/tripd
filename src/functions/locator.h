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
 * \brief Peer locator
 *
 * Holds information about known configured peers
 */

#ifndef _LOCATOR_H
#define _LOCATOR_H

#include <protocol/protocol.h>
#include <db/pib.h>

#include <netinet/in.h>


/** \brief Timers */
typedef struct {
    uint16_t                hold;
    uint16_t                keepalive;
    int                     connect_retry;
    int                     max_purge_time;
    int                     disable_time;
    int                     min_itad_orig_int;
    int                     min_route_advert_int;
} timers_t;

/** \brief Known peer info object */
typedef struct {
    struct sockaddr_in6     addr;
    
    uint32_t                itad;
    capinfo_transmode_t     transmode;

    /* timers */
    timers_t                timers;

    /* policy */
    routemap_t             *routemap_in, *routemap_out;
} peer_t;

/** \brief Peer locator */
typedef struct {
    peer_t     *peers;
    size_t      peers_size, peers_capacity;
} locator_t;


/** \brief Initialize singleton locator known peer list */
locator_t *locator_new();

/** \brief Add a known peer */
peer_t *locator_add(locator_t *locator, const struct sockaddr_in6 *addr,
    uint32_t itad, const timers_t *timers, capinfo_transmode_t transmode);

/** \brief Lookup peer by its address */
peer_t *locator_lookup(locator_t *locator,
    const struct sockaddr_in6 *addr);

/** \brief Destroy locator object */
void locator_destroy(locator_t *locator);


#endif /* _LOCATOR_H */

