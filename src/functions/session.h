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
 * \brief Session logic
 *
 * Logic for session communication, receive loop thread
 */

#ifndef _SESSION_H
#define _SESSION_H

#include "locator.h"
#include <protocol/protocol.h>
#include <util/util.h>
#include <db/trib.h>

#include <netinet/in.h>




/** \brief Session states */
typedef enum {
    STATE_IDLE,
    STATE_CONNECT,
    STATE_ACTIVE,
    STATE_OPENSENT,
    STATE_OPENCONFIRM,
    STATE_ESTABLISHED
} session_state_t;

/** \brief Session state strings */
extern const char *session_state_strs[];

/** \brief Session object */
typedef struct {
    pthread_t               thread;     /**< Session thread ID */
    session_state_t         state;      /**< Session state */
    int                     initiated;  /**< Initiated by local -> nonzero */
    int                     mark_stop_init; /**< Tell initiating thread to quit*/

    int                     fd;         /**< Session socket */

    /* negotiated */
    uint16_t                hold;       /**< Negotiated hold timer */
    uint16_t                keepalive;  /**< Negotiated hold timer */


    const peer_t           *peer;       /**< From address */
    uint32_t                id;         /**< Found in OPEN */

    /* times */
    time_t                  state_time;         /**< Time since entered state */
    time_t                  last_read_time;     /**< Time of last read */
    time_t                  last_write_time;    /**< Time of last write */

    time_t                  last_orig_time;     /**< Last origination time */
    time_t                  last_advert_time;   /**< Last advertisement time */

    /* capabilities */
    capinfo_transmode_t     transmode;        /**< Peer transmode */
    capinfo_routetype_t    *routetypes;       /**< Supported route types */
    size_t                  routetypes_count; /**< Supported route types count*/

    /* adj tables */
    table_t                *adj_trib_in;    /**< Adj-TRIB-in */
    table_t                *adj_trib_out;   /**< Adj-TRIB-out */
} session_t;


/** \brief Change session state */
void session_change_state(session_t *s, session_state_t new_state);

/** \brief Send notification helper */
int send_notification(int fd, int code, int subcode);

/** \brief LSID string */
const char *id_str(uint32_t id);

/** \brief Session loop */
void *session_loop(void *arg);

/** \brief Update session
 *
 * Send UPDATEs to peer according to new entries in Adj-TRIB-Out
 */
void update_session(const session_t *s, uint32_t local_id, uint32_t local_itad);

/** \brief Shutdown socket, terminate connection and thread */
void session_shutdown(session_t *session);

/** \brief Destroy session object */
void session_destroy(session_t *session);


#endif /* _SESSION_H */

