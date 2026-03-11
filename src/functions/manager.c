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

    manager.c: location server session manager

*/

/** \file
 * Singleton class.
 * Implements connection management.
 * Listens for incoming connection requests. When one arrives, it parses
 * the OPEN on a thread to find capabilities, creates a session object and
 * delegates the session to session_run in the existing thread.
 * The manager can also initiate connections.
 */

#include "manager.h"

#include "locator.h"
#include <logging/logging.h>
#include <protocol/protocol.h>
#include "session.h"
#include <util/util.h>

#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/param.h>

#include <pthread.h>

#include <arpa/inet.h>
#include <unistd.h>

#define _COMPONENT_ "manager"

#ifndef MAX_BACKOFF_CONNECT_RETRY
    #define MAX_BACKOFF_CONNECT_RETRY 3600
#endif /* MAX_BACKOFF_CONNECT_RETRY */


static void *connect_loop(void *arg);


/** \brief Lookup session by ITAD and ID match */
static session_t *
manager_session_lookup_itad_id(const manager_t *m, uint32_t itad,
    uint32_t id)
{
    for (size_t i = 0; i < m->sessions_size; i++)
        if (m->sessions[i]->peer->itad == itad &&
            m->sessions[i]->id == id)
        {
            return m->sessions[i];
        }
    return NULL;
}

/** \brief Lookup session by locator peer */
static session_t *
manager_session_lookup_peer(const manager_t *m, const peer_t *peer)
{
    for (size_t i = 0; i < m->sessions_size; i++)
        if (m->sessions[i]->peer == peer && !m->sessions[i]->mark_stop_init)
        {
            return m->sessions[i];
        }
    return NULL;
}

/** \brief Lookup session by locator peer */
session_t *
manager_session_lookup_address(const manager_t *m,
    const struct sockaddr_in6 *addr)
{
    for (size_t i = 0; i < m->sessions_size; i++)
        if (!m->sessions[i]->mark_stop_init &&
            memcmp(&m->sessions[i]->peer->addr.sin6_addr, &addr->sin6_addr,
            sizeof(struct in6_addr)) == 0)
        {
            return m->sessions[i];
        }
    return NULL;
}

/** \brief Send notification corresponding to protocol error
 *
 * for OPEN handler, only send if res warrants a NOTIFICATION
 */
static int
send_notification_res(int fd, int res)
{
    uint8_t code = NOTIF_CODE_ERROR_OPEN, subcode = 0;
    switch (res) {
        case ERROR_MSGTYPE:
            code = NOTIF_CODE_ERROR_MSG;
            subcode = NOTIF_SUBCODE_MSG_BAD_TYPE;
        break;
        case ERROR_VERSION: subcode = NOTIF_SUBCODE_OPEN_UNSUP_VERSION; break;
        case ERROR_ITAD: subcode = NOTIF_SUBCODE_OPEN_BAD_ITAD; break;
        case ERROR_OPT: subcode = NOTIF_SUBCODE_OPEN_UNSUP_OPT; break;
        case ERROR_HOLD: subcode = NOTIF_SUBCODE_OPEN_BAD_HOLD; break;
        case ERROR_CAPINFO_CODE: subcode = NOTIF_SUBCODE_OPEN_UNSUP_CAP; break;
    }

    if (subcode)
        if(send_notification(fd, code, subcode) < 0)
            return -1;

    return 0;
}

/** \brief Add session to manager */
static void
manager_session_add(manager_t *m, session_t *s)
{
    if (m->sessions_size + 1 > m->sessions_capacity) {
        m->sessions = realloc(m->sessions,
            2 * sizeof(session_t) * m->sessions_capacity);
        m->sessions_capacity *= 2;
    }

    m->sessions[m->sessions_size++] = s;
}

/** \brief Remove session from manager */
static void
manager_session_remove(manager_t *m, session_t *s)
{
    int s_idx = -1;
    for (int i = 0; i < m->sessions_size; i++)
        if (m->sessions[i] == s)
            s_idx = i;

    session_destroy(s);
    memcpy(&m->sessions[s_idx], &m->sessions[s_idx + 1],
        sizeof(session_t*) * (m->sessions_size - s_idx));
    m->sessions_size--;
}

/** \brief Compare two ITAD,ID pairs
 *
 * \return non-zero if pair 1 < pair 2, zero if otherwise
 */
static int
compare_itad_id(uint32_t itad1, uint32_t id1, uint32_t itad2, uint32_t id2)
{
    return (id1 < id2) || (id1 == id2 && (itad1 < itad2));
}

