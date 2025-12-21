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
        int session_fd = accept(m->manager_fd, (struct sockaddr*)&peer_addr,
            &peer_addr_size);
        if (session_fd < 0) {
            fprintf(stderr, "[ERROR manager] could not accept peer: %s",
                strerror(errno));
            continue;
        }

        /* check that connection comes from peer, and that this peer does not
         * have an active session */
        const peer_t *peer = NULL;
        int idx = locator_lookup(m->manager_locator, &peer, &peer_addr);
        if (!peer) {
            printf("[INFO manager] rejecting unknown peer connection: %s\n",
                inet_ntop(AF_INET6, &peer_addr.sin6_addr, addr_buff,
                INET6_ADDRSTRLEN));
            close(session_fd);
            continue;
        }

        if (m->manager_sessions[idx]) {
            printf("[INFO manager] rejecting existing peer connection: %s\n",
                inet_ntop(AF_INET6, &peer_addr.sin6_addr, addr_buff,
                INET6_ADDRSTRLEN));
            close(session_fd);
            continue;
        }

        /* create session (run session thread */
        session_t *session = session_new_peer(m->manager_itad, m->manager_id,
            peer->peer_hold, peer->peer_transmode, &peer_addr, session_fd);

        m->manager_sessions[idx] = session; /* save session */
    }
}


manager_t *
manager_new(uint32_t itad, uint32_t id, const struct sockaddr_in6 *listen_addr)
{
    manager_t *m = malloc(sizeof(manager_t));
    m->manager_thread = 0;
    m->manager_itad = itad;
    m->manager_id = id;

    m->manager_locator = locator_new();

    m->manager_sessions = malloc(m->manager_locator->locator_peers_size *
        sizeof(session_t*));
    memset(m->manager_sessions, 0, m->manager_locator->locator_peers_size *
        sizeof(session_t*));

    /* create listen socket */
    m->manager_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (m->manager_fd < 0) {
        fprintf(stderr, "[ERROR manager] could not create listen socket: %s\n",
            strerror(errno));
        return NULL;
    }

    if (bind(m->manager_fd, (const struct sockaddr*)listen_addr,
        sizeof(struct sockaddr_in6)) < 0)
    {
        fprintf(stderr, "[ERROR manager] could not bind() listen socket: %s\n",
            strerror(errno));
        return NULL;
    }

    if (listen(m->manager_fd, SOMAXCONN) < 0) {
        fprintf(stderr,
            "[ERROR manager] could not listen() listen socket: %s\n",
            strerror(errno));
        return NULL;
    }

    return m;
}

void
manager_run(manager_t *manager)
{
    pthread_create(&manager->manager_thread, NULL, &manager_loop, manager);
    pthread_detach(manager->manager_thread);
}

void
manager_destroy(manager_t *manager)
{
    locator_destroy(manager->manager_locator);
    free(manager->manager_sessions);
    free(manager);
}

