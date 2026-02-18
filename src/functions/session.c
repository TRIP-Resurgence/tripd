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

/** \file
 * Implements P2P connection between two LS.
 */

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
        inet_ntop(AF_INET6, &s->addr->sin6_addr, abuff,
            sizeof(abuff)),
        s->peer_itad,
        inet_ntop(AF_INET, &s->peer_id, abuff2, sizeof(abuff2)));
    return str;
}

const char *
id_str(uint32_t id)
{
    static char idbuff[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &id, idbuff, sizeof(idbuff));
    return idbuff;
}

static void
session_change_state(session_t *s, session_state_t new_state)
{
    DEBUG("peer session %s changed state from %s to %s", session_str(s),
        session_state_strs[s->state], session_state_strs[new_state]);
    s->state = new_state;
}

int
send_notification(int fd, int code, int subcode)
{
    int res = 0;
    char buff[MAX_MSG_SIZE];

    PROTO_TRY(
        new_msg_notif(buff, MAX_MSG_SIZE,
            NOTIF_CODE_ERROR_MSG, subcode, 0, NULL),
        res, goto sock_error
    );

    SOCK_TRY_SEND(
        send(fd, buff, res, 0),
        goto sock_error
    );

    return 0;

sock_error:
    return -1;
}

/** /brief for session handler
 *
 * only if res warrants a NOTIFICATION
 */
static int
send_notification_res(int fd, int res)
{
    uint8_t code = 0, subcode = 0;
    switch (res) {
        case ERROR_MSGTYPE:
            code = NOTIF_CODE_ERROR_MSG;
            subcode = NOTIF_SUBCODE_MSG_BAD_TYPE;
        break;
    }

    if (subcode)
        if(send_notification(fd, code, subcode) < 0)
            return -1;

    return 0;
}


void *
session_loop(void *arg)
{
    session_t *s = arg;

    int res = 0, toread = 0;
    char buff[MAX_MSG_SIZE];

    while (1) {
        void *recv_wnd = buff;
        /* receive and decode message header */
        SOCK_TRY_RECV(s->fd, recv_wnd, msg_t, goto sock_error);

        const msg_t *msg = NULL;
        PROTO_TRY(
            parse_msg(buff, res, &msg),
            res, goto proto_error
        );

        DEBUG("received msg: %s[%d]", msg_type_strs[msg->msg_type],
            msg->msg_len);

        switch (msg->msg_type) {
        case MSG_TYPE_OPEN: {
            ERROR("session %s unexpected OPEN message", session_str(s));

            send_notification(s->fd, NOTIF_CODE_ERROR_MSG,
                NOTIF_SUBCODE_MSG_BAD_TYPE);

            goto sock_error;
        } break;
        case MSG_TYPE_UPDATE: {
            /* TODO */
        } break;
        case MSG_TYPE_NOTIFICATION: {
            /* TODO */
        } break;
        case MSG_TYPE_KEEPALIVE: {
            /* TODO: reset timer (TODO keepalive timer */
        } break;
        }

        /* flush and continue */
        res = recv(s->fd, buff, MAX_MSG_SIZE, 0);
        if (res < 0) {
            ERROR("recv(): %s", strerror(errno)); \
            goto sock_error;
        } else if (res == 0) {
            DEBUG("connection closed by peer"); \
            goto sock_error;
        } else {
            DEBUG("%d trailing bytes dropped", res);
        }
    }

proto_error:
    send_notification_res(s->fd, res);

sock_error:
    close(s->fd);
    session_change_state(s, STATE_IDLE);
    return NULL;
}

void
session_shutdown(session_t *session)
{
    /* TODO send CEASE NOTIFICATION */
    DEBUG("shutting down session %s", session_str(session));
    shutdown(session->fd, SHUT_RDWR); /* recv loop does close() */
    session->state = STATE_IDLE;
}

void
session_destroy(session_t *session)
{
    free(session->addr);
    free(session);
}

