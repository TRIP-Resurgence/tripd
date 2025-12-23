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

#include <arpa/inet.h>


int
cmd_end(parser_t *parser, int no, const char *args)
{
    parser->state.ctx = CTX_BASE;
    if (parser->state.ctx == CTX_BASE)
        parser->state.enabled = 0;
    return 0;
}

int
cmd_exit(parser_t *parser, int no, const char *args)
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
cmd_enable(parser_t *parser, int no, const char *args)
{
    parser->state.enabled = 1;
}

int
cmd_configure(parser_t *parser, int no, const char *args)
{
    parser->state.ctx = CTX_CONFIG;
}

int
cmd_show(parser_t *parser, int no, const char *args)
{

}

/* config context */

int
cmd_config_log(parser_t *parser, int no, const char *args)
{

}

int
cmd_config_bind(parser_t *parser, int no, const char *args)
{
    args = strip(args);
    parser->listen_addr.sin6_family = AF_INET6;
    parser->listen_addr.sin6_port = htons(PROTO_TCP_PORT);
    return inet_pton(AF_INET6, args, &parser->listen_addr.sin6_addr);
}

int
cmd_config_prefixlist(parser_t *parser, int no, const char *args)
{
    parser->state.ctx = CTX_PREFIXLIST;
}

int
cmd_config_trip(parser_t *parser, int no, const char *args)
{
    parser->state.ctx = CTX_TRIP;
}

/* prefix list context */

int
cmd_config_prefixlist_prefix(parser_t *parser, int no, const char *args)
{

}

/* trip context */

int
cmd_config_trip_lsid(parser_t *parser, int no, const char *args)
{
    parser->manager = manager_new(10, 1234, &parser->listen_addr);
    if (!parser->manager)
        return -1;
    
    manager_run(parser->manager);
}

int
cmd_config_trip_timers(parser_t *parser, int no, const char *args)
{

}

int
cmd_config_trip_peer(parser_t *parser, int no, const char *args)
{

}


