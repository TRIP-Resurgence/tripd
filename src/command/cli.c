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

    cli.c: command line interface

*/

/** \file */

#include "cli.h"

#include "commands.h"
#include <logging/logging.h>

#include <string.h>

#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/param.h>

#define _COMPONENT_ "cli"

static parser_t *g_parser = NULL;
struct termios oldt;

#define BASE_PROMPT "tripd"

const char *ctx_prompts[] = {
    "",
    "(config)",
    "(config-routemap)",
    "(config-trip)"
};

void
cli_print_prompt()
{
    if (!g_parser)
        return;

    fflush(stdout);
    printf(BASE_PROMPT "%s%c ", ctx_prompts[g_parser->state.ctx],
        ">#"[g_parser->state.enabled]);
    fflush(stdout);
}

static int
count_matching(const char *s1, const char *s2)
{
    int c = 0;
    while (*s1 && *s2 && *s1++ == *s2++)
        c++;
    return c;
}

static void
autocomplete(parser_t *parser, char *line, char **line_ptr)
{
    if ((*line_ptr - line) == 0)
        return;

    /* match commands and complete */
    char autocomp[4096];
    autocomp[0] = '\0';

    if ((line < *line_ptr) && *(*line_ptr - 1) == ' ') {
        for (const cmd_def_t *cmd = ctx_cmds[parser->state.ctx];
            cmd->cmd; cmd++)
        {
            if ((strncmp(line, cmd->cmd, (*line_ptr - line) - 1) == 0)
                && cmd->syntax)
            {
                printf("\n%s\n", cmd->syntax);
            }
        }
    } else {
        int complete = 0;

        for (const cmd_def_t *cmd = ctx_cmds[parser->state.ctx];
            cmd->cmd; cmd++)
        {
            if (strncmp(line, cmd->cmd, *line_ptr - line) == 0) {
                if (!autocomp[0])
                    strcpy(autocomp, cmd->cmd);
                else
                    autocomp[count_matching(autocomp, cmd->cmd)] = '\0';

                complete = ((*line_ptr - line) == strlen(cmd->cmd))
                    && (strncmp(line, cmd->cmd, *line_ptr - line) == 0);
            }
        }

        if (complete)
            *(*line_ptr)++ = ' ';
        else {
            size_t comp_len = strlen(autocomp);
            strncpy(line, autocomp, comp_len);
            *line_ptr = line + comp_len;
        }
    }
}

void
cli_run(parser_t *parser)
{
    char line[4096], *line_ptr = line;
    g_parser = parser;

    /* set up terminal */
    struct termios newt;
    if (tcgetattr(STDIN_FILENO, &oldt) < 0)
        ERROR("failed to get terminal attrs");
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) < 0)
        ERROR("failed to set terminal attrs");

    cli_print_prompt();

    char c = 0;
    int esc = 0, csi = 0;
    while (1) {
        if (read(STDIN_FILENO, &c, 1) < 1) {
            ERROR("reading stdin: %s", strerror(errno));
            break;
        }

        /* ignore Fe and CSI sequencies */
        if (esc && (c >= 0x40) && (c <= 0x5f)) {
            if (c == '[') {
                csi = 1;
                esc = 0;
            }
            continue;
        }

        if (csi) {
            if (c >= 0x30 && c <= 0x3f)
                continue;
            else if (c >= 0x20 && c <= 0x2f)
                continue;
            else if (c >= 0x40 && c <= 0x7e) {
                csi = 0;
                continue;
            }
        }

        if (c == '\n' || (line_ptr - line == 4094)) {
            *line_ptr = '\0';
            write(STDOUT_FILENO, "\n", 1);
            if (line_ptr != line)
                parser_parse_cmd(parser, line);
            cli_print_prompt();
            line_ptr = line;
        } else if (c == '\t') {
            autocomplete(parser, line, &line_ptr);

            printf("\r");
            cli_print_prompt();
            write(STDOUT_FILENO, line, line_ptr - line);
            fflush(stdout);
        } else if (c == '?') {
            /* print matching commands */
            printf("\n");
            for (const cmd_def_t *cmd = ctx_cmds[parser->state.ctx];
                cmd->cmd; cmd++)
            {
                if (strncmp(line, cmd->cmd, line_ptr - line) == 0)
                    printf("  %-20s%s\n", cmd->cmd, cmd->desc);
            }

            cli_print_prompt();
            write(STDOUT_FILENO, line, line_ptr - line);
            fflush(stdout);
        } else if (c == 127) {
            if (line_ptr == line)
                continue;
            write(STDOUT_FILENO, "\b \b", 3);
            fflush(stdout);
            line_ptr--;
        } else if (c == '\e') {
            esc = 1;
        } else {
            *line_ptr++ = c;
            if (write(STDOUT_FILENO, &c, 1) < 1) {
                ERROR("writing stdout: %s", strerror(errno));
                break;
            }
        }
    }
}

void
cli_reset()
{
    if (tcsetattr(STDIN_FILENO, TCSANOW, &oldt) < 0)
        ERROR("failed to set terminal attrs");
    printf("\r");
}

