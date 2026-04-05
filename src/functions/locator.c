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

/** \file */

#include "locator.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

static locator_t g_locator = { 0 };


locator_t *
locator_new()
{
    g_locator.peers_capacity = 64;
    g_locator.peers = malloc(g_locator.peers_capacity * sizeof(peer_t));
    g_locator.peers_size = 0;

    return &g_locator;
}

peer_t *
locator_add(locator_t *locator, const struct sockaddr_in6 *addr,
    uint32_t itad, uint16_t hold, capinfo_transmode_t transmode)
{
    if (locator->peers_size + 1 > locator->peers_capacity) {
        locator->peers_capacity *= 2;
        locator->peers = realloc(locator->peers,
            locator->peers_capacity * sizeof(peer_t));
    }

    peer_t *p= &locator->peers[locator->peers_size++];
    memcpy(&p->addr, addr, sizeof(struct sockaddr_in6));
    p->itad = itad;
    p->hold = hold;
    p->transmode = transmode;
    
    return p;
}

peer_t *
locator_lookup(locator_t *locator, const struct sockaddr_in6 *addr)
{
    peer_t *p = NULL;
    size_t i = 0;
    for (; i < locator->peers_size; i++) {
        if (memcmp(&addr->sin6_addr,
            &locator->peers[i].addr.sin6_addr,
            sizeof(addr->sin6_addr)) == 0)
        {
            p = &locator->peers[i];
            break;
        }
    }

    return p;
}


void
locator_destroy(locator_t *locator)
{
    if (locator != &g_locator)
        return;
    free(locator->peers);
}

