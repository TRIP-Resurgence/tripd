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

    parser.c: command parser

*/

/** \file */

#include "parser.h"

#include "commands.h"

#include <functions/manager.h>

#include <string.h>


/* handler function pointer type */
typedef int(*cmd_handler_t)(parser_t *parser, int no, char *args);

/* command definition type */
typedef struct {
    const char     *cmd;
    cmd_handler_t   cmd_handler;
} cmd_def_t;

/* command definitions per context */
const cmd_def_t cmds_base[] = {
    { "end",            &cmd_end },
    { "exit",           &cmd_exit },
    { "enable",         &cmd_enable },
    { "configure",      &cmd_configure },
    { "show",           &cmd_show },
    { "shutdown",       &cmd_shutdown },
    { NULL,             NULL }
};

const cmd_def_t cmds_config[] = {
    { "end",            &cmd_end },
    { "exit",           &cmd_exit },
    { "log",            &cmd_config_log },
    { "bind-address",   &cmd_config_bind },
    { "prefix-list",    &cmd_config_prefixlist },
    { "trip",           &cmd_config_trip },
    { NULL,             NULL }
};

const cmd_def_t cmds_prefixlist[] = {
    { "end",            &cmd_end },
    { "exit",           &cmd_exit },
    { "prefix",         &cmd_config_prefixlist_prefix },
    { NULL,             NULL }
};

const cmd_def_t cmds_trip[] = {
    { "end",            &cmd_end },
    { "exit",           &cmd_exit },
    { "ls-id",          &cmd_config_trip_lsid },
    { "timers",         &cmd_config_trip_timers },
    { "peer",           &cmd_config_trip_peer },
    { NULL,             NULL }
};

const cmd_def_t *cmds[] = {
    cmds_base,
    cmds_config,
    cmds_prefixlist,
    cmds_trip
};


char *
strip(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}


parser_t *
parser_init(FILE *outf)
{
    static parser_t parser;

    parser.state.enabled = 0;
    parser.state.ctx = CTX_BASE;

    parser.outf = outf;

    return &parser;
}


int
parser_parse_cmd(parser_t *parser, char *cmd)
{
    cmd = strip(cmd);
    if (!*cmd || *cmd == '!' || *cmd == '#')
        return 0;

    const cmd_def_t *ctx_cmds = cmds[parser->state.ctx];

    int no = 0;
    if (strcmp("no", cmd) == 0) {
        no = 1;
        cmd = strip(cmd + 2);
    }

    /* TODO: no support */
    if (no) {
        fprintf(parser->outf, "command `no` currently unsupported\n");
        return -1;
    }

    for (size_t i = 0; ctx_cmds[i].cmd; i++)
        if (strncmp(cmd, ctx_cmds[i].cmd, strlen(ctx_cmds[i].cmd)) == 0)
            return ctx_cmds[i].cmd_handler(parser, no,
                cmd + strlen(ctx_cmds[i].cmd));

    fprintf(parser->outf, "unknown command: %s\n", cmd);
    return -1;
}

int
parser_parse_file(parser_t *parser, FILE *f)
{
    static char line[4096];

    if (!f)
        return -1;

    while (fgets(line, sizeof(line), f)) {
        line[strlen(line) - 1] = '\0';
        parser_parse_cmd(parser, line);
    }

    return 0;
}

