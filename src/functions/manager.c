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
 * the OPEN on a thread to find capabilities and creates a session object and
 * delegates the session to session_run in the existing thread.
 * The manager can also initiate connections.
 */

#include "manager.h"

#include "locator.h"
#include "session.h"

#include <logging/logging.c>
#include <util/util.c>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/param.h>

#include <pthread.h>

#include <arpa/inet.h>
#include <unistd.h>

#define _COMPONENT_ "manager"


typedef struct {
    manager_t              *manager;

    pthread_t               thread;
    int                     peer_idx;
    struct sockaddr_in6    *addr;
    int                     fd;
    session_state_t         state;

    /* discovered */
    uint32_t                id;
    uint32_t                hold;
} request_t;


static void
request_change_state(request_t *r, session_state_t new_state)
{
    char abuff[INET6_ADDRSTRLEN];
    DEBUG("peer (%s):%d request changed state from %s to %s",
        inet_ntop(AF_INET6, &r->addr->sin6_addr, abuff,
            sizeof(abuff)),
        r->manager->locator->peers[r->peer_idx].itad,
        session_state_strs[r->state], session_state_strs[new_state]);

    r->state = new_state;
}


/** /brief for OPEN handler
 *
 * only if res warrants a NOTIFICATION
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



/* =================== REQUEST HANDLING ===================================== */

