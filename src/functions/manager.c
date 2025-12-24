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

#include "manager.h"

#include "locator.h"
#include "session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <pthread.h>

#include <arpa/inet.h>
#include <unistd.h>


static void *
manager_loop(void *arg)
{
    manager_t *m = arg;

    struct sockaddr_in6 peer_addr;
    socklen_t peer_addr_size;
    char addr_buff[INET6_ADDRSTRLEN];

    while (1) {
        /* accept connection (block) */
        int session_fd = accept(m->fd, (struct sockaddr*)&peer_addr,
            &peer_addr_size);
        if (session_fd < 0) {
            fprintf(stderr, "[ERROR manager] could not accept peer: %s",
                strerror(errno));
            return NULL;
        }

        /* check that connection comes from peer, and that this peer does not
         * have an active session */
        const peer_t *peer = NULL;
        int idx = locator_lookup(m->locator, &peer, &peer_addr);
        if (!peer) {
            printf("[INFO manager] rejecting unknown peer connection: %s\n",
                inet_ntop(AF_INET6, &peer_addr.sin6_addr, addr_buff,
                INET6_ADDRSTRLEN));
            close(session_fd);
            continue;
        }

        if (m->sessions[idx]) {
            printf("[INFO manager] rejecting existing peer connection: %s\n",
                inet_ntop(AF_INET6, &peer_addr.sin6_addr, addr_buff,
                INET6_ADDRSTRLEN));
            close(session_fd);
            continue;
        }

        /* create session (run session thread */
        session_t *session = session_new_peer(m->itad, m->id,
            peer->hold, peer->transmode, &peer_addr, session_fd);

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
        fprintf(stderr, "[ERROR manager] could not create listen socket: %s\n",
            strerror(errno));
        return NULL;
    }

    if (bind(m->fd, (const struct sockaddr*)listen_addr,
        sizeof(struct sockaddr_in6)) < 0)
    {
        fprintf(stderr, "[ERROR manager] could not bind() listen socket: %s\n",
            strerror(errno));
        return NULL;
    }

    if (listen(m->fd, SOMAXCONN) < 0) {
        fprintf(stderr,
            "[ERROR manager] could not listen() listen socket: %s\n",
            strerror(errno));
        return NULL;
    }

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
        fprintf(stderr, "[WARNING manager] manager session vector "
            "inconsistent with locator\n");
        return;
    }

    manager->sessions[manager->sessions_size - 1] =
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
manager_destroy(manager_t *manager)
{
    locator_destroy(manager->locator);
    free(manager->sessions);
    manager->itad = 0;
}

