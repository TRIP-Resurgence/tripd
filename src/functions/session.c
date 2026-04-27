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

#include "db/trib.h"
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
        new_msg_notif(buff, MAX_MSG_SIZE, code, subcode, 0, NULL),
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
            /* flush for now */
            if (recv(s->fd, recv_wnd, msg->msg_len, 0) < 0) {
                ERROR("shit");
                goto sock_error;
            }
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
    close(s->fd);
    return NULL;
}

static ssize_t
serialize_group(char *buff, size_t len, const session_t *s, entry_group_t *group,
    uint32_t local_id, uint32_t local_itad)
{
    msg_update_attr_t *attr_bufs[10]; /* max 10 num of attrs per UPDATE */
    for (size_t i = 0; i < 10; i++) {
        attr_bufs[i] = malloc(MAX_MSG_SIZE);
        memset(attr_bufs[i], 0, MAX_MSG_SIZE);
    }
    size_t attrs_count = 0;
    int r = 0;

    /* create array of routes from array of entry references */
    route_t routes[4096];
    for (size_t j = 0; j < group->size; j++) {
        routes[j].route_af = group->entries[j]->af;
        routes[j].route_app_proto = group->entries[j]->app_proto;
        routes[j].route_len = strlen(group->entries[j]->prefix);
        memcpy(&routes[j].route_addr, group->entries[j]->prefix,
            strlen(group->entries[j]->prefix));
    }

    /* figure out max seq number on entries */
    int32_t seq = INITIAL_SEQUENCE_NUMBER;
    for (size_t j = 0; j < group->size; j++)
        if (group->entries[j]->seq > seq)
            seq = group->entries[j]->seq;
    seq += 1; /* next sequence number */
    /* set last used sequence number */
    for (size_t j = 0; j < group->size; j++)
        group->entries[j]->seq = seq;

    /* if WithdrawnRoutes, only that one attribute needed (?) */
    if (group->attrs.withdrawn) {
        new_attr_withdrawnroutes(attr_bufs[attrs_count], MAX_MSG_SIZE,
            /* internal or external peer
             * always link-state encapsulate for internal flooding */
            s->peer->itad == local_itad,
            local_id, seq, routes, group->size);
        attrs_count++;

        goto finish;
    }

    /* for ReacheableRoutes */
    new_attr_reachableroutes(attr_bufs[attrs_count], MAX_MSG_SIZE,
        /* internal or external peer
         * always link-state encapsulate for internal flooding */
        s->peer->itad == local_itad,
        local_id, seq, routes, group->size);
    attrs_count++;
    
    /* NextHopServer */
    if (!ATTR_IS_USED_NEXTHOP(group->attrs.use)) {
        ERROR("tried to advertise reachableroutes without nexthopserver");
        return -1;
    }

    new_attr_nexthopserver(attr_bufs[attrs_count++], MAX_MSG_SIZE,
        group->attrs.nextitad, group->attrs.nexthop);

    /* AdvertisementPath */
    if (ATTR_IS_USED_ADVERTPATH(group->attrs.use)) {
        itadpath_t path = {
            ITADPATH_TYPE_AP_SEQUENCE, group->attrs.routedpath_size
        };
        memcpy(&path.itadpath_segs, group->attrs.routedpath,
            sizeof(uint32_t) * group->attrs.routedpath_size);
        new_attr_routedpath(attr_bufs[attrs_count++], MAX_MSG_SIZE, &path);
    }

    /* RoutedPath */
    if (ATTR_IS_USED_ROUTEDPATH(group->attrs.use)) {
        itadpath_t path = {
            ITADPATH_TYPE_AP_SEQUENCE, group->attrs.routedpath_size
        };
        memcpy(&path.itadpath_segs, group->attrs.routedpath,
            sizeof(uint32_t) * group->attrs.routedpath_size);
        new_attr_routedpath(attr_bufs[attrs_count++], MAX_MSG_SIZE, &path);
    }

    /* AtomicAggregate */
    if (group->attrs.atomicaggregate) {
        new_attr_atomicaggregate(attr_bufs[attrs_count++], MAX_MSG_SIZE);
    }
        
    /* LocalPreference
     * intra-domain only */
    if (ATTR_IS_USED_LOCALPREF(group->attrs.use) && s->peer->itad == local_itad)
        new_attr_localpref(attr_bufs[attrs_count++], MAX_MSG_SIZE,
            group->attrs.local_pref);

    /* MultiExitDiscriminator
     * extra-domain only */
    if (ATTR_IS_USED_METRIC(group->attrs.use) && s->peer->itad != local_itad)
        new_attr_multiexitdisc(attr_bufs[attrs_count++], MAX_MSG_SIZE,
            group->attrs.metric);

    /* Communities */
    if (ATTR_IS_USED_COMMUNITIES(group->attrs.use))
        new_attr_communities(attr_bufs[attrs_count++], MAX_MSG_SIZE,
            group->attrs.communities, group->attrs.communities_size);

    /* ConvertedRoute propagate */
    if (group->attrs.convertedroute)
        new_attr_convertedroute(attr_bufs[attrs_count++], MAX_MSG_SIZE);

finish:
    /* serialize serialized attributes into UPDATE */
    r = new_msg_update(buff, MAX_MSG_SIZE,
        (const msg_update_attr_t**)attr_bufs, attrs_count);

    for (size_t i = 0; i < 10; i++)
         free(attr_bufs[i]);

    return r;
}

void
session_update(const session_t *s, uint32_t local_id, uint32_t local_itad)
{
    entry_t **new_ents = NULL;

    /* entries that havent been sent UPDATE'd */
    size_t new_ents_count = get_new_entries(s->adj_trib_out, &new_ents);

    /* group entries by attributes */
    entry_group_t *groups = NULL;
    size_t groups_count = group_entries_by_attrs(new_ents, new_ents_count,
        &groups);
    
    for (size_t i = 0; i < groups_count; i++) {
        char msg_buff[MAX_MSG_SIZE];

        ssize_t upd_size = serialize_group(msg_buff, MAX_MSG_SIZE, s, &groups[i],
            local_id, local_itad);

        if (upd_size < 0)
            continue;

        SOCK_TRY_SEND(send(s->fd, msg_buff, upd_size, 0), goto sock_error);
    }

sock_error:
    for (size_t i = 0; i < groups_count; i++)
        free(groups[i].entries);
    free(groups);
    free(new_ents);
}

void
session_shutdown(session_t *session)
{
    if (session->state == STATE_ACTIVE)
        send_notification(session->fd, NOTIF_CODE_CEASE, 0);
    DEBUG("shutting down session %s", session_str(session));
    shutdown(session->fd, SHUT_RDWR); /* recv loop does close() */
    session_change_state(session, STATE_IDLE);
}

void
session_destroy(session_t *session)
{
    if (session->routetypes)
        free(session->routetypes);
    free(session);
}

