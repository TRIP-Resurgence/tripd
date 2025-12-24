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

#ifndef _LOCATOR_H
#define _LOCATOR_H

#include <protocol/protocol.h>

#include <netinet/in.h>


typedef struct {
    struct sockaddr_in6     addr;
    uint32_t                itad;
    uint16_t                hold;
    capinfo_transmode_t     transmode;
} peer_t;

typedef struct {
    peer_t     *peers;
    size_t      peers_size, peers_capacity;
} locator_t;


/* initialize singleton locator known peer list */
locator_t *locator_new();

/* add a peer */
void locator_add(locator_t *locator, const struct sockaddr_in6 *addr,
    uint32_t itad, uint16_t hold, capinfo_transmode_t transmode);

/* lookup by address */
int locator_lookup(locator_t *locator, const peer_t **peer,
    const struct sockaddr_in6 *addr);

void locator_destroy(locator_t *locator);


#endif /* _LOCATOR_H */