/** \brief Compare two colliding sessions
 *
 * \return non-zero if s1 should be closed, zero if s2 should be closed
 *
 * \param s1 New session
 * \param s2 Old session
 */
static int
manager_collision_sessions_compare(const manager_t *m, const session_t *s1,
    const session_t *s2)
{
    if (s1->initiated == s2->initiated) {
        ERROR("colliding connections initiated by same ID");
        return 1;
    } else if (s1->initiated) {
        /* new session initiated by local
         * return local < s2 peer */
        return compare_itad_id(m->itad, m->id, s2->peer->itad, s2->id);
    } else {
        /* old connection initiated by local
         * return s1 peer < local */
        return compare_itad_id(s1->peer->itad, s1->id, m->itad, m->id);
    }
}

/* =================== REQUEST HANDLING ===================================== */

/** \brief Handle a received OPEN message (OpenConfirm State) */
static int
handle_open(manager_t *m, session_t *s, const msg_t *msg,
    void *recv_wnd)
{
    int res = 0, toread = 0;
    SOCK_TRY_RECV(s->fd, recv_wnd, msg_open_t, goto sock_error);

    const msg_open_t *open = NULL;
    PROTO_TRY(
        parse_msg_open(msg->msg_val, res, &open),
        res, goto proto_error
    );

    DEBUG("OPEN(ver %d, hold %d, itad %d, id %s, opts len %d)",
        open->open_ver, open->open_hold, open->open_itad,
        id_str(open->open_id), open->open_opts_len);

    /* check received OPEN fields */
    if (open->open_itad != s->peer->itad) {
        ERROR("peer ITAD mismatch");
        send_notification(s->fd, NOTIF_CODE_ERROR_OPEN,
            NOTIF_SUBCODE_OPEN_BAD_ITAD);
        return -1;
    }

    /* duplicate ID in ITAD peer collision detection */
    if ((open->open_itad == m->itad) && (open->open_id == m->id)) {
        ERROR("duplicate ID in ITAD detected, colliding peer rejected");
        send_notification(s->fd, NOTIF_CODE_ERROR_OPEN,
            NOTIF_SUBCODE_OPEN_BAD_ID);
        return -1;
    }

    /* section 6.8 collision detection */
    session_t *coll_s = manager_session_lookup_itad_id(m, open->open_itad,
        open->open_id);
    if (coll_s) {
        WARNING("collision detected with peer (%d,%d)", open->open_itad,
            open->open_id);
        if (coll_s->state == STATE_OPENCONFIRM) {
            WARNING("collision resolved: kept higher ID or ITAD");
            if (manager_collision_sessions_compare(m, s, coll_s)) {
                send_notification(s->fd, NOTIF_CODE_CEASE, 0);
                return -1;
            } else {
                /* cease, close and destroy old session, remove from vector */
                send_notification(coll_s->fd, NOTIF_CODE_CEASE, 0);
                session_shutdown(coll_s);
                manager_session_remove(m, coll_s);
            }
        } else if (coll_s->state == STATE_ESTABLISHED) {
            WARNING("collision resolved: kept established connection");
            send_notification(s->fd, NOTIF_CODE_CEASE, 0);
            return -1;
        }
    }

    /* find old still initiating session pre-openconfirm if exists and stop it*/
    session_t *init_s = manager_session_lookup_peer(m, s->peer);
    if (init_s && (init_s->initiated == 1) && (init_s != s)) {
        INFO("closing old initiating session");
        init_s->mark_stop_init = 1;
    }


    s->id = open->open_id;
    s->hold = MIN(s->peer->hold, open->open_hold);
    s->keepalive = s->hold / 3;

    /* now add session to manager */
    if (!s->initiated)
        manager_session_add(m, s);

    size_t opts_toread = open->open_opts_len;
    const void *opt_cur = open->open_opts;
    while (opts_toread) {
        SOCK_TRY_RECV(s->fd, recv_wnd, msg_open_opt_t,
            goto sock_error);

        const msg_open_opt_t *opt = NULL;
        PROTO_TRY(
            parse_msg_open_opt(opt_cur, res, &opt),
            res, goto proto_error
        );

        opts_toread -= res;

        DEBUG(" option: %s[%d]", open_opt_type_strs[opt->opt_type],
            opt->opt_len);

        switch (opt->opt_type) {
        case OPEN_OPT_TYPE_CAPABILITY_INFO: {
            size_t capinfos_toread = opt->opt_len;
            const void *capinfo_cur = opt->opt_val;
            while (capinfos_toread) {
                SOCK_TRY_RECV(s->fd, recv_wnd, capinfo_t,
                    goto sock_error);

                const capinfo_t *capinfo = NULL;
                PROTO_TRY(
                    parse_capinfo(capinfo_cur, res, &capinfo),
                    res, goto proto_error
                );

                opts_toread -= res;
                capinfos_toread -= res;

                DEBUG("  capability info: %s[%d]",
                    capinfo_code_strs[capinfo->capinfo_code],
                    capinfo->capinfo_len);


                switch (capinfo->capinfo_code) {
                case CAPINFO_CODE_ROUTETYPE: {
                    size_t routetypes_toread = capinfo->capinfo_len;
                    const void *routetype_cur = capinfo->capinfo_val;

                    s->routetypes_count = routetypes_toread /
                        sizeof(capinfo_routetype_t);
                    s->routetypes = malloc(routetypes_toread);
                    void *crt_cur = s->routetypes;

                    while (routetypes_toread) {
                        SOCK_TRY_RECV(s->fd, recv_wnd,
                            capinfo_routetype_t, goto sock_error);

                        const capinfo_routetype_t *routetype = NULL;
                        PROTO_TRY(
                            parse_capinfo_routetype(routetype_cur, res,
                                &routetype),
                            res, goto proto_error
                        );

                        routetype_cur += res;
                        opts_toread -= res;
                        capinfos_toread -= res;
                        routetypes_toread -= res;

                        DEBUG("   route type: %s:%s",
                            af_strs[routetype->routetype_af],
                            app_proto_str(routetype->routetype_app_proto));

                        memcpy(crt_cur, routetype, res);
                        crt_cur += res;
                    }
                } break;
                case CAPINFO_CODE_TRANSMODE: {
                    SOCK_TRY_RECV(s->fd, recv_wnd,
                        capinfo_transmode_t, goto sock_error);

                    const capinfo_transmode_t *transmode = NULL;
                    PROTO_TRY(
                        parse_capinfo_transmode(capinfo->capinfo_val,
                            res, &transmode),
                        res, goto proto_error
                    );

                    opts_toread -= res;
                    capinfos_toread -= res;

                    DEBUG("   transmission mode: %s",
                        capinfo_transmode_strs[*transmode]);

                    if (*transmode != CAPINFO_TRANS_SEND_RECV &&
                        (*transmode == s->peer->transmode))
                    {
                        WARNING("    transmission mode mismatch");
                        send_notification(s->fd, NOTIF_CODE_ERROR_OPEN,
                            NOTIF_SUBCODE_OPEN_CAP_MISMATCH);
                        return -1;
                    }

                    s->transmode = *transmode;
                } break;
                }

                capinfo_cur += sizeof(capinfo_t) + capinfo->capinfo_len;
            }
        } break;
        }

        opt_cur += sizeof(msg_open_opt_t) + opt->opt_len;
    }

    return 0;

proto_error:
    send_notification_res(s->fd, res);

sock_error:
    return -1;
}


