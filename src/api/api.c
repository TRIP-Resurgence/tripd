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

    api.c: implement api endpoints

*/

/** \file
 * Implements API endpoints
 */

#include "api.h"
#include "db/trib.h"
#include "protocol/protocol.h"

#include <api/http_status.h>
#include <logging/logging.h>
#include <util/util.h>
#include <command/commands.h>

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define _COMPONENT_ "api"

#define BUF_SIZE    65535


static const char *bool_str[] = { "false", "trie" };

static const char *
ast_technology(uint16_t app_proto)
{
    switch (app_proto) {
    case APP_PROTO_SIP: return "PJSIP";
    case APP_PROTO_IAX2: return "IAX2";
    default: return NULL;
    }
}

int
handle_route(int fd, char *query, char *buf, ssize_t req_len, trib_t *trib)
{
    char sendbuf[BUF_SIZE], body[BUF_SIZE];
    table_t *loc = &trib->loc_trib;

    char *num = strtok(query, "/");
    char *qtype = strtok(NULL, "/");

    size_t bodysize = 0;


    if (!num) {
        bodysize += snprintf(body + bodysize, BUF_SIZE - bodysize,
            "%ld routes\n", loc->size);
        for (size_t i = 0; i < loc->size; i++) {
            bodysize += snprintf(body + bodysize, BUF_SIZE - bodysize,
                "%s %s %s\n", af_strs[loc->table[i]->af],
                app_proto_str(loc->table[i]->app_proto), loc->table[i]->prefix);
        }
        goto send;
    } else if (strcmp(num, "full") == 0) {
        /* TODO: send full TRIB dump */
    }

#if 0
    size_t matching_count = 0;
    const entry_t *matching[256];
    for (size_t i = 0; i < loc->size; i++) {
        if (strncmp(num, loc->table[i]->prefix, numlen) == 0)
            matching[matching_count++] = loc->table[i];
    }

    if (matching_count > 1) {
        bodysize += snprintf(body + bodysize, BUF_SIZE - bodysize,
            "%ld routes\n", matching_count);
        for (size_t i = 0; i < matching_count; i++) {
            bodysize += snprintf(body + bodysize, BUF_SIZE - bodysize,
                "%s %s %s\n", af_strs[matching[i]->af],
                app_proto_str(matching[i]->app_proto), matching[i]->prefix);
        }
        goto send;
    }
#endif

    const entry_t *e = trib_table_lookup(&trib->loc_trib, AF_E164, 0, num);
    if (!e) {
        SOCK_TRY_SEND(send(fd, STATUS_404, sizeof(STATUS_404), 0), return -1);
        return 404;
    }

    if (!qtype) {
        char advertpath[4096], routedpath[4096], communities[4096],
            trip[4096], attrs[4096];

        char *ptr = advertpath;
        if (e->attrs.advertpath_size)
            snprintf(ptr, 4096, "%d", e->attrs.advertpath[0]);
        for (size_t i = 0; i < e->attrs.advertpath_size; i++)
            ptr += snprintf(ptr, 4096 - (ptr - advertpath), ",%d",
                e->attrs.advertpath[i]);

        ptr = routedpath;
        if (e->attrs.routedpath_size)
            snprintf(ptr, 4096, "%d", e->attrs.routedpath[0]);
        for (size_t i = 0; i < e->attrs.routedpath_size; i++)
            ptr += snprintf(ptr, 4096 - (ptr - routedpath), ",%d",
                e->attrs.routedpath[i]);

        ptr = communities;
        if (e->attrs.communities_size)
            snprintf(ptr, 4096, "{\"itad\":%d,\"id\":\"%s\"}",
                e->attrs.communities[0].community_itad,
                inaddr_str(e->attrs.communities[0].community_id));
        for (size_t i = 0; i < e->attrs.communities_size; i++)
            ptr += snprintf(ptr, 4096 - (ptr - communities),
                ",{\"itad\":%d,\"id\":\"%s\"}",
                e->attrs.communities[i].community_itad,
                inaddr_str(e->attrs.communities[i].community_id));

        *trip = '\0';
        if (e->type == ENTRY_TYPE_TRIP)
            snprintf(trip, 4096,
                    "\"learn_lsid\":\"%s\","
                    "\"learn_peer\":\"%s\",",
                inaddr_str(e->learn_lsid),
                sockaddr6_str(&e->learn_peer->addr));

        *attrs = '\0';
        ptr = attrs;
        ptr += snprintf(ptr, 4096 - (ptr - attrs),
            "\"withdrawn\":%s,", bool_str[e->attrs.withdrawn]);
        if (ATTR_IS_USED_NEXTHOP(e->attrs.use))
            ptr += snprintf(ptr, 4096 - (ptr - attrs),
                "\"nextitad\":%d,\"nexthop\":\"%s\",",
                e->attrs.nextitad, e->attrs.nexthop);
        if (ATTR_IS_USED_ADVERTPATH(e->attrs.use))
            ptr += snprintf(ptr, 4096 - (ptr - attrs),
                "\"advertpath\":[%s],", advertpath);
        if (ATTR_IS_USED_ROUTEDPATH(e->attrs.use))
            ptr += snprintf(ptr, 4096 - (ptr - attrs),
                "\"routedpath\":[%s],", routedpath);
        ptr += snprintf(ptr, 4096 - (ptr - attrs),
            "\"atomicaggregate\":%s,", bool_str[e->attrs.atomicaggregate]);
        if (ATTR_IS_USED_LOCALPREF(e->attrs.use))
            ptr += snprintf(ptr, 4096 - (ptr - attrs),
                "\"local_pref\":%d,", e->attrs.local_pref);
        if (ATTR_IS_USED_METRIC(e->attrs.use))
            ptr += snprintf(ptr, 4096 - (ptr - attrs),
                "\"metric\":%d,", e->attrs.metric);
        if (ATTR_IS_USED_COMMUNITIES(e->attrs.use))
            ptr += snprintf(ptr, 4096 - (ptr - attrs),
                "\"communities\":[%s],", communities);
        ptr += snprintf(ptr, 4096 - (ptr - attrs),
            "\"convertedroute\":%s", bool_str[e->attrs.convertedroute]);

        bodysize = snprintf(body, BUF_SIZE,
            "{"
                "\"af\":\"%s\","
                "\"app_proto\":\"%s\","
                "\"prefix\":\"%s\","
                "\"type\":\"%s\","
                "\"learn_itad\":%d,"
                "%s"
                "\"seq\":%d,"
                "\"time\":%ld,"
                "\"attrs\":{"
                    "%s"
                "},"
                "\"sent\":\"%d\""
            "}\r\n",
            af_strs[e->af], app_proto_str(e->app_proto), e->prefix,
            (const char*[]){"trip", "connected", "static"}[e->type],
            e->learn_itad, trip, e->seq, e->time, attrs, e->sent);
    } else if (strcmp(qtype,  "af") == 0)
        bodysize = snprintf(body, BUF_SIZE, "%s\r\n", af_strs[e->af]);
    else if (strcmp(qtype, "app-proto") == 0)
        bodysize = snprintf(body, BUF_SIZE, "%s\r\n", app_proto_str(e->app_proto));
    else if (strcmp(qtype, "nexthop-server") == 0)
        bodysize = snprintf(body, BUF_SIZE, "%s\r\n", e->attrs.nexthop);
    else if (strcmp(qtype, "asterisk") == 0) {
        const char *tech = ast_technology(e->app_proto);
        if (!tech) {
            SOCK_TRY_SEND(send(fd, STATUS_422, sizeof(STATUS_422), 0), return -1);
            return 422;
        }
        bodysize = snprintf(body, BUF_SIZE, "%s/%s/%s\r\n", tech,
            e->attrs.nexthop, num);
    } else if (strcmp(qtype, "sip-uri") == 0) {
        if (e->app_proto != APP_PROTO_SIP) {
            SOCK_TRY_SEND(send(fd, STATUS_422, sizeof(STATUS_422), 0), return -1);
            return 422;
        }
        bodysize = snprintf(body, BUF_SIZE, "sip:%s@%s\r\n",
            num, e->attrs.nexthop);
    } else if (strcmp(qtype, "human") == 0) {
        bodysize = route_details(body, BUF_SIZE, e);
    }

send:
    if (bodysize >= BUF_SIZE)
        return 0; /* TODO: body too large 5xx */
    size_t sendsize = snprintf(sendbuf, BUF_SIZE, "%sContent-Length: %ld\r\n\r\n%s",
        STATUS_200, bodysize, body);
    SOCK_TRY_SEND(send(fd, sendbuf, sendsize, 0), return -1);
    return 200;
}

const endpoint_t api[] = {
    { "/route", &handle_route },
    { NULL, NULL }
};

