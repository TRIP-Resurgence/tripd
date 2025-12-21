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
    struct sockaddr_in6     peer_addr;
    uint32_t                peer_itad;
    uint16_t                peer_hold;
    capinfo_transmode_t     peer_transmode;
} peer_t;

typedef struct {
    peer_t     *locator_peers;
    size_t      locator_peers_size;
} locator_t;


locator_t *locator_new();

int locator_lookup(locator_t *locator, const peer_t **peer,
    const struct sockaddr_in6 *addr);

void locator_destroy(locator_t *locator);


#endif /* _LOCATOR_H */

