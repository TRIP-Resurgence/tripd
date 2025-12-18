/*

    trip: Modern TRIP LS implementation
    Copyright (C) 2023 arf20 (Ángel Ruiz Fernandez)

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

#include "ls_peer_session.h"

#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <pthread.h>


void *
session_loop(void *arg)
{
    session_t *s = arg;

    int r = 0;
    while (1) {
        SOCK_TRY(
            recv(s->session_fd, s->session_buff, sizeof(msg_t), 0),
            goto sock_error;
        );

        const msg_t *msg = NULL;
        PROTO_TRY_PARSE(
            parse_msg(s->session_buff, r, &msg),
            goto proto_error
        )
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


session_t *
session_new_initiate(uint32_t itad, uint32_t id, uint16_t hold,
    const struct sockaddr *peer_addr)
{

}

session_t *
session_new_peer(uint32_t itad, uint32_t id, uint16_t hold,
    capinfo_transmode_t transmode, const struct sockaddr *peer_addr, int fd)
{
    /* allocate resources */
    session_t *session = malloc(sizeof(session_t));
    session->session_buff = malloc(MAX_MSG_SIZE);
    session->session_state = STATE_IDLE;
    session->session_type = 0;
    session->session_itad = itad;
    session->session_id = id;

    memcpy(&session->session_peer_addr, peer_addr, sizeof(struct sockaddr));
    session->session_fd = fd;

    
    /* send OPEN */
    int r = 0;
    PROTO_TRY(
        new_msg_open(session->session_buff, MAX_MSG_SIZE,
            hold, itad, id, supported_routetypes, supported_routetypes_size,
            transmode),
        goto error
    );

    SOCK_TRY(
        send(fd, session->session_buff, r, 0) < 0,
        goto error
    );

    session->session_state = STATE_OPENSENT;

    pthread_create(&session->session_thread, NULL, &session_loop, session);
    pthread_detach(session->session_thread);

error:
    close(fd);
    return session;
}


void
session_destroy(session_t *session)
{
    free(session->session_buff);
    free(session);
}

