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
#include <stddef.h>
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

/** \brief Parse UPDATE attributes */
int
handle_update(manager_t *m, session_t *s, msg_t *msg)
{
    /* parse attribute headers */
    size_t toparse = msg->msg_len;
    void *attr_ptr = (void*)&msg->msg_val;
    msg_update_attr_t *attrs[12] = { };
    int attrs_count = 0, attrs_size_sum = 0;
    int res = 0;
    while (toparse) {
        msg_update_attr_t *attr = NULL;
        void *attr_val = NULL;
        if (IS_ATTR_FLAG_LSENCAP(*(uint8_t*)attr_ptr)) {
            msg_update_attr_lsencap_t *attr_lsencap = NULL;
            PROTO_TRY(
                parse_msg_update_attr_lsencap(attr_ptr, msg->msg_len,
                    &attr_lsencap),
                res, return -1
            );

            attr = (msg_update_attr_t*)attr_lsencap;
            attrs_size_sum += sizeof(msg_update_attr_lsencap_t) + attr->attr_len;
            toparse -= sizeof(msg_update_attr_lsencap_t) + attr->attr_len;
        } else {
            PROTO_TRY(
                parse_msg_update_attr(attr_ptr, msg->msg_len,
                    &attr),
                res, return -1
            );

            attrs_size_sum += sizeof(msg_update_attr_t) + attr->attr_len;
            toparse -= sizeof(msg_update_attr_t) + attr->attr_len;
        }

        DEBUG(" %s[%d] (%s)", attr_strs[attr->attr_type], attr->attr_len,
            flags_str(attr->attr_flags));

        attrs[attrs_count++] = attr;

        attr_ptr = (void*)&attr->attr_val + attr->attr_len;
    }

    if (attrs_size_sum != msg->msg_len) {
        ERROR("malformed UPDATE (attribute size)");
        return -1;
    }

    /* handle attributes */
    int lsencapsulated = 0, routing_update = 0, withdrawn = 0;
    uint32_t id = 0, seq = 0;
    for (int i = 0; i < attrs_count; i++) {
        if (IS_ATTR_FLAG_LSENCAP(attrs[i]->attr_flags)) {
            lsencapsulated = 1;
            id = ((msg_update_attr_lsencap_t*)attrs[i])->attr_id;
            seq = ((msg_update_attr_lsencap_t*)attrs[i])->attr_seq;
        }
        if (attrs[i]->attr_type == ATTR_TYPE_WITHDRAWNROUTES) {
            routing_update = 1;
            withdrawn = 1;
        } else if (attrs[i]->attr_type == ATTR_TYPE_REACHABLEROUTES) {
            routing_update = 1;
            withdrawn = 0;
        }
    }

    entry_t entries[1024] = { };
    int route_count = 0;

    entry_attrs_t ent_attrs = { };
    for (int i = 0; i < attrs_count; i++) {
        if (routing_update &&
            (attrs[i]->attr_type == ATTR_TYPE_WITHDRAWNROUTES ||
            attrs[i]->attr_type == ATTR_TYPE_REACHABLEROUTES))
        {
            toparse = attrs[i]->attr_len;
            void *route_ptr = attrs[i]->attr_val;
            while (toparse) {
                route_t *route = NULL;
                PROTO_TRY(parse_route(route_ptr, toparse, &route),
                    res, return -1);

                entries[route_count].af = route->route_af;
                entries[route_count].app_proto = route->route_app_proto;
                entries[route_count].prefix = strndup(route->route_addr,
                    route->route_len);

                route_count++;
                route_ptr += res + route->route_len;
                toparse -= res + route->route_len;
            }
        } else if (!withdrawn &&
            attrs[i]->attr_type == ATTR_TYPE_NEXTHOPSERVER)
        {
            attr_nexthopserver_t *nexthop = NULL;
            PROTO_TRY(
                parse_attr_nexthopserver(attrs[i]->attr_val, attrs[i]->attr_len,
                    &nexthop),
                res, return -1
            );

            ent_attrs.nexthop = strndup(nexthop->nexthopserver_server,
                nexthop->nexthopserver_serverlen);
            ent_attrs.nextitad = nexthop->nexthopserver_itad;
        } else if (!withdrawn &&
            attrs[i]->attr_type == ATTR_TYPE_ADVERTISEMENTPATH)
        {
            itadpath_t *itadpath = NULL;
            PROTO_TRY(
                parse_itadpath(attrs[i]->attr_val, attrs[i]->attr_len,
                    &itadpath),
                res, return -1
            );

            ent_attrs.advertpath = malloc(itadpath->itadpath_len);

            for (int j = 0; j < itadpath->itadpath_len / sizeof(uint32_t); j++) {
                uint32_t *itad = NULL;
                PROTO_TRY(
                    parse_itad(&itadpath->itadpath_segs[j], sizeof(uint32_t),
                        &itad),
                    res, return -1
                );

                ent_attrs.advertpath[j] = *itad;
            }
        } else if (!withdrawn &&
            attrs[i]->attr_type == ATTR_TYPE_ROUTEDPATH)
        {
            itadpath_t *itadpath = NULL;
            PROTO_TRY(
                parse_itadpath(attrs[i]->attr_val, attrs[i]->attr_len,
                    &itadpath),
                res, return -1
            );

            ent_attrs.routedpath = malloc(itadpath->itadpath_len);

            for (int j = 0; j < itadpath->itadpath_len / sizeof(uint32_t); j++) {
                uint32_t *itad = NULL;
                PROTO_TRY(
                    parse_itad(&itadpath->itadpath_segs[j], sizeof(uint32_t),
                        &itad),
                    res, return -1
                );

                ent_attrs.routedpath[j] = *itad;
            }
        } else if (!withdrawn &&
            attrs[i]->attr_type == ATTR_TYPE_ATOMICAGGREGATE)
        {
            ent_attrs.atomicaggregate = 1;
        } else if (!withdrawn &&
            attrs[i]->attr_type == ATTR_TYPE_LOCALPREFERENCE)
        {
            attr_localpref_t *localpref = NULL;
            PROTO_TRY(
                parse_attr_localpref(attrs[i]->attr_val, attrs[i]->attr_len,
                    &localpref),
                res, return -1
            );

            ent_attrs.local_pref = *localpref;
        } else if (!withdrawn &&
            attrs[i]->attr_type == ATTR_TYPE_MULTIEXITDISC)
        {
            attr_multiexitdisc_t *multiexitdisc = NULL;
            PROTO_TRY(
                parse_attr_multiexitdisc(attrs[i]->attr_val, attrs[i]->attr_len,
                    &multiexitdisc),
                res, return -1
            );

            ent_attrs.metric = *multiexitdisc;
        } else if (!withdrawn &&
            attrs[i]->attr_type == ATTR_TYPE_COMMUNITIES)
        {
            community_t *communities = (void*)&attrs[i]->attr_val; /* preparse*/
            community_t *community = NULL;

            ent_attrs.communities_size =
                attrs[i]->attr_len / sizeof(community_t);

            for (int j = 0; j < attrs[i]->attr_len / sizeof(community_t); j++) {
                PROTO_TRY(
                    parse_community(&communities[j], sizeof(community_t),
                        &community),
                    res, return -1
                );
                
                ent_attrs.communities[j] = *community;
            }
        } else if (attrs[i]->attr_type == ATTR_TYPE_ITADTOPOLOGY) {
            /* TODO: this idfk */
        } else if (!withdrawn &&
            attrs[i]->attr_type == ATTR_TYPE_CONVERTEDROUTE)
        {
            ent_attrs.convertedroute = 1;
        }
    }

    if (!routing_update)
        return 0;

    for (int i = 0; i < ent_attrs.advertpath_size; i++) {
        if (ent_attrs.advertpath[i] == m->itad) {
            INFO("routes with this itad in advertisementpath rejected");
            return 0;
        }
    }

    for (int i = 0; i < ent_attrs.advertpath_size; i++) {
        if (ent_attrs.advertpath[i] == m->itad) {
            INFO("routes with this itad in routedpath rejected");
            return 0;
        }
    }

    if (s->peer->itad == m->itad && ent_attrs.nextitad == m->itad) {
        INFO("external route with this itad in nexthop rejected");
        return 0;
    }


    if (withdrawn) {
        for (int i = 0; i < route_count; i++) {
            entry_t **match = trib_table_find(&s->adj_trib_in, entries[i].af,
                entries[i].prefix);
            if (!match) {
                DEBUG("withdrawn update not found");
                continue;
            }

            (*match)->attrs.withdrawn = 1;

            free(entries[i].prefix);
        }
    } else {
        time_t learntime = time(NULL);
        trib_table_clear(&s->adj_trib_in);
        for (int i = 0; i < route_count; i++) {
            entries[i].type = ENTRY_TYPE_TRIP;
            entries[i].learn_itad = s->peer->itad;
            entries[i].learn_lsid = s->id;
            if (lsencapsulated)
                entries[i].seq = seq;
            entries[i].time = learntime;
            entries[i].attrs = ent_attrs;
            entries[i].sent = 0;
            trib_table_insert_or_replace(&s->adj_trib_in,
                entry_clone(&entries[i]));

            free(entries[i].prefix);
        }

    }

    DEBUG("updated %d routes", route_count);

    free(ent_attrs.advertpath);
    free(ent_attrs.routedpath);
    free(ent_attrs.communities);
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

        msg_t *msg = NULL;
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

            size_t toread = msg->msg_len;
            while (toread) {
                res = recv(s->fd, recv_wnd, msg->msg_len, 0);
                if (res < 0) {
                    ERROR("recv(): %s", strerror(errno));
                    goto sock_error;
                } else if (res == 0) {
                    DEBUG("connection closed by peer");
                    goto sock_error;
                }

                recv_wnd += res;
                toread -= res;
            }
            
            if (msg->msg_len + sizeof(msg_t) > MAX_MSG_SIZE) {
                ERROR("message too large");
                continue; /* drop */
            }

            handle_update(m, s, msg);
        } break;
        case MSG_TYPE_NOTIFICATION: {
            SOCK_TRY_RECV(s->fd, recv_wnd, msg_notif_t, goto sock_error);

            msg_notif_t *msg_notif = NULL;
            PROTO_TRY(
                parse_msg_notif(msg->msg_val, sizeof(msg_notif_t), &msg_notif),
                res, goto proto_error
            );

            INFO("received notification: %s",
                notif_code_subcode_str(msg_notif->notif_error_code,
                    msg_notif->notif_error_subcode));

            size_t toread = msg->msg_len - sizeof(msg_notif_t);
            while (toread) {
                res = recv(s->fd, recv_wnd, msg->msg_len, 0);
                if (res < 0) {
                    ERROR("recv(): %s", strerror(errno));
                    goto sock_error;
                } else if (res == 0) {
                    DEBUG("connection closed by peer");
                    goto sock_error;
                }

                recv_wnd += res;
                toread -= res;
            }

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
serialize_routes(void *buff, size_t len, entry_group_t *group)
{
    void *ptr = buff;

    for (size_t i = 0; i < group->size; i++) {
        if (ptr - buff > len)
            return -1;

        int r = new_route(ptr, len, group->entries[i]->af,
            group->entries[i]->app_proto, group->entries[i]->prefix);
        ptr += r;
        len -= r;
    }

    return ptr - buff;
}


static ssize_t
serialize_group(char *buff, size_t len, const session_t *s, entry_group_t *group,
    uint32_t local_id, uint32_t local_itad)
{
    char buff_array[10][MAX_MSG_SIZE];
    msg_update_attr_t *attr_bufs[10]; /* max 10 num of attrs per UPDATE */
    for (size_t i = 0; i < 10; i++) {
        attr_bufs[i] = (void*)buff_array[i];
        memset(attr_bufs[i], 0, MAX_MSG_SIZE);
    }
    size_t attrs_count = 0;
    int r = 0;

    /* create array of routes from array of entry references */
    char routes[MAX_MSG_SIZE];
    ssize_t routes_size = serialize_routes(routes, MAX_MSG_SIZE, group);
    if (routes_size < 0) {
        ERROR("too many routes to send");
        return -1;
    }

    /* figure out max seq number on entries */
    int32_t seq = INITIAL_SEQUENCE_NUMBER;
    for (size_t j = 0; j < group->size; j++)
        if (group->entries[j]->seq > seq)
            seq = group->entries[j]->seq;
    seq += 1; /* next sequence number */
    /* set last used sequence number TODO: move after send */
    for (size_t j = 0; j < group->size; j++)
        group->entries[j]->seq = seq;

    /* if WithdrawnRoutes, only that one attribute needed (?) */
    if (group->attrs.withdrawn) {
        PROTO_TRY(
            new_attr_withdrawnroutes(attr_bufs[attrs_count++], MAX_MSG_SIZE,
                /* internal or external peer
                 * always link-state encapsulate for internal flooding */
                s->peer->itad == local_itad,
                local_id, seq, routes, routes_size),
            r, goto proto_error
        );

        goto finish;
    }

    /* for ReacheableRoutes */
    PROTO_TRY(
        new_attr_reachableroutes(attr_bufs[attrs_count++], MAX_MSG_SIZE,
            /* internal or external peer
             * always link-state encapsulate for internal flooding */
            s->peer->itad == local_itad,
            local_id, seq, routes, routes_size),
        r, goto proto_error
    );

    /* NextHopServer */
    if (!ATTR_IS_USED_NEXTHOP(group->attrs.use)) {
        ERROR("tried to advertise reachableroutes without nexthopserver");
        return -1;
    }

    PROTO_TRY(
        new_attr_nexthopserver(attr_bufs[attrs_count++], MAX_MSG_SIZE,
            group->attrs.nextitad, group->attrs.nexthop),
        r, goto proto_error
    );

    /* AdvertisementPath */
    if (ATTR_IS_USED_ADVERTPATH(group->attrs.use)) {
        PROTO_TRY(
            new_attr_advertisementpath(attr_bufs[attrs_count++], MAX_MSG_SIZE,
                ITADPATH_TYPE_AP_SEQUENCE, group->attrs.advertpath,
                group->attrs.advertpath_size),
            r, goto proto_error
        );
    }

    /* RoutedPath */
    if (ATTR_IS_USED_ROUTEDPATH(group->attrs.use)) {
        PROTO_TRY(
            new_attr_routedpath(attr_bufs[attrs_count++], MAX_MSG_SIZE,
                ITADPATH_TYPE_AP_SEQUENCE, group->attrs.routedpath,
                    group->attrs.routedpath_size),
            r, goto proto_error
        );
    }

    /* AtomicAggregate */
    if (group->attrs.atomicaggregate) {
        PROTO_TRY(
            new_attr_atomicaggregate(attr_bufs[attrs_count++], MAX_MSG_SIZE),
            r, goto proto_error
        );
    }
        
    /* LocalPreference
     * intra-domain only */
    if (ATTR_IS_USED_LOCALPREF(group->attrs.use) && s->peer->itad == local_itad) {
        PROTO_TRY(
            new_attr_localpref(attr_bufs[attrs_count++], MAX_MSG_SIZE,
                group->attrs.local_pref),
            r, goto proto_error
        );
    }

    /* MultiExitDiscriminator
     * extra-domain only */
    if (ATTR_IS_USED_METRIC(group->attrs.use) && s->peer->itad != local_itad) {
        PROTO_TRY(
            new_attr_multiexitdisc(attr_bufs[attrs_count++], MAX_MSG_SIZE,
                group->attrs.metric),
            r, goto proto_error
        );
    }

    /* Communities */
    if (ATTR_IS_USED_COMMUNITIES(group->attrs.use)) {
        PROTO_TRY(
            new_attr_communities(attr_bufs[attrs_count++], MAX_MSG_SIZE,
                group->attrs.communities, group->attrs.communities_size),
            r, goto proto_error
        );
    }

    /* ConvertedRoute propagate */
    if (group->attrs.convertedroute) {
        PROTO_TRY(
            new_attr_convertedroute(attr_bufs[attrs_count++], MAX_MSG_SIZE),
            r, goto proto_error
        );
    }

finish:
    /* serialize serialized attributes into UPDATE */
    r = new_msg_update(buff, MAX_MSG_SIZE,
        (const msg_update_attr_t**)attr_bufs, attrs_count);

    return r;

proto_error:
    return -1;
}

void
session_update(const session_t *s, uint32_t local_id, uint32_t local_itad)
{
    entry_t **new_ents = NULL;

    /* entries that havent been sent UPDATE'd */
    size_t new_ents_count = get_new_entries(&s->adj_trib_out, &new_ents);

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
    if (session->state == STATE_ESTABLISHED)
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