/** \brief Establish TRIP session for both incoming and outgoing connections
 * 
 * Send OPEN, listen for OPEN, decode OPEN, check fields against peers,
 * send KEEPALIVE, receive KEEPALIVE, established the session -> hand off to
 * session loop
 *
 * \param arg Of type (void*){ manager_t *m, session_t *s }
 */
static void *
peer_handshake(void *arg)
{
    manager_t *m = ((void**)arg)[0];
    session_t *s = ((void**)arg)[1];

    int res = 0, toread = 0;
    char buff[MAX_MSG_SIZE];

    /* send OPEN */
    PROTO_TRY(
        new_msg_open(buff, MAX_MSG_SIZE,
            s->peer->hold, m->itad, m->id,
            supported_routetypes, supported_routetypes_size,
            s->peer->transmode),
        res, goto proto_error
    );

    SOCK_TRY_SEND(
        send(s->fd, buff, res, 0),
        goto sock_error
    );
    session_change_state(s, STATE_OPENSENT);


    /* receive OPEN */
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
            if (handle_open(m, s, msg, recv_wnd) < 0)
                goto sock_error;

            PROTO_TRY(
                new_msg_keepalive(buff, MAX_MSG_SIZE),
                res, goto proto_error
            );

            SOCK_TRY_SEND(
                send(s->fd, buff, res, 0),
                goto sock_error
            );

            session_change_state(s, STATE_OPENCONFIRM);
        } break;
        case MSG_TYPE_NOTIFICATION: {
            SOCK_TRY_RECV(s->fd, recv_wnd, msg_notif_t, goto sock_error);

            const msg_notif_t *notif= NULL;
            PROTO_TRY(
                parse_msg_notif(msg->msg_val, res, &notif),
                res, goto proto_error
            );

            DEBUG("error code: %s, error subcode: %s",
                notif_code_strs[notif->notif_error_code],
                notif_code_subcodes_strs[notif->notif_error_code]
                    [notif->notif_error_subcode]);
        } break;
        case MSG_TYPE_KEEPALIVE: {
            if (s->state == STATE_OPENCONFIRM)
                session_change_state(s, STATE_ESTABLISHED);
            time_t now = time(NULL);
            s->established_time = s->last_read_time = now;

            /* Hand newly established session off to session_loop */
            session_loop(arg);

            if (s->mark_stop_init) {
                close(s->fd);
                free(arg);
                return NULL;
            }

            /* If peer disconnects going back to idle, start connect loop */
            s->initiated = 1;
            connect_loop(arg);
        } break;
        default:
            ERROR("unexpected %s message");
            goto sock_error;
        }
    }


