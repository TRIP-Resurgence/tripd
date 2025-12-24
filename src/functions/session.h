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

#ifndef _SESSION_H
#define _SESSION_H

#include <protocol/protocol.h>

#include <netinet/in.h>


typedef enum {
    STATE_IDLE,
    STATE_CONNECT,
    STATE_ACTIVE,
    STATE_OPENSENT,
    STATE_OPENCONFIRM,
    STATE_ESTABLISHED
} session_state_t;

typedef struct {
    pthread_t           session_thread;
    void               *session_buff;
    session_state_t     session_state;
    uint32_t            session_itad, session_id;
    uint16_t            session_hold;

    time_t              session_connect_retry;

    capinfo_transmode_t session_transmode;

    struct sockaddr_in6 session_peer_addr;
    int                 session_fd;

    uint32_t            session_peer_itad, session_peer_id;
} session_t;


/* sessions start with no data exchanged yet */

/* initiate connection to peer */
session_t *session_new_initiate(uint32_t itad, uint32_t id, uint16_t hold,
    capinfo_transmode_t transmode, const struct sockaddr_in6 *peer_addr,
    uint32_t peer_itad);

/* connection request received from peer */
session_t *session_new_peer(uint32_t itad, uint32_t id, uint16_t hold,
    capinfo_transmode_t transmode, const struct sockaddr_in6 *peer_addr,
    int fd);

session_state_t session_get_state(const session_t *session);

void session_destroy(session_t *session);


#endif /* _SESSION_H */

