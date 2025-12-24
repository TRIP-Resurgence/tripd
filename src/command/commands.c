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

    commands.c: command actions

*/

#include "commands.h"

#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>



static void
map_addr_inet_inet6(struct sockaddr_in6 *sin6, const struct sockaddr_in *sin)
{
    memset(&sin6->sin6_addr.s6_addr[0], 0x00, 10);  /* 80 0s */
    memset(&sin6->sin6_addr.s6_addr[10], 0xff, 2);  /* 16 1s */
    memcpy(&sin6->sin6_addr.s6_addr[12],            /* 32 IPv4 addr */
        &sin->sin_addr.s_addr,
        sizeof(in_addr_t));
}



int
cmd_end(parser_t *parser, int no, char *args)
{
    parser->state.ctx = CTX_BASE;
    if (parser->state.ctx == CTX_BASE)
        parser->state.enabled = 0;
    return 0;
}

int
cmd_exit(parser_t *parser, int no, char *args)
{
    switch (parser->state.ctx) {
    case CTX_BASE: parser->state.enabled = 0; break;
    case CTX_CONFIG: parser->state.ctx = CTX_BASE; break;
    case CTX_PREFIXLIST: parser->state.ctx = CTX_CONFIG; break;
    case CTX_TRIP: parser->state.ctx = CTX_CONFIG; break;
    default: return -1;
    }
    return 0;
}

/* base context */

int
cmd_enable(parser_t *parser, int no, char *args)
{
    parser->state.enabled = 1;
}

int
cmd_configure(parser_t *parser, int no, char *args)
{
    parser->state.ctx = CTX_CONFIG;
}

int
cmd_show(parser_t *parser, int no, char *args)
{
    /* TODO */
}

/* config context */

int
cmd_config_log(parser_t *parser, int no, char *args)
{
    /* TODO */
}

int
cmd_config_bind(parser_t *parser, int no, char *args)
{
    args = strip(args);

    /* resolve listen address */
    struct addrinfo *listen_addrs;
    int res = getaddrinfo(args, NULL, NULL, &listen_addrs);
    if (res != 0) {
        fprintf(parser->outf, "bind-address: getaddrinfo() error: %s for %s\n",
            gai_strerror(res), args);
        return -1;
    }

    if (listen_addrs->ai_addr->sa_family == AF_INET6) {
        memcpy(&parser->listen_addr, listen_addrs->ai_addr,
            listen_addrs->ai_addrlen);
    } else if (listen_addrs->ai_addr->sa_family == AF_INET) {
        parser->listen_addr.sin6_family = AF_INET6;
        parser->listen_addr.sin6_port = htons(PROTO_TCP_PORT);
        /* map IPv4 into IPv4-mapped IPv6 */
        map_addr_inet_inet6(&parser->listen_addr,
            (struct sockaddr_in *)listen_addrs->ai_addr);
    } else {
        fprintf(parser->outf, "bind-address: unsupported address family: %s\n",
            args);
        return -1;
    }

    /* create session manager */
    parser->manager = manager_new(&parser->listen_addr);
    if (!parser->manager)
        return -1;
}

int
cmd_config_prefixlist(parser_t *parser, int no, char *args)
{
    parser->state.ctx = CTX_PREFIXLIST;
}

int
cmd_config_trip(parser_t *parser, int no, char *args)
{
    if (!parser->manager) {
        fprintf(parser->outf, "bind-address must be set first\n");
        return -1;
    }

    parser->state.ctx = CTX_TRIP;
    
    args = strip(args);
    uint32_t itad = strtoul(args, NULL, 10);

    if (parser->manager->itad != 0 && parser->manager->itad != itad) {
        fprintf(parser->outf,
            "error: changing itad of existing instance unallowed\n");
        return -1;
    }

    parser->manager->itad = itad;
}

/* prefix list context */

int
cmd_config_prefixlist_prefix(parser_t *parser, int no, char *args)
{
    /* TODO */
}

/* trip context */

int
cmd_config_trip_lsid(parser_t *parser, int no, char *args)
{
    if (!parser->manager) {
        fprintf(parser->outf, "bind-address must be set first\n");
        return -1;
    }

    args = strip(args);
    uint32_t lsid = 0;
    if (inet_pton(AF_INET, args, &lsid) == 0) {
        fprintf(parser->outf, "ls-id: invalid id: %s\n", args);
        return -1;
    }

    if (parser->manager->id != 0 && parser->manager->id != lsid) {
        fprintf(parser->outf,
            "error: changing id of existing instance unallowed\n");
        return -1;
    }

    parser->manager->id = lsid;
    
    manager_run(parser->manager);
}

int
cmd_config_trip_timers(parser_t *parser, int no, char *args)
{
    args = strip(args);
    parser->manager->hold = strtoul(args, NULL, 10);
}

int
cmd_config_trip_peer(parser_t *parser, int no, char *args)
{
    args = strip(args);
    char *peer = strtok(args, " ");
    char *remote_itad_arg = strtok(NULL, " ");
    char *remote_itad = strtok(NULL, " ");

    /* check args */
    if (!peer || !remote_itad_arg || !remote_itad ||
        strcmp(remote_itad_arg, "remote-itad") != 0)
    {
        fprintf(parser->outf, "peer: invalid args: %s\n", args);
        return -1;
    }

    /* resolve host */
    struct addrinfo *peer_addrs;
    int res = getaddrinfo(peer, NULL, NULL, &peer_addrs);
    if (res != 0) {
        fprintf(parser->outf, "peer: getaddrinfo() error: %s\n",
            gai_strerror(res));
        return -1;
    }

    struct sockaddr_in6 peer_addr = { 0 };

    if (peer_addrs->ai_addr->sa_family == AF_INET6) {
        memcpy(&peer_addr, peer_addrs->ai_addr, peer_addrs->ai_addrlen);
    } else if (peer_addrs->ai_addr->sa_family == AF_INET) {
        peer_addr.sin6_family = AF_INET6;
        peer_addr.sin6_port = htons(PROTO_TCP_PORT);
        /* map IPv4 into IPv4-mapped IPv6 */
        map_addr_inet_inet6(&peer_addr,
            (struct sockaddr_in *)peer_addrs->ai_addr);
    } else {
        fprintf(parser->outf, "peer: unsupported address family: %s\n", args);
        return -1;
    }


    uint32_t remote_itad_num = strtoul(remote_itad, NULL, 10);

    /* pick first */
    manager_add_peer(parser->manager, &peer_addr, remote_itad_num);
}