static int
handle_open(request_t *r, const peer_t *peer, const msg_t *msg, void *recv_wnd)
{
    int res = 0, toread = 0;
    SOCK_TRY_RECV(r->fd, recv_wnd, msg_open_t, goto sock_error);

    const msg_open_t *open = NULL;
    PROTO_TRY(
        parse_msg_open(msg->msg_val, res, &open),
        res, goto proto_error
    );

    DEBUG("OPEN(ver %d, hold %d, itad %d, id %s, opts len %d)",
        open->open_ver, open->open_hold, open->open_itad,
        id_str(open->open_id), open->open_opts_len);

    /* check received OPEN fields */
    if (open->open_itad != peer->itad) {
        ERROR("peer ITAD mismatch");
        send_notification(r->fd, NOTIF_CODE_ERROR_OPEN,
            NOTIF_SUBCODE_OPEN_BAD_ITAD);
        return -1;
    }

    /* TODO collision detection as per RFC
    if (manager_lookup_itad_id(s->itad, open->open_id)) {
        ERROR("peer ID exists in same ITAD");
        send_notification(s, NOTIF_CODE_ERROR_OPEN,
            NOTIF_SUBCODE_OPEN_BAD_ID);
        return -1;
    } */

    r->id = open->open_id;
    r->hold = MIN(peer->hold, open->open_hold);

    size_t opts_toread = open->open_opts_len;
    const void *opt_cur = open->open_opts;
    while (opts_toread) {
        SOCK_TRY_RECV(r->fd, recv_wnd, msg_open_opt_t,
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
                SOCK_TRY_RECV(r->fd, recv_wnd, capinfo_t,
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
                    while (routetypes_toread) {
                        SOCK_TRY_RECV(r->fd, recv_wnd,
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
                    }
                } break;
                case CAPINFO_CODE_TRANSMODE: {
                    SOCK_TRY_RECV(r->fd, recv_wnd,
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
                        (*transmode == peer->transmode))
                    {
                        send_notification(r->fd, NOTIF_CODE_ERROR_OPEN,
                            NOTIF_SUBCODE_OPEN_CAP_MISMATCH);
                        return -1;
                    }

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
    send_notification_res(r->fd, res);

sock_error:
    return -1;
}


/** \brief Establish TRIP session
 * 
 * Send OPEN, listen for OPEN, decode OPEN, check fields against peers,
 * send KEEPALIVE, receive KEEPALIVE, established the session -> hand off to
 * session loop
 */
static void *
request_handler(void *arg)
{
    request_t *req = arg;

    int res = 0, toread = 0;
    char buff[MAX_MSG_SIZE];

    const peer_t *peer = &req->manager->locator->peers[req->peer_idx];

    /* send OPEN */
    PROTO_TRY(
        new_msg_open(buff, MAX_MSG_SIZE,
            peer->hold, req->manager->itad, req->manager->id,
            supported_routetypes, supported_routetypes_size,
            peer->transmode),
        res, goto proto_error
    );

    SOCK_TRY_SEND(
        send(req->fd, buff, res, 0) < 0,
        goto sock_error
    );
    request_change_state(req, STATE_OPENSENT);


    /* receive OPEN */
    while (1) {
        void *recv_wnd = buff;
        /* receive and decode message header */
        SOCK_TRY_RECV(req->fd, recv_wnd, msg_t, goto sock_error);

        const msg_t *msg = NULL;
        PROTO_TRY(
            parse_msg(buff, res, &msg),
            res, goto proto_error
        );

        DEBUG("received msg: %s[%d]", msg_type_strs[msg->msg_type],
            msg->msg_len);

        switch (msg->msg_type) {
        case MSG_TYPE_OPEN: {
            if (handle_open(req, peer, msg, recv_wnd) < 0)
                goto sock_error;

            PROTO_TRY(
                new_msg_keepalive(buff, MAX_MSG_SIZE),
                res, goto proto_error
            );

            SOCK_TRY_SEND(
                send(req->fd, buff, res, 0) < 0,
                goto sock_error
            );

            request_change_state(req, STATE_OPENCONFIRM);
        } break;
        case MSG_TYPE_NOTIFICATION: {
            SOCK_TRY_RECV(req->fd, recv_wnd, msg_notif_t, goto sock_error);

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
            if (req->state == STATE_OPENCONFIRM)
                request_change_state(req, STATE_ESTABLISHED);

            /* create session, insert session into sessions
             * hand off to session loop, free request
             * no constructor because it only occurs here and would be
             * cumbersome */
            session_t *session = malloc(sizeof(session_t));
            session->thread = req->thread;
            session->fd = req->fd;
            session->state = STATE_IDLE;
            session->itad = req->manager->itad;
            session->id = req->manager->id;
            session->hold = req->hold;
            session->transmode = peer->transmode;
            session->addr = req->addr;
            session->peer_itad = peer->itad;
            session->peer_id = req->id;

            req->manager->sessions[req->peer_idx] = session;

            free(req);

            session_loop(session);
        } break;
        default:
            ERROR("unexpected %s message");
            goto sock_error;
        }
    }


proto_error:
    send_notification_res(req->fd, res);

sock_error:
    close(req->fd);
    request_change_state(req, STATE_IDLE);
    free(req->addr);
    free(req);
    return NULL;
}

/* =================== CONNECTION HANDLING  ================================= */

static void *
manager_loop(void *arg)
{
    manager_t *m = arg;

    struct sockaddr_in6 peer_addr = { 0 };
    socklen_t peer_addr_size = sizeof(struct sockaddr_in6);

    while (1) {
        /* accept connection (block) */
        int request_fd = accept(m->fd, (struct sockaddr*)&peer_addr,
            &peer_addr_size);
        if (request_fd < 0) {
            ERROR("could not accept() peer: %s", strerror(errno));
            return NULL;
        }

        /* check that connection comes from peer, and that this peer does not
         * have an active session */
        const peer_t *peer = NULL;
        int idx = locator_lookup(m->locator, &peer, &peer_addr);
        if (!peer) {
            INFO("rejecting unknown peer connection: %s",
                sockaddr_str((struct sockaddr *)&peer_addr));
            close(request_fd);
            continue;
        }

        if (m->sessions[idx]) {
            INFO("rejecting existing peer connection: %s",
                sockaddr_str((struct sockaddr *)&peer_addr));
            close(request_fd);
            continue;
        }

        DEBUG("accepted connection from %s, initiating peer",
            sockaddr_str((struct sockaddr*)&peer_addr));


        /* hand off connection to request handler on a new thread */
        request_t *req = malloc(sizeof(request_t));
        req->manager = m;
        req->peer_idx = idx;
        req->addr = malloc(peer_addr_size);
        memcpy(req->addr, &peer_addr, peer_addr_size);
        req->fd = request_fd;

        pthread_create(&req->thread, NULL, &request_handler, req);
        pthread_detach(req->thread);
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

    m->thread = 0;
    m->itad = 0;
    m->id = 0;

    m->locator = locator_new();

    m->sessions = malloc(m->locator->peers_size * sizeof(session_t*));
    memset(m->sessions, 0, m->locator->peers_size * sizeof(session_t*));

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

    char abuff[INET6_ADDRSTRLEN];
    DEBUG("started session manager, listening at [%s]:%d",
        sockaddr_str(listen_addr),
        ntohs(listen_addr->sin6_port));

    return m;
}


/* ===================== SESSION INITIATION ================================= */

/** \brief Try to establish a TCP channel */
static void *
connect_loop(void *arg)
{
    request_t *req = arg;
    time_t connect_retry = 60;

    int r = 0;
    while (1) {
        request_change_state(req, STATE_CONNECT);

        int res = connect(req->fd,
            (struct sockaddr*)req->addr,
            sizeof(struct sockaddr_in6));

        if (res < 0) {
            ERROR("connect(): %s", strerror(errno));
            request_change_state(req, STATE_IDLE);
            sleep(connect_retry);
            if (connect_retry < 3600)
                connect_retry *= 2;
            continue;
        }

        break;
    }

    /* TCP channel established, hand off to request handler */
    request_handler(arg);
}


void
manager_add_peer(manager_t *manager, const struct sockaddr_in6 *addr,
    uint32_t itad)
{
    /* add peer to peer locator */
    int idx = locator_add(manager->locator, addr, itad, manager->hold,
        CAPINFO_TRANS_SEND_RECV);
    
    if (manager->sessions_size + 1 == manager->locator->peers_size) {
        manager->sessions = realloc(manager->sessions,
            manager->locator->peers_size * sizeof(session_t*));
    } else {
        WARNING("manager session vector inconsistent with locator");
        return;
    }

    /* create session request object and hand off to connect loop */
    request_t *req = malloc(sizeof(request_t));
    req->manager = manager;
    req->peer_idx = idx;
    req->addr = malloc(sizeof(struct sockaddr_in6));
    memcpy(req->addr, addr, sizeof(struct sockaddr_in6));
    req->state = STATE_IDLE;

    req->fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    
    pthread_create(&req->thread, NULL, &connect_loop, req);
}

void
manager_run(manager_t *manager)
{
    pthread_create(&manager->thread, NULL, &manager_loop, manager);
    pthread_detach(manager->thread);
}

void
manager_stop(manager_t *manager)
{
    shutdown(manager->fd, SHUT_RDWR);
}

void
manager_shutdown(manager_t *manager)
{
    DEBUG("beginning shutdown");

    shutdown(manager->fd, SHUT_RDWR);

    for (size_t i = 0; i < manager->sessions_size; i++) {
        if (manager->sessions[i]) {
            session_shutdown(manager->sessions[i]);
            session_destroy(manager->sessions[i]);
        }
    }

    DEBUG("shutdown complete");
}

void
manager_destroy(manager_t *manager)
{
    locator_destroy(manager->locator);
    free(manager->sessions);
    manager->itad = 0;
}

