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
 * \brief Session manager
 *
 * Listens for connections, owns peer locator and sessions, which are created
 * by this object
 */

#ifndef _MANAGER_H
#define _MANAGER_H

#include <netinet/in.h>

#include "session.h"
#include "locator.h"
#include <db/trib.h>
#include <db/pib.h>


/** \brief Manager object */
typedef struct {
    int         run;                /**< run threads = 1 */
    pthread_t   listen_thread;
    pthread_t   maintenance_thread;
    int         fd;

    /* Local */
    uint32_t    itad;
    uint32_t    id;

    /* Peer information */
    locator_t  *locator;

    /* Telephony Routing Information Base */
    trib_t     *trib;
    /* Policy Information Base */
    pib_t      *pib;

    /* Session instances */
    session_t **sessions;
    size_t      sessions_size, sessions_capacity;

    /* Timers peer default  */
    uint16_t    hold;
    uint16_t    keepalive;
    /* Timers per trip instance */
    int         connect_retry;
    int         max_purge_time;
    int         disable_time;
    int         min_itad_orig_int;
    int         min_route_advert_int;
} manager_t;


/** \brief Lookup session by locator peer */
session_t *manager_session_lookup_address(const manager_t *m,
    const struct sockaddr_in6 *addr);

/** \brief Create manager and bind socket */
manager_t *manager_new(const struct sockaddr_in6 *listen_addr);

/** \brief Add known peer to underlaying locator */
void manager_peer_add(manager_t *manager, const struct sockaddr_in6 *addr,
    uint32_t itad);

/** \brief Find known peer by address */
peer_t *manager_peer_find(manager_t *manager, const struct sockaddr_in6 *addr);

/** \brief Run accept loop in thread */
void manager_run(manager_t *manager);

/** \brief Stop accept loop */
void manager_stop(manager_t *manager);

/** \brief Shut down manager and all sessions */
void manager_shutdown(manager_t *manager);

/** \brief Destroy manager object */
void manager_destroy(manager_t *manager);


#endif /* _MANAGER_H */

