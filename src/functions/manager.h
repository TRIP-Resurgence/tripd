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

*/

#ifndef _MANAGER_H
#define _MANAGER_H

#include <netinet/in.h>

#include "session.h"
#include "locator.h"


typedef struct {
    pthread_t   thread;
    int         fd;

    uint32_t    itad;
    uint32_t    id;
    uint16_t    hold;
    locator_t  *locator;

    session_t **sessions;
    size_t      sessions_size;
} manager_t;

/* create manager and bind socket */
manager_t *manager_new(const struct sockaddr_in6 *listen_addr);

void manager_add_peer(manager_t *manager, const struct sockaddr_in6 *addr,
    uint32_t itad);

/* run accept loop in thread */
void manager_run(manager_t *manager);

void manager_stop(manager_t *manager);

void manager_destroy(manager_t *manager);


#endif /* _MANAGER_H */

