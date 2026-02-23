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
    parser.state.ctx = CTX_ROOT;

    parser.outf = outf;

    return &parser;
}


int
parser_parse_cmd(parser_t *parser, char *cmd)
{
    cmd = strip(cmd);
    if (!*cmd || *cmd == '!' || *cmd == '#')
        return 0;

    const cmd_def_t *cmds = ctx_cmds[parser->state.ctx];

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

    for (size_t i = 0; cmds[i].cmd; i++)
        if (strncmp(cmd, cmds[i].cmd, strlen(cmds[i].cmd)) == 0)
            return cmds[i].cmd_handler(parser, no,
                cmd + strlen(cmds[i].cmd));

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

