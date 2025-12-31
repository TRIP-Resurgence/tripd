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

#include <protocol/protocol.h>

#include <netinet/in.h>

/** \brief Send helper macro */
#define SOCK_TRY_SEND(o, a) \
    if (o < 0) { \
        ERROR("send(): %s", strerror(errno)); \
        a; \
    }

/** \brief Receive helper macro */
#define SOCK_TRY_RECV(fd, buff, type, action) \
    toread = sizeof(type); \
    while (1) { \
        res = recv(fd, buff, toread, 0); \
        if (res < 0) { \
            ERROR("recv(): %s", strerror(errno)); \
            action; break; \
        } else if (res == 0) { \
            DEBUG("connection closed by peer"); \
            action; break; \
        } else if (res < sizeof(type)) { \
            buff += res; break; \
            toread -= res; \
            continue; \
        } \
        toread -= res; \
        buff += res; break; \
    }



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
    pthread_t               thread;
    session_state_t         state;
    uint32_t                itad, id;
    uint16_t                hold;

    capinfo_transmode_t     transmode;

    struct sockaddr_in6    *addr;
    int                     fd;

    uint32_t                peer_itad, peer_id;
} session_t;



/** \brief Send notification helper */
int send_notification(int fd, int code, int subcode);

/** \brief LSID string */
const char *id_str(uint32_t id);

/** \brief Session loop */
void *session_loop(void *arg);

/** \brief Shutdown socket, terminate connection and thread */
void session_shutdown(session_t *session);

/** \brief Destroy session object */
void session_destroy(session_t *session);


#endif /* _SESSION_H */

