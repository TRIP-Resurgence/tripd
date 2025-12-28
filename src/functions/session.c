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

    ls_peer_session.c: peer session logic

*/

/** \file */

#include "session.h"

#include <logging/logging.h>
#include <util/util.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <unistd.h>
#include <pthread.h>

#include <arpa/inet.h>

#define _COMPONENT_ "session"

#define SOCK_TRY_SEND(o, a) \
    if (o < 0) { \
        ERROR("send(): %s", strerror(errno)); \
        a; \
    }

#define SOCK_TRY_RECV(fd, buff, type, action) \
    while (1) { \
        res = recv(fd, buff, sizeof(type), 0); \
        if (res < 0) { \
            ERROR("recv(): %s", strerror(errno)); \
            action; break; \
        } else if (res == 0) { \
            action; break; \
        } else if (res < sizeof(type)) { \
            continue; \
        } \
        buff += res; break; \
    }


const char *session_state_strs[] = {
    "idle",
    "connect",
    "active",
    "opensent",
    "openconfirm",
    "established"
};

static const char *
session_str(session_t *s)
{
    static char str[256], abuff[INET6_ADDRSTRLEN], abuff2[INET_ADDRSTRLEN];
    snprintf(str, 256, "(%s):%d:%s",
        inet_ntop(AF_INET6, &s->session_peer_addr.sin6_addr, abuff,
            sizeof(abuff)),
        s->session_peer_itad,
        inet_ntop(AF_INET, &s->session_peer_id, abuff2, sizeof(abuff2)));
    return str;
}

static void
session_change_state(session_t *s, session_state_t new_state)
{
    DEBUG("peer session %s changed state from %s to %s", session_str(s),
        session_state_strs[s->session_state], session_state_strs[new_state]);
    s->session_state = new_state;
}

static void *
session_loop(void *arg)
{
    session_t *s = arg;

    int r = 0;

    /* send OPEN */
    PROTO_TRY(
        new_msg_open(s->session_buff, MAX_MSG_SIZE,
            s->session_hold, s->session_itad, s->session_id,
            supported_routetypes, supported_routetypes_size,
            s->session_transmode),
        goto proto_error
    );

    SOCK_TRY_SEND(
        send(s->session_fd, s->session_buff, r, 0) < 0,
        goto sock_error
    );
    session_change_state(s, STATE_OPENSENT);

    int res = 0;
    while (1) {
        void *recv_wnd = s->session_buff;
        /* receive message */
        SOCK_TRY_RECV(s->session_fd, recv_wnd, msg_t, goto sock_error);

        const msg_t *msg = NULL;
        PROTO_TRY(
            parse_msg(s->session_buff, r, &msg),
            goto proto_error
        );

        DEBUG("received msg: %d[%d]", msg->msg_type, msg->msg_len);

        /* continue */
    }

proto_error:
    uint8_t subcode = 0;
    switch (r) {
        case ERROR_MSGTYPE: subcode = NOTIF_SUBCODE_MSG_BAD_TYPE; break;
    }

    PROTO_TRY(
        new_msg_notification(s->session_buff, MAX_MSG_SIZE,
            NOTIF_CODE_ERROR_MSG, subcode, 0, NULL),
        goto proto_error
    );

sock_error:
    close(s->session_fd);
    s->session_state = STATE_IDLE;
    return NULL;
}


static void *
connect_loop(void *arg)
{
    session_t *s = arg;
    s->session_connect_retry = 60;

    int r = 0;
    while (1) {
        session_change_state(s, STATE_CONNECT);

        int res = connect(s->session_fd,
            (struct sockaddr*)&s->session_peer_addr,
            sizeof(struct sockaddr_in6));

        if (res < 0) {
            ERROR("connect(): %s", strerror(errno));
            session_change_state(s, STATE_IDLE);
            sleep(s->session_connect_retry);
            if (s->session_connect_retry < 3600)
                s->session_connect_retry *= 2;
            continue;
        }

        break;
    }

    s->session_connect_retry = 60;
    session_loop(arg);
}


session_t *
session_new_initiate(uint32_t itad, uint32_t id, uint16_t hold,
    capinfo_transmode_t transmode, const struct sockaddr_in6 *peer_addr,
    uint32_t peer_itad)
{
    /* allocate resources */
    session_t *session = malloc(sizeof(session_t));
    session->session_buff = malloc(MAX_MSG_SIZE);
    session->session_state = STATE_IDLE;
    session->session_transmode = transmode;
    session->session_itad = itad;
    session->session_id = id;
    session->session_peer_itad = peer_itad;
    session->session_peer_id = 0;

    memcpy(&session->session_peer_addr, peer_addr, sizeof(struct sockaddr_in6));
    session->session_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

    pthread_create(&session->session_thread, NULL, &connect_loop, session);
    pthread_detach(session->session_thread);

    DEBUG("initiated peer session %s", session_str(session));

    return session;
}

session_t *
session_new_peer(uint32_t itad, uint32_t id, uint16_t hold,
    capinfo_transmode_t transmode, const struct sockaddr_in6 *peer_addr,
    uint32_t peer_itad, int fd)
{
    /* allocate resources */
    session_t *session = malloc(sizeof(session_t));
    session->session_buff = malloc(MAX_MSG_SIZE);
    session->session_state = STATE_IDLE;
    session->session_transmode = transmode;
    session->session_itad = itad;
    session->session_id = id;
    session->session_peer_itad = peer_itad;
    session->session_peer_id = 0;

    memcpy(&session->session_peer_addr, peer_addr, sizeof(struct sockaddr_in6));
    session->session_fd = fd;

    pthread_create(&session->session_thread, NULL, &session_loop, session);
    pthread_detach(session->session_thread);

    DEBUG("accepted peer session %s", session_str(session));

    return session;
}

void
session_shutdown(session_t *session)
{
    /* TODO send CEASE NOTIFICATION */
    DEBUG("shutting down session %s", session_str(session));
    shutdown(session->session_fd, SHUT_RDWR); /* recv loop does close() */
    session->session_state = STATE_IDLE;
}

void
session_destroy(session_t *session)
{
    free(session->session_buff);
    free(session);
}

