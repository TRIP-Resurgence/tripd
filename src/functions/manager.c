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

/** \file */

#include "manager.h"

#include "locator.h"
#include "session.h"

#include <logging/logging.c>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <pthread.h>

#include <arpa/inet.h>
#include <unistd.h>

#define _COMPONENT_ "manager"

static const char *
sockaddr_str(const struct sockaddr *sa)
{
    static char addr_buff[INET6_ADDRSTRLEN];
    switch (sa->sa_family) {
    case AF_INET6:
        inet_ntop(AF_INET6, &((struct sockaddr_in6 *)sa)->sin6_addr, addr_buff,
            INET6_ADDRSTRLEN);
    break;
    case AF_INET:
        inet_ntop(AF_INET, &((struct sockaddr_in *)sa)->sin_addr, addr_buff,
            INET_ADDRSTRLEN);
    break;
    }
    return addr_buff;
}

static void *
manager_loop(void *arg)
{
    manager_t *m = arg;

    struct sockaddr_in6 peer_addr = { 0 };
    socklen_t peer_addr_size = sizeof(struct sockaddr_in6);

    while (1) {
        /* accept connection (block) */
        int session_fd = accept(m->fd, (struct sockaddr*)&peer_addr,
            &peer_addr_size);
        if (session_fd < 0) {
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
            close(session_fd);
            continue;
        }

        if (m->sessions[idx]) {
            INFO("rejecting existing peer connection: %s",
                sockaddr_str((struct sockaddr *)&peer_addr));
            close(session_fd);
            continue;
        }

        /* create session (run session thread */
        session_t *session = session_new_peer(m->itad, m->id,
            peer->hold, peer->transmode, &peer_addr, peer->itad, session_fd);

        m->sessions[idx] = session; /* save session */
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
        inet_ntop(AF_INET6, &listen_addr->sin6_addr, abuff, sizeof(abuff)),
        ntohs(listen_addr->sin6_port));

    return m;
}


void
manager_add_peer(manager_t *manager, const struct sockaddr_in6 *addr,
    uint32_t itad)
{
    locator_add(manager->locator, addr, itad, manager->hold,
        CAPINFO_TRANS_SEND_RECV);
    
    if (manager->sessions_size + 1 == manager->locator->peers_size) {
        manager->sessions = realloc(manager->sessions,
            manager->locator->peers_size * sizeof(session_t*));
    } else {
        WARNING("manager session vector inconsistent with locator");
        return;
    }

    manager->sessions[manager->sessions_size++] =
        session_new_initiate(manager->itad, manager->id, manager->hold,
            CAPINFO_TRANS_SEND_RECV, addr, itad);
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

