/*

    trip: Modern TRIP LS implementation
    Copyright (C) 2026 arf20 (Ángel Ruiz Fernandez)

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

/** \file
 * \brief HTTP API server
 *
 * HTTP server
 */

#ifndef _SERVER_H
#define _SERVER_H

#include <db/trib.h>

#include <stdint.h>
#include <netinet/in.h>


typedef struct {
    pthread_t           thread;
    struct sockaddr_in6 listen_sa;
    int                 fd;
    int                 run;
    trib_t             *trib;
} server_t;


server_t *server_new(const struct sockaddr_in6 *listen_sa, trib_t *trib);
void server_run(server_t *server);
void server_stop(server_t *server);

#endif /* _SERVER_H */

