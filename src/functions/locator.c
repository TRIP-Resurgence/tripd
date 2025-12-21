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

    locator.c: configured or discovered peer information

*/

#include "locator.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

static locator_t *g_locator = NULL;


locator_t *
locator_new()
{
    if (g_locator != NULL)
        return NULL;

    g_locator = malloc(sizeof(locator_t));
    /* test peer */
    g_locator->locator_peers = malloc(1 * sizeof(peer_t));
    g_locator->locator_peers_size = 1;

    peer_t testpeer = { 0 };
    inet_pton(AF_INET6, "[::1]", &testpeer.peer_addr);
    testpeer.peer_itad = 10;
    testpeer.peer_hold = 480;

    memcpy(g_locator->locator_peers, &testpeer, sizeof(testpeer));
    return g_locator;
}

int
locator_lookup(locator_t *locator, const peer_t **peer,
    const struct sockaddr_in6 *addr)
{
    peer_t *p = NULL;
    size_t i = 0;
    for (; i < locator->locator_peers_size; i++)
        if (memcmp(&addr->sin6_addr,
            &locator->locator_peers[i].peer_addr.sin6_addr,
            sizeof(addr->sin6_addr)) == 0)
        {
            p = &locator->locator_peers[i];
        }

    *peer = p;
    return p ? (int)i : -1;
}


void
locator_destroy(locator_t *locator)
{
    if (locator != g_locator)
        return;
    free(locator);
    g_locator = NULL;
}