proto_error:
    send_notification_res(s->fd, res);

sock_error:
    if (s->mark_stop_init)
        return NULL;
    close(s->fd);
    session_change_state(s, STATE_IDLE);
    return NULL;
}

/* =================== CONNECTION HANDLING  ================================= */

/** \brief Listen and accept connections */
static void *
listen_loop(void *arg)
{
    manager_t *m = arg;

    struct sockaddr_in6 peer_addr = { 0 };
    socklen_t peer_addr_size = sizeof(struct sockaddr_in6);

    while (m->run) {
        /* accept connection (block) */
        int request_fd = accept(m->fd, (struct sockaddr*)&peer_addr,
            &peer_addr_size);
        if (request_fd < 0) {
            ERROR("could not accept() peer: %s", strerror(errno));
            return NULL;
        }

        /* check that connection comes from peer, and that this peer does not
         * have an active session */
        const peer_t *peer = locator_lookup(m->locator, &peer_addr);
        if (!peer) {
            INFO("rejecting unknown peer connection: %s",
                sockaddr_str((struct sockaddr *)&peer_addr));
            close(request_fd);
            continue;
        }

        DEBUG("accepted connection from %s, initiating peer",
            sockaddr_str((struct sockaddr*)&peer_addr));


        /* hand off connection to handshake handler on a new thread */
        session_t *s = malloc(sizeof(session_t));
        memset(s, 0, sizeof(session_t));
        s->peer = peer;
        s->fd = request_fd;
        s->initiated = 0;

        void **handshake_data = malloc(2 * sizeof(void*));
        handshake_data[0] = m;
        handshake_data[1] = s;

        pthread_create(&s->thread, NULL, &peer_handshake, handshake_data);
    }

    return NULL;
}


/** \brief Maintain sessions */
static void *
maintenance_loop(void *arg)
{
    manager_t *m = arg;
    char buff[MAX_MSG_SIZE];

    while (m->run) {
        time_t now = time(NULL);

        for (int i = 0; i < m->sessions_size; i++) {
            session_t *s = m->sessions[i];
            if (s->state != STATE_ESTABLISHED || s->hold == 0)
                continue;

            if (now - s->last_write_time > s->keepalive) {
                int n = new_msg_keepalive(buff, sizeof(buff));

                DEBUG("sending KEEPALIVE");
                SOCK_TRY_SEND(send(s->fd, buff, n, 0), session_shutdown(s));
                s->last_write_time = now;
            }

            if (now - s->last_read_time > s->hold) {
                int n = new_msg_notif(buff, sizeof(buff),
                    NOTIF_CODE_ERROR_EXPIRED, 0, 0, NULL);

                ERROR("session %s hold timer expired %ld",
                    sockaddr6_str(&s->peer->addr), s->last_read_time);
                SOCK_TRY_SEND(send(s->fd, buff, n, 0), session_shutdown(s));
                session_shutdown(s);
            }

        }

        usleep(100000);
    }


    return NULL;
}
    


