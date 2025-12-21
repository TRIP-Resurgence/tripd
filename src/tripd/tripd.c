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

    tripd.c: daemon

*/

#include <protocol/protocol.h>

#include <functions/locator.h>
#include <functions/manager.h>

#include <stdio.h>

#include <unistd.h>
#include <arpa/inet.h>


int
main(int arg, char **argv)
{
    printf("tripd\n");

    struct sockaddr_in6 listen_addr = {
        AF_INET6,
        htons(PROTO_TCP_PORT),
        0
    };
    inet_pton(AF_INET6, "[::1]", &listen_addr.sin6_addr);

    manager_t *manager = manager_new(10, 1234, &listen_addr);
    if (!manager)
        return 1;
    
    manager_run(manager);

    while (1) {
        sleep(1000);
    }

    return 0;
}

