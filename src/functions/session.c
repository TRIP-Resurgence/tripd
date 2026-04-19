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

#include "manager.h"
#include "protocol/protocol.h"

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
        inet_ntop(AF_INET6, &s->peer->addr.sin6_addr, abuff,
            sizeof(abuff)),
        s->peer->itad,
        inet_ntop(AF_INET, &s->id, abuff2, sizeof(abuff2)));
    return str;
}

const char *
id_str(uint32_t id)
{
    static char idbuff[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &id, idbuff, sizeof(idbuff));
    return idbuff;
}

void
session_change_state(session_t *s, session_state_t new_state)
{
    if (s->state != new_state)
        DEBUG("peer session %s changed state from %s to %s", session_str(s),
            session_state_strs[s->state], session_state_strs[new_state]);
    s->state = new_state;
    s->state_time = time(NULL);
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

/** \brief Session loop
 *
 * \param arg Of type (void*){ manager_t *m, session_t *s }
 */
void *
session_loop(void *arg)
{
    manager_t *m = ((void**)arg)[0];
    session_t *s = ((void**)arg)[1];

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
            s->last_read_time = time(NULL);
            /* TODO: update */
        } break;
        case MSG_TYPE_NOTIFICATION: {
            SOCK_TRY_RECV(s->fd, recv_wnd, msg_notif_t, goto sock_error);

            const msg_notif_t *msg_notif = NULL;
            PROTO_TRY(
                parse_msg_notif(msg->msg_val, sizeof(msg_notif_t), &msg_notif),
                res, goto proto_error
            );

            INFO("received notification: %s",
                notif_code_subcode_str(msg_notif->notif_error_code,
                    msg_notif->notif_error_subcode));

            if (msg_notif->notif_error_code == NOTIF_CODE_ERROR_EXPIRED)
                goto sock_error;
        } break;
        case MSG_TYPE_KEEPALIVE: {
            s->last_read_time = time(NULL);
        } break;
        }
    }

proto_error:
    send_notification_res(s->fd, res);

sock_error:
    if (!s->mark_stop_init)
        session_change_state(s, STATE_IDLE);
    close(s->fd);
    return NULL;
}


void
update_session(const session_t *s)
{
    char buff[MAX_MSG_SIZE];

    /* should be enough */
    size_t new_ents_size = 0, new_ents_capacity = 256;
    entry_t **new_ents = malloc(sizeof(entry_t*) * new_ents_capacity);

    /* get new entries */
    for (size_t i = 0; i < s->adj_trib_out->size; i++) {
        if (s->adj_trib_in->table[i]->sent)
            continue;

        if (new_ents_size + 1 > new_ents_capacity) {
            new_ents_capacity *= 2;
            new_ents = realloc(new_ents, sizeof(entry_t*) * new_ents_capacity);
        }

        new_ents[new_ents_capacity] = s->adj_trib_in->table[i];
    }

    /* TODO: group entries by attributes */
    
    //new_msg_update(buff, MAX_MSG_SIZE, attrs, size);
}

void
session_shutdown(session_t *session)
{
    /* TODO send CEASE NOTIFICATION */
    DEBUG("shutting down session %s", session_str(session));
    shutdown(session->fd, SHUT_RDWR); /* recv loop does close() */
    session_change_state(session, STATE_IDLE);
}

void
session_destroy(session_t *session)
{
    if (session->routetypes)
        free(session->routetypes);
    trib_table_deinit(session->adj_trib_in);
    trib_table_deinit(session->adj_trib_out);
    free(session);
}

