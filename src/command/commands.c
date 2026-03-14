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

/** \file */

#include "commands.h"

#include "cli.h"
#include "command/parser.h"
#include "functions/manager.h"
#include "functions/session.h"
#include "protocol/protocol.h"
#include <ctype.h>
#include <logging/logging.h>
#include <netinet/in.h>
#include <util/util.h>
#include <trib/trib.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <time.h>


int
cmd_end(parser_t *parser, int no, char *args)
{
    if (parser->state.ctx == CTX_PREFIXLIST)
        trib_update(parser->manager->trib);
    parser->state.ctx = CTX_ROOT;
    if (parser->state.ctx == CTX_ROOT)
        parser->state.enabled = 0;
    return 0;
}

int
cmd_exit(parser_t *parser, int no, char *args)
{
    switch (parser->state.ctx) {
    case CTX_ROOT: parser->state.enabled = 0; break;
    case CTX_CONFIG: parser->state.ctx = CTX_ROOT; break;
    case CTX_PREFIXLIST: {
        parser->state.ctx = CTX_CONFIG;
        trib_update(parser->manager->trib);
    } break;
    case CTX_TRIP: parser->state.ctx = CTX_CONFIG; break;
    default: return -1;
    }
    return 0;
}

int
cmd_help(parser_t *parser, int no, char *args)
{
    for (const cmd_def_t *cmd = ctx_cmds[parser->state.ctx]; cmd->cmd; cmd++)
        printf("  %-20s%s\n", cmd->cmd, cmd->desc);
    return 0;
}

/* root context */

int
cmd_enable(parser_t *parser, int no, char *args)
{
    parser->state.enabled = 1;
    return 0;
}

int
cmd_disable(parser_t *parser, int no, char *args)
{
    parser->state.enabled = 0;
    return 0;
}

int
cmd_configure(parser_t *parser, int no, char *args)
{
    if (!parser->state.enabled) {
        printf("configure: unprivileged\n");
        return -1;
    }

    parser->state.ctx = CTX_CONFIG;
    return 0;
}

void
time_since(char *buff, time_t since)
{
    time_t elapsed = time(NULL) - since;
    snprintf(buff, 256, "%02ld:%02ld:%02ld", elapsed / 3600, (elapsed / 60) % 60,
        elapsed % 60);
}

int
cmd_show(parser_t *parser, int no, char *args)
{
    args = strip(args);
    // show < running-config | peers | sessions | session <host> >
    if (strncmp(args, "running-config", 14) == 0) {
        
    } else if (strncmp(args, "peers", 5) == 0) {
        const locator_t *locator = parser->manager->locator;
        printf("  %8s  %-30s %-6s %-12s\n", "itad", "host", "hold", "transmode");
        for (int i = 0; i < locator->peers_size; i++)
            printf("  %8d  %-30s %-6d %-12s\n", locator->peers[i].itad,
                sockaddr6_str(&locator->peers[i].addr),
                locator->peers[i].hold,
                capinfo_transmode_strs[locator->peers[i].transmode]);
    } else if (strncmp(args, "sessions", 8) == 0) {
        const manager_t *manager = parser->manager;
        printf("  %8s  %-30s %-6s %-12s %-10s\n", "itad", "host", "hold", "id", "state");
        for (int i = 0; i < manager->sessions_size; i++) {
            if (manager->sessions[i]->mark_stop_init)
                continue;
            printf("  %8d  %-30s %-6d %-12s %-10s\n",
                manager->sessions[i]->peer->itad,
                sockaddr6_str(&manager->sessions[i]->peer->addr),
                manager->sessions[i]->hold,
                inaddr_str(manager->sessions[i]->id),
                session_state_strs[manager->sessions[i]->state]);
        }
    } else if (strncmp(args, "session ", 8) == 0) {
        const manager_t *manager = parser->manager;
        struct sockaddr_in6 show_addr;
        if (normalize_str_addr(&show_addr, args + 8) < 0)
            return -1;

        session_t *show_session =
            manager_session_lookup_address(parser->manager, &show_addr);

        if (!show_session) {
            printf("show session: session not found\n");
            return -1;
        }

        printf(
            "TRIP peer is %s, remote ITAD %d\n"
            "  TRIP version 1, remote LS ID %s\n"
            "  TRIP state = %s",
            sockaddr6_str(&show_session->peer->addr), show_session->peer->itad,
            inaddr_str(show_session->id),
            session_state_strs[show_session->state]
        );

        char established[16], last_read[16], last_write[16];
        time_since(established, show_session->established_time);
        time_since(last_read, show_session->last_read_time);
        time_since(last_write, show_session->last_write_time);
        if (show_session->state == STATE_ESTABLISHED)
            printf(
                ", up for %s\n"
                "  last read %s, last write %s, hold time is %d, "
                "keepalive interval is %d seconds\n"
                "  neighbor capabilities:\n",
                established, last_read, last_write,
                show_session->hold, show_session->keepalive
            );
        else
            printf("\n");
    } else if (strncmp(args, "routes", 6) == 0) {
        if (!*strip(args + 6)) {
            const trib_t *t = parser->manager->trib;
            printf("\tS - static, C - connected, T - TRIP derived\n"
                    "\tE - E.164, D - decimal, P - pentadecimal\n");
            for (int i = 0; i < t->loc_trib.size; i++)
                printf("%c %c %s via %s:%s\n",
                    "SCT"[t->loc_trib.table[i]->type],
                    "EDP"[t->loc_trib.table[i]->af - 1],
                    t->loc_trib.table[i]->prefix,
                    app_proto_str(t->loc_trib.table[i]->app_proto),
                    t->loc_trib.table[i]->nexthop);
        } else {
            printf("show route: unrecognized argument\n");
        }
    } else {
        printf("show: unrecognized argument\n");
    }

    return 0;
}

