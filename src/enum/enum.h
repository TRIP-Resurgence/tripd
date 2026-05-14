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
 * \brief ENUM emulation query interface
 */

#ifndef _ENUM_H
#define _ENUM_H

#include <db/trib.h>

#include <sys/types.h>


typedef struct {
    pthread_t           thread;
    struct sockaddr_in6 listen_sa;
    int                 fd;
    int                 run;
    trib_t             *trib;
} enum_t;


enum_t *enum_new(const char *zone, const struct sockaddr_in6 *listen_sa,
    trib_t *trib);
void enum_run(enum_t *en);
void enum_stop(enum_t *en);

#endif /* _ENUM_H */