manager_t *
manager_new(const struct sockaddr_in6 *listen_addr)
{
    static manager_t manager = { 0 };

    if (manager.itad)
        return NULL;

    manager_t *m = &manager;
    memset(m, 0, sizeof(manager_t));

    /* init values */
    m->run = 0;
    m->listen_thread = 0;
    m->maintenance_thread = 0;
    m->fd = 0;

    m->itad = 0;
    m->id = 0;

    m->locator = locator_new();
    m->trib = trib_new();

    m->sessions_size = 0;
    m->sessions_capacity = 16;
    m->sessions = malloc(m->sessions_capacity * sizeof(session_t*));
    memset(m->sessions, 0, m->sessions_capacity * sizeof(session_t*));

    m->connect_retry = TIMER_CONNECT_RETRY;
    m->hold = TIMER_HOLD_TIME;
    m->keepalive = TIMER_KEEPALIVE;
    m->max_purge_time = TIMER_MAX_PURGE_TIME;
    m->disable_time = TIMER_DISABLE_TIME;
    m->min_itad_orig_int = TIMER_MIN_ITAD_ORIG_INT;
    m->min_route_advert_int = TIMER_MIN_ROUTE_ADVERT_INT;

    /* create listen socket */
    m->fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (m->fd < 0) {
        ERROR("could not create listen socket: %s", strerror(errno));
        return NULL;
    }

    if (bind(m->fd, (const struct sockaddr*)listen_addr,
        sizeof(struct sockaddr_in6)) < 0)
    {
        ERROR("could not bind() listen socket: %s", strerror(errno));
        return NULL;
    }

    if (listen(m->fd, SOMAXCONN) < 0) {
        ERROR("could not listen() listen socket: %s", strerror(errno));
        return NULL;
    }

    DEBUG("started session manager, listening at [%s]:%d",
        sockaddr_str((struct sockaddr *)listen_addr),
        ntohs(listen_addr->sin6_port));

    return m;
}


/* ===================== SESSION INITIATION ================================= */

/** \brief Try to establish a TCP channel
 *
 * \param arg Of type (void*){ manager_t *m, session_t *s }
 */
static void *
connect_loop(void *arg)
{
    manager_t *m = ((void**)arg)[0];
    session_t *s = ((void**)arg)[1];

    /* create socket every time we try to connect */
    s->fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

    time_t connect_retry = m->connect_retry;

    while (1) {
        if (s->mark_stop_init) {
            manager_session_remove(m, s);
            return NULL;
        }

        session_change_state(s, STATE_CONNECT);

        int res = connect(s->fd, (struct sockaddr*)&s->peer->addr,
            sizeof(struct sockaddr_in6));

        if (res < 0) {
            ERROR("connect(): %s", strerror(errno));
            session_change_state(s, STATE_IDLE);

            uint64_t retry_left = connect_retry * 1e6;
            while (retry_left && !s->mark_stop_init) {
                usleep(100e3);
                retry_left -= 100e3;
            }

            if (connect_retry < MAX_BACKOFF_CONNECT_RETRY)
                connect_retry *= 2;
            continue;
        }

        break;
    }

    /* TCP channel established, hand off to request handler */
    peer_handshake(arg);
    return NULL;
}


void
manager_add_peer(manager_t *manager, const struct sockaddr_in6 *addr,
    uint32_t itad)
{
    /* add peer to peer locator */
    const peer_t *peer = locator_add(manager->locator, addr, itad,
        manager->hold, CAPINFO_TRANS_SEND_RECV);
    
    /* create session object and hand off to connect loop */
    session_t *s = malloc(sizeof(session_t));
    memset(s, 0, sizeof(session_t));
    s->peer = peer;
    s->state = STATE_IDLE;
    s->initiated = 1;

    /* add session to manager session vector */
    manager_session_add(manager, s);

    void **connect_data = malloc(2 * sizeof(void*));
    connect_data[0] = manager;
    connect_data[1] = s;

    pthread_create(&s->thread, NULL, &connect_loop, connect_data);
}

void
manager_run(manager_t *manager)
{
    manager->run = 1;
    pthread_create(&manager->listen_thread, NULL, &listen_loop, manager);
    pthread_create(&manager->maintenance_thread, NULL, &maintenance_loop,
        manager);
}

void
manager_stop(manager_t *manager)
{
    manager->run = 0;
    shutdown(manager->fd, SHUT_RDWR);
    pthread_join(manager->listen_thread, NULL);
    pthread_join(manager->maintenance_thread, NULL);
}

void
manager_shutdown(manager_t *manager)
{
    DEBUG("beginning shutdown");

    for (size_t i = 0; i < manager->sessions_size; i++) {
        if (!manager->sessions[i])
            continue;
        session_shutdown(manager->sessions[i]);
        manager->sessions[i]->mark_stop_init = 1;
    }
    
    for (size_t i = 0; i < manager->sessions_size; i++) {
        if (!manager->sessions[i])
            continue;
        pthread_join(manager->sessions[i]->thread, NULL);
    }

    for (size_t i = 0; i < manager->sessions_size; i++) {
        if (!manager->sessions[i])
            continue;
        session_destroy(manager->sessions[i]);
    }

    manager_stop(manager);

    DEBUG("shutdown complete");
}

void
manager_destroy(manager_t *manager)
{
    locator_destroy(manager->locator);
    free(manager->sessions);
    manager->itad = 0;
}