int
cmd_shutdown(parser_t *parser, int no, char *args)
{
    if (!parser->state.enabled) {
        printf("shutdown: unprivileged\n");
        return -1;
    }
    manager_shutdown(parser->manager);
    manager_destroy(parser->manager);
    cli_reset();

    exit(0);
}

/* config context */

int
cmd_config_log(parser_t *parser, int no, char *args)
{
    args = strip(args);

    char *file_str = strtok(args, " ");
    char *level_str = strtok(NULL, " ");

    if (!file_str || !level_str) {
        fprintf(parser->outf, "log: invalid arguments\n");
        return -1;
    }

    FILE *f = stderr;
    if (strcmp(file_str, "stdout") == 0)
        f = stdout;
    else if (strcmp(file_str, "stderr") == 0)
        f = stderr;
    else {
        f = fopen(file_str, "a");
        if (!f) {
            fprintf(parser->outf, "log: error opening file: %s\n",
                strerror(errno));
            return -1;
        }
    }

    loglevel_t level = LOG_DEBUG;
    if (strcmp(file_str, "error") == 0)
        level = LOG_ERROR;
    else if (strcmp(file_str, "warning") == 0)
        level = LOG_WARNING;
    else if (strcmp(file_str, "info") == 0)
        level = LOG_INFO;
    else if (strcmp(file_str, "debug") == 0)
        level = LOG_DEBUG;
    else if (strcmp(file_str, "trace") == 0)
        level = LOG_TRACE;

    logging_init(f, level);

    return 0;
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
        parser->listen_addr.sin6_port = htons(PROTO_TCP_PORT);
    } else if (listen_addrs->ai_addr->sa_family == AF_INET) {
        parser->listen_addr.sin6_family = AF_INET6;
        parser->listen_addr.sin6_port = htons(PROTO_TCP_PORT);
        /* map IPv4 into IPv4-mapped IPv6 */
        map_addr_inet_inet6(&parser->listen_addr,
            (struct sockaddr_in *)listen_addrs->ai_addr);
    } else {
        fprintf(parser->outf, "bind-address: unsupported address family: %s\n",
            args);
        freeaddrinfo(listen_addrs);
        return -1;
    }

    freeaddrinfo(listen_addrs);

    /* create session manager */
    parser->manager = manager_new(&parser->listen_addr);
    if (!parser->manager)
        return -1;

    return 0;
}

int
cmd_config_prefixlist(parser_t *parser, int no, char *args)
{
    parser->state.ctx = CTX_PREFIXLIST;

    return 0;
}

int
cmd_config_trip(parser_t *parser, int no, char *args)
{
    if (!parser->manager) {
        fprintf(parser->outf, "bind-address must be set first\n");
        return -1;
    }

    args = strip(args);
    uint32_t itad = strtoul(args, NULL, 10);

    if (parser->manager->itad != 0 && parser->manager->itad != itad) {
        fprintf(parser->outf,
            "error: changing itad of existing instance unallowed\n");
        return -1;
    }

    parser->state.ctx = CTX_TRIP;
    parser->manager->itad = itad;

    return 0;
}

/* prefix list context */

static int
atoaf(const char *s)
{
    if (strcmp(s, "dec") == 0)
        return AF_DECIMAL;
    else if (strcmp(s, "pentadec") == 0)
        return AF_PENTADECIMAL;
    else if (strcmp(s, "e164") == 0)
        return AF_E164;
    else if (strcmp(s, "trunkgroup") == 0)
        return AF_TRUNKGROUP;
    else if (strcmp(s, "carrier") == 0)
        return AF_CARRIER;
    else return 0;
}

static int
atoappproto(const char *s)
{
    if (strcmp(s, "sip") == 0)
        return APP_PROTO_SIP;
    else if (strcmp(s, "q931") == 0)
        return APP_PROTO_H323_225_0_Q931;
    else if (strcmp(s, "ras") == 0)
        return APP_PROTO_H323_225_0_RAS;
    else if (strcmp(s, "annex-g") == 0)
        return APP_PROTO_H323_225_0_ANNEXG;
    else if (strcmp(s, "iax2") == 0)
        return APP_PROTO_IAX2;
    else return 0;
}

