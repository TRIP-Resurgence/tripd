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
#include "db/pib.h"
#include "functions/manager.h"
#include "functions/session.h"
#include "protocol/protocol.h"
#include <ctype.h>
#include <logging/logging.h>
#include <netinet/in.h>
#include <util/util.h>
#include <db/trib.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <time.h>


static const char *set_attr_strs[] = {
    "local-preference",
    "metric",
    "next-hop",
    "itad-path prepend",
};

static const char *access_strs[] = {
    "permit",
    "deny"
};

static const char *af_strs_short[] = {
    "nil",
    "decimal",
    "pentadecimal",
    "e164",
    "trunkgroup",
    "carrier"
};

int
cmd_end(parser_t *parser, int no, char *args)
{
    parser->state.ctx = CTX_ROOT;
    if (parser->state.ctx == CTX_ROOT)
        parser->state.enabled = 0;

    if (parser->state.ctx == CTX_TRIP) {
        trib_update_local(parser->manager->trib);
        trib_update_full(parser->manager->trib);
    }

    return 0;
}

int
cmd_exit(parser_t *parser, int no, char *args)
{
    switch (parser->state.ctx) {
    case CTX_ROOT: parser->state.enabled = 0; break;
    case CTX_CONFIG: parser->state.ctx = CTX_ROOT; break;
    case CTX_ROUTEMAP: parser->state.ctx = CTX_CONFIG; break;
    case CTX_TRIP:
        parser->state.ctx = CTX_CONFIG;
        trib_update_local(parser->manager->trib);
        break;
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

    const char *subcmd = strtok(args, " ");

    if (strcmp(subcmd, "running-config") == 0) {
        printf("soon\n"); /* TODO: this cmd */
    } else if (strcmp(subcmd, "peers") == 0) {
        const locator_t *locator = parser->manager->locator;
        printf("  %8s  %-30s %-6s %-12s\n", "itad", "host", "hold", "transmode");
        for (int i = 0; i < locator->peers_size; i++)
            printf("  %8d  %-30s %-6d %-12s\n", locator->peers[i].itad,
                sockaddr6_str(&locator->peers[i].addr),
                locator->peers[i].hold,
                capinfo_transmode_strs[locator->peers[i].transmode]);
    } else if (strcmp(subcmd, "session") == 0) {
        const manager_t *manager = parser->manager;
        const char *s_addr = strtok(NULL, " ");

        if (!s_addr) {
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
        } else {
            struct sockaddr_in6 addr;
            if (normalize_str_addr(&addr, s_addr) < 0)
                return -1;

            session_t *session =
                manager_session_lookup_address(parser->manager, &addr);

            if (!session) {
                printf("show session: session not found\n");
                return -1;
            }

            /* peer info */
            printf(
                "TRIP peer is %s, remote ITAD %d\n"
                "  transmission mode %s\n"
                "  route map in %s, out %s\n",
                sockaddr6_str(&session->peer->addr),
                session->peer->itad,
                capinfo_transmode_strs[session->peer->transmode],
                session->peer->routemap_in ?
                    session->peer->routemap_in->name : "(undefined)",
                session->peer->routemap_out ?
                    session->peer->routemap_out->name : "(undefined)");

            /* session info */
            char state_time[16], last_read_time[16], last_write_time[16];
            time_since(state_time, session->state_time);
            printf(
                "  TRIP version 1\n"
                "  TRIP state = %s for %s\n"
                "  remote LS ID %s\n",
                session_state_strs[session->state], state_time,
                inaddr_str(session->id)
            );

            if (session->state == STATE_ESTABLISHED) {
                time_since(last_read_time, session->last_read_time);
                time_since(last_write_time, session->last_write_time);
                printf(
                    "  last read %s, last write %s, hold time is %d seconds, "
                    "keepalive interval is %d seconds\n"
                    "  neighbor capabilities:\n",
                    last_read_time, last_write_time, session->hold,
                        session->keepalive
                );
                printf("    route types:\n");
                for (size_t i = 0; i < session->routetypes_count; i++)
                    printf("      %s:%s\n",
                        af_strs[session->routetypes[i].routetype_af],
                        app_proto_str(session->routetypes[i].routetype_app_proto));
            } else
                printf("\n");
        }
    } else if (strcmp(subcmd, "route") == 0) {
        const char *for_s = strtok(NULL, " ");

        if (!for_s) {
            const trib_t *t = parser->manager->trib;
            printf("\tS - static, C - connected, T - TRIP derived\n"
                    "\tE - E.164, D - decimal, P - pentadecimal\n");
            for (int i = 0; i < t->loc_trib.size; i++)
                printf("%c %c %s via %s:%s\n",
                    "TCS"[t->loc_trib.table[i]->type],
                    "DPETC"[t->loc_trib.table[i]->af - 1],
                    t->loc_trib.table[i]->prefix,
                    app_proto_str(t->loc_trib.table[i]->app_proto),
                    t->loc_trib.table[i]->attrs.nexthop);
        } else {
        }
    } else if (strcmp(subcmd, "acl") == 0) {
        const pib_t *pib = parser->manager->pib;
        const char *acl = strtok(NULL, " ");

        for (size_t i = 0; i < pib->acls_size; i++) {
            if (acl && strcmp(pib->acls[i].name, acl) != 0)
                continue;
            for (size_t j = 0; j < pib->acls[i].entries_size; j++) {
                printf("%-20s %-7s %s\n", pib->acls[i].name,
                    access_strs[pib->acls[i].entries[j].deny],
                    pib->acls[i].entries[j].expression);
            }
        }
    } else if (strcmp(subcmd, "route-map") == 0) {
        const pib_t *pib = parser->manager->pib;
        const char *routemap = strtok(NULL, " ");

        for (size_t i = 0; i < pib->routemaps_size; i++) {
            if (routemap && strcmp(pib->routemaps[i].name, routemap) != 0)
                continue;

            printf("%s:\n", pib->routemaps[i].name);

            for (size_t j = 0; j < pib->routemaps[i].size; j++) {
                printf(" statement %s %d:\n",
                    access_strs[pib->routemaps[i].statements[j].deny],
                    pib->routemaps[i].statements[j].seq);
                for (size_t k = 0; k < pib->routemaps[i].statements[j].matchers_size; k++) {
                    printf("  match %s",
                        af_strs_short[pib->routemaps[i].statements[j].matchers[k].af]);
                    for (size_t l = 0; l < pib->routemaps[i].statements[j].matchers[k].size; l++)
                        printf(" %s", pib->routemaps[i].statements[j].matchers[k].acls[l]->name);
                    printf("\n");
                }
                for (size_t k = 0; k < pib->routemaps[i].statements[j].actions_size; k++) {
                    printf("  set %s",
                        set_attr_strs[pib->routemaps[i].statements[j].actions[k].attribute]);
                    switch (pib->routemaps[i].statements[j].actions[k].attribute) {
                    case ROUTEMAP_SET_LOCALPREF:
                    case ROUTEMAP_SET_METRIC:
                    case ROUTEMAP_SET_ITADPATH_PREPEND:
                        printf(" %d\n", pib->routemaps[i].statements[j].actions[k].value);
                        break;
                    case ROUTEMAP_SET_NEXTHOP:
                        printf( "%s %s\n",
                            pib->routemaps[i].statements[j].actions[k].valstr1,
                            pib->routemaps[i].statements[j].actions[k].valstr2);
                        break;
                    }
                }
            }
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
cmd_config_route(parser_t *parser, int no, char *args)
{
    args = strip(args);

    if (strncmp(args, "add ", 4) == 0) {
        args += 4;
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
        if (e->af == 0) {
            printf("route: invalid address family\n");
            return -1;
        }
        e->app_proto = atoappproto(app_proto);
        if (e->app_proto == 0) {
            printf("route: invalid application protocol\n");
            return -1;
        }
        e->prefix = strdup(pfx);
        e->type = ENTRY_TYPE_STATIC;
        e->learn_itad = parser->manager->itad;
        e->learn_lsid = 0;
        e->seq = INITIAL_SEQUENCE_NUMBER;
        e->time = time(NULL);
        e->attrs.use = ATTR_USED_NEXTHOP | ATTR_USED_ADVERTPATH
            | ATTR_USED_ROUTEDPATH; /* tripd always originates with paths */
        e->attrs.withdrawn = 0;
        e->attrs.nextitad = parser->manager->itad;
        e->attrs.nexthop = strdup(srv);
        e->attrs.advertpath = NULL; /* will be realloc()'ed and appended */
        e->attrs.advertpath_size = 0;
        e->attrs.routedpath = NULL; /* will be realloc()'ed and appended */
        e->attrs.routedpath_size = 0;
        e->attrs.atomicaggregate = 0;
        e->attrs.local_pref = 0;
        e->attrs.metric = 0;
        e->attrs.communities = NULL;
        e->attrs.communities_size = 0;
        e->attrs.convertedroute = 0;
        e->sent = 0;

        trib_table_insert(&parser->manager->trib->local_routes, e);
    } else if (strncmp(args, "del ", 4) == 0) {
        /* TODO: this */
    } else {
        printf("route: unrecognized argument\n");
        return -1;
    }

    trib_update_full(parser->manager->trib);

    manager_schedule_update(parser->manager);

    return 0;
}

static int
ispfxdigit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'E');
}

static int
check_acl_expr(const char *expr)
{
    int ispfx = 1;
    const char *p = expr;
    while (*p) {
        if (!ispfxdigit(*p)) {
            ispfx = 0;
            break;
        }
        p++;
    }

    if (ispfx)
        return 0;

    if (*expr != '_')
        return -1;

    p = expr + 1;
    while (*p) {
        if (!ispfxdigit(*p) && *p != 'X' && *p != 'Z' && *p != 'N' && *p != '.')
            return -1;
        p++;
    }

    return 0;
}

int
cmd_config_acl(parser_t *parser, int no, char *args)
{
    args = strip(args);

    char *name = strtok(args, " ");
    char *access = strtok(NULL, " ");
    char *expr = strtok(NULL, " ");

    int deny = 0;
    if (strcmp(access, "permit") == 0)
        deny = 0;
    else if (strcmp(access, "deny") == 0)
        deny = 1;
    else {
        printf("acl: unrecognized access: %s\n", access);
        return -1;
    }


    if (check_acl_expr(expr) < 0) {
        printf("acl: invalid expression\n");
        return -1;
    }


    acl_t *acl = pib_acl_find(parser->manager->pib, name);

    if (!acl)
        acl = pib_acl_new(parser->manager->pib, name);
    else if (acl_find(acl, expr)) {
        printf("acl: entry already exists for expression\n");
        return -1;
    }

    acl_insert(acl, deny, expr);

    return 0;
}

int
cmd_config_routemap(parser_t *parser, int no, char *args)
{
    args = strip(args);

    const char *name = strtok(args, " ");
    const char *access = strtok(NULL, " ");
    const char *seq_s = strtok(NULL, " ");

    int deny = 0;

    if (access) {
        if (strcmp(access, "permit") == 0)
            deny = 0;
        else if (strcmp(access, "deny") == 0)
            deny = 1;
        else {
            printf("route-map: unrecognized access: %s\n", access);
            return -1;
        }
    }

    routemap_t *routemap = pib_routemap_find(parser->manager->pib, name);

    if (!routemap)
        routemap = pib_routemap_new(parser->manager->pib, name, deny);

    uint32_t seq = seq_s ? atoi(seq_s) : 10;
    routemap_statement_t *statement = routemap_statement_find(routemap, seq);

    if (!statement)
        statement = routemap_statement_new(routemap, seq, deny);

    parser->state.ctx = CTX_ROUTEMAP;
    parser->state.routemap_statement = statement;

    return 0;
}

int
cmd_config_trip(parser_t *parser, int no, char *args)
{
    if (!parser->manager) {
        fprintf(parser->outf, "bind-address not set\n");
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
    parser->manager->trib->local_itad = itad;

    return 0;
}

/* routemap context */

int
cmd_config_routemap_match(parser_t *parser, int no, char *args)
{
    args = strip(args);

    const char *af_s = strtok(args, " ");
    int af = atoaf(af_s);
    if (af == 0) {
        printf("match: invalid address family\n");
        return -1;
    }

    routemap_matcher_t *m = routemap_statement_matcher_new(
        parser->state.routemap_statement, af);

    const char *acl_name = NULL;
    while ((acl_name = strtok(NULL, " "))) {
        acl_t *acl = pib_acl_find(parser->manager->pib, acl_name);
        if (!acl) {
            printf("match: access list not found: %s\n", acl_name);
            return -1;
        }

        routemap_matcher_insert(m, acl);
    }

    return 0;
}

int
cmd_config_routemap_set(parser_t *parser, int no, char *args)
{
    routemap_action_t s = { 0 };
    
    args = strip(args);
    const char *attr = strtok(args, " ");
    const char *v1 = strtok(NULL, " ");
    const char *v2 = strtok(NULL, " ");

    if (!attr || !v1) {
        printf("set: attribute and or value required\n");
        return -1;
    }


    if (strcmp(attr, "local-preference") == 0) {
        s.attribute = ROUTEMAP_SET_LOCALPREF;
        s.value = atoi(v1);
    } else if (strcmp(attr, "metric") == 0) {
        s.attribute = ROUTEMAP_SET_METRIC;
        s.value = atoi(v1);
    } else if (strcmp(attr, "next-hop") == 0) {
        if (!v2) {
            printf("set: server required\n");
            return -1;
        }

        s.attribute = ROUTEMAP_SET_NEXTHOP;
        s.valstr1 = strdup(v1);
        s.valstr2 = strdup(v2);
    } else if (strcmp(attr, "next-hop") == 0) {
        if (strcmp(v1, "prepend") != 0) {
            printf("set: unsupported itad-path set\n");
            return -1;
        }

        s.attribute = ROUTEMAP_SET_ITADPATH_PREPEND;
        s.value = atoi(v2);
    } else {
        printf("set: unrecognized attribute: %s\n", args);
        return -1;
    }

    if (routemap_statement_action_find(parser->state.routemap_statement,
        s.attribute))
    {
        printf("set: duplicate attribute %s\n", attr);
        return -1;
    }

    routemap_statement_insert_action(parser->state.routemap_statement, &s);

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
cmd_config_trip_default(parser_t *parser, int no, char *args)
{
    args = strip(args);

    char *attr = strtok(args, " ");
    char *val = strtok(NULL, " ");

    if (strcmp(attr, "local-pref") == 0) {
        parser->manager->def_local_pref = atoi(val);
    } else if (strcmp(attr, "metric") == 0) {
        parser->manager->def_metric = atoi(val);
    } else {
        printf("unrecognized attribute\n");
        return -1;
    }

    return 0;
}

int
cmd_config_trip_peer(parser_t *parser, int no, char *args)
{
    args = strip(args);
    char *peer_s = strtok(args, " ");
    if (!peer_s) {
        fprintf(parser->outf, "peer: must provide a peer\n");
        return -1;
    }

    /* resolve host */
    struct addrinfo *peer_addrs;
    int res = getaddrinfo(peer_s, NULL, NULL, &peer_addrs);
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

    peer_t *peer = manager_peer_find(parser->manager, &peer_addr);

    char *subcmd = strtok(NULL, " ");
    if (!subcmd) {
        fprintf(parser->outf, "peer: must provide a peer\n");
        return -1;
    }

    if (strcmp(subcmd, "remote-itad") == 0) {
        char *remote_itad = strtok(NULL, " ");

        /* check args */
        if (!remote_itad) {
            fprintf(parser->outf, "peer: remote ITAD required\n");
            return -1;
        }

        uint32_t remote_itad_num = strtoul(remote_itad, NULL, 10);

        if (peer) {
            fprintf(parser->outf, "peer: peer exists\n");
            return -1;
        }

        /* pick first */
        manager_peer_add(parser->manager, &peer_addr, remote_itad_num);
    } else if (strcmp(subcmd, "route-map") == 0) {
        char *routemap_name = strtok(NULL, " ");
        if (!routemap_name) {
            fprintf(parser->outf, "peer: route map name required\n");
            return -1;
        }

        routemap_t *routemap = pib_routemap_find(parser->manager->pib,
            routemap_name);
        if (!routemap) {
            fprintf(parser->outf, "peer: route map not found\n");
            return -1;
        }

        char *direction = strtok(NULL, " ");
        if (!direction) {
            fprintf(parser->outf, "peer: directon required\n");
            return -1;
        }

        if (strcmp(direction, "in") == 0)
            peer->routemap_in = routemap;
        else if (strcmp(direction, "out") == 0)
            peer->routemap_out = routemap;
        else {
            fprintf(parser->outf, "peer: unrecognized direction\n");
            return -1;
        }
    } else {
        fprintf(parser->outf, "peer: unrecognized argument: %s\n", subcmd);
        return -1;
    }
    
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
    { "show",           &cmd_show, "show running system information", "show < running-config | peers | session [addr] | route [destination] | acl [name] | route-map [name] >" },
    { "shutdown",       &cmd_shutdown, "shutdown system", NULL },
    { NULL,             NULL, NULL, NULL }
};

const cmd_def_t cmds_config[] = {
    { "end",            &cmd_end, "exit from configure mode", NULL },
    { "exit",           &cmd_exit,"exit current context", NULL },
    { "help",           &cmd_help,"show command help", NULL },
    { "log",            &cmd_config_log, "set log file", "log <log file>" },
    { "bind-address",   &cmd_config_bind, "set bind address and port", "bind-address <addr> <port>" },
    { "route",          &cmd_config_route, "insert route into routing table", "route { add <af> <prefix> <app-proto> <server> | del <af> <prefi> }" },
    { "acl",            &cmd_config_acl, "add acl entry", "acl <acl-name> { permit | deny } <expression>" },
    { "route-map",      &cmd_config_routemap, "define route map", "route-map <map-name> [ permit | deny ]" },
    { "trip",           &cmd_config_trip, "trip configuration", "trip <itad>" },
    { NULL,             NULL, NULL, NULL }
};

const cmd_def_t cmds_routemap[] = {
    { "end",            &cmd_end, "exit from configure mode", NULL },
    { "exit",           &cmd_exit,"exit current context", NULL },
    { "help",           &cmd_help,"show command help", NULL },
    { "match",          &cmd_config_routemap_match, "match routes", "match <af> <acl-name> [ <acl-name> ... ]" },
    { "set",            &cmd_config_routemap_set, "modify routes", "set <attribute> <value> ... see documentation" },
    { NULL,             NULL, NULL, NULL }
};

const cmd_def_t cmds_trip[] = {
    { "end",            &cmd_end, "exit from configure mode", NULL },
    { "exit",           &cmd_exit,"exit current context", NULL },
    { "help",           &cmd_help,"show command help", NULL },
    { "ls-id",          &cmd_config_trip_lsid, "set local id", "ls-id <id in dotted notation" },
    { "timers",         &cmd_config_trip_timers, "set timers", "timers <hold> [keep-alive] [connect-retry] [max-purge-time] [disable-time] [min-itad-orig-int] [min-route-advert-int]" },
    { "default",        &cmd_config_trip_default, "set defaults", "default { local-preference | metric } <value>" },
    { "peer",           &cmd_config_trip_peer, "add peer", "peer <host> { remote-itad <itad> | route-map <map-name> }" },
    { NULL,             NULL, NULL, NULL }
};

const cmd_def_t *ctx_cmds[] = {
    cmds_root,
    cmds_config,
    cmds_routemap,
    cmds_trip
};

