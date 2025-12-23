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

    command.c: command parser

*/


#include "parser.h"

#include <functions/manager.h>


/* handler function pointer type */
typedef int(*cmd_handler_t)(parser_t *parser, int no, const char *args);

/* command definition type */
typedef struct {
    const char     *cmd;
    cmd_handler_t   cmd_handler;
} cmd_def_t;

/* command definitions per context */
const cmd_def_t cmds[][] = {
    {
        { "end",            &cmd_end },
        { "exit",           &cmd_exit },
        { "enable",         &cmd_enable },
        { "configure",      &cmd_configure },
        { "show",           &cmd_show },
        { NULL,             NULL }
    },
    {
        { "end",            &cmd_end },
        { "exit",           &cmd_exit },
        { "no",             &cmd_no },
        { "log",            &cmd_config_log },
        { "bind-address",   &cmd_config_bind },
        { "trip",           &cmd_config_trip },
        { NULL,             NULL }
    },
    {
        { "end",            &cmd_end },
        { "exit",           &cmd_exit },
        { "no",             &cmd_no },
        { "ls-id",          &cmd_config_trip_lsid },
        { "timers",         &cmd_config_trip_timers },
        { "peer",           &cmd_config_trip_peer },
        { "prefix",         &cmd_config_trip_prefix },
        { NULL,             NULL }
    }
};


static const char *
strip(const char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}


parser_t *
command_parser_init()
{
    static parser_t parser;

    parser.state.state_enabled = 0;
    parser.state.state_ctx = CTX_BASE;

    return &parser;
}


int
parser_parse_cmd(parser_t *parser, const char *cmd)
{
    cmd = strip(cmd);
    const cmd_def_t *ctx_cmds = cmds[parser->state.state_ctx];

    for (size_t i = 0; ctx_cmds[i].cmd; i++)
        if (strcmp(cmd, ctx_cmds[i].cmd) == 0)
            return ctx_cmds[i].cmd_handler(parser, 0,
                cmd + strlen(ctx_cmds[i].cmd));

    fprintf(parser->outf, "[WARNING parser] unknown command: %s\n", cmd);
    return -1;
}

int
parser_parse_file(parser_t *parser, FILE *f)
{
    static char line[4096];

    if (!f)
        return -1;

    while (fgets(line, sizeof(line), f)) {
        parser_parse_cmd(parser, line);
    }

    return 0;
}