int
cmd_config_prefixlist_prefix(parser_t *parser, int no, char *args)
{
    args = strip(args);

    char *af = strtok(args, " ");
    char *pfx = strtok(NULL, " ");
    char *app_proto = strtok(NULL, " ");
    char *srv = strtok(NULL, " ");

    if (!af || !pfx || !app_proto || !srv) {
        fprintf(parser->outf, "error: invalid route format\n");
        return -1;
    }

    entry_t *e = malloc(sizeof(entry_t));
    e->af = atoaf(af);
    e->app_proto = atoappproto(app_proto);
    e->prefix = strdup(pfx);
    e->type = ENTRY_TYPE_STATIC;
    e->nexthop = strdup(srv);
    e->itad = parser->manager->itad;
    e->lsid = 0;
    e->seq = 0;
    e->time = time(NULL);
    e->local_pref = UINT32_MAX;
    e->metric = UINT32_MAX;
    e->itad_path = NULL;
    e->itad_path_size = 0;
    e->withdrawn= 0;

    trib_table_add(&parser->manager->trib->local_routes, e);

    return 0;
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

    return 0;
}

int
cmd_config_trip_timers(parser_t *parser, int no, char *args)
{
    args = strip(args);

    if (strlen(args) == 0 || !isdigit(*args)) {
        printf("timers: number expected\n");
        return -1;
    }

    char *end = NULL;
    parser->manager->hold = strtoul(args, &end, 10);
    if (end == args) {
        printf("timers: number expected\n");
    }
    if (*end == '\0') return 0;
    parser->manager->keepalive = strtoul(end, &end, 10);
    if (*end == '\0') return 0;
    if (parser->manager->keepalive < 3)
        printf("timers: keepalive too low\n");
    parser->manager->connect_retry = strtoul(end, &end, 10);
    if (*end == '\0') return 0;
    parser->manager->max_purge_time = strtoul(end, &end, 10);
    if (*end == '\0') return 0;
    parser->manager->disable_time = strtoul(end, &end, 10);
    if (*end == '\0') return 0;
    parser->manager->min_itad_orig_int = strtoul(end, &end, 10);
    if (*end == '\0') return 0;
    parser->manager->min_route_advert_int = strtoul(end, &end, 10);
    return 0;
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
        freeaddrinfo(peer_addrs);
        return -1;
    }

    freeaddrinfo(peer_addrs);

    uint32_t remote_itad_num = strtoul(remote_itad, NULL, 10);

    /* pick first */
    manager_add_peer(parser->manager, &peer_addr, remote_itad_num);
    
    return 0;
}


/* command definitions per context */
const cmd_def_t cmds_root[] = {
    { "end",            &cmd_end, "exit from configure mode", NULL },
    { "exit",           &cmd_exit,"exit current context", NULL },
    { "help",           &cmd_help,"show command help", NULL },
    { "enable",         &cmd_enable, "enable privileged commands", NULL },
    { "disable",        &cmd_disable, "disable privileged commands", NULL },
    { "configure",      &cmd_configure, "enter configuration mode", NULL },
    { "show",           &cmd_show, "show running system information", "show < running-config | peers | sessions | session <host> | route >" },
    { "shutdown",       &cmd_shutdown, "shutdown system", NULL },
    { NULL,             NULL, NULL, NULL }
};

const cmd_def_t cmds_config[] = {
    { "end",            &cmd_end, "exit from configure mode", NULL },
    { "exit",           &cmd_exit,"exit current context", NULL },
    { "help",           &cmd_help,"show command help", NULL },
    { "log",            &cmd_config_log, "set log file", "log <log file>" },
    { "bind-address",   &cmd_config_bind, "set bind address and port", "bind-address <addr> <port>" },
    { "prefix-list",    &cmd_config_prefixlist, "define local prefix list", "prefix-list" },
    { "trip",           &cmd_config_trip, "trip configuration", "trip <itad>" },
    { NULL,             NULL, NULL, NULL }
};

const cmd_def_t cmds_prefixlist[] = {
    { "end",            &cmd_end, "exit from configure mode", NULL },
    { "exit",           &cmd_exit,"exit current context", NULL },
    { "help",           &cmd_help,"show command help", NULL },
    { "prefix",         &cmd_config_prefixlist_prefix, "add prefix", "prefix <pfx-type> <prefix> <app-layer-proto> <server>" },
    { NULL,             NULL, NULL, NULL }
};

const cmd_def_t cmds_trip[] = {
    { "end",            &cmd_end, "exit from configure mode", NULL },
    { "exit",           &cmd_exit,"exit current context", NULL },
    { "help",           &cmd_help,"show command help", NULL },
    { "ls-id",          &cmd_config_trip_lsid, "set local id", "ls-id <id in dotted notation" },
    { "timers",         &cmd_config_trip_timers, "set timers", "timers <hold> [keep-alive] [connect-retry] [max-purge-time] [disable-time] [min-itad-orig-int] [min-route-advert-int]" },
    { "peer",           &cmd_config_trip_peer, "add peer", "peer <host> remote-itad <itad>" },
    { NULL,             NULL, NULL, NULL }
};

const cmd_def_t *ctx_cmds[] = {
    cmds_root,
    cmds_config,
    cmds_prefixlist,
    cmds_trip
};

