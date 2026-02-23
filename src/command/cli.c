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

/** \file
 * \brief Command line interface 
 */

#include "cli.h"

#include <logging/logging.h>

#include <string.h>

#include <termios.h>
#include <unistd.h>
#include <errno.h>

#define _COMPONENT_ "cli"

static parser_t *g_parser = NULL;
struct termios oldt;

#define BASE_PROMPT "tripd"

const char *ctx_prompts[] = {
    "",
    "(config)",
    "(config-pfxlist)",
    "(config-trip)"
};

void
cli_print_prompt()
{
    if (!g_parser)
        return;

    printf(BASE_PROMPT "%s%c ", ctx_prompts[g_parser->state.ctx],
        ">#"[g_parser->state.enabled]);
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

    while (1) {
        char c = 0;
        if (read(STDIN_FILENO, &c, 1) < 1) {
            ERROR("reading stdin: %s", strerror(errno));
            break;
        }

        if (c == '\n' || (line_ptr - line == 4094)) {
            *line_ptr = '\0';
            write(STDOUT_FILENO, "\n", 1);
            if (line_ptr != line)
                parser_parse_cmd(parser, line);
            cli_print_prompt();
            fflush(stdout);
            line_ptr = line;
        } else if (c == '\t') {
            printf("(autocompletion)\n");
            cli_print_prompt();
            write(STDOUT_FILENO, line, line_ptr - line);
            fflush(stdout);
        } else if (c == '?') {
            printf("(help)\n");
            cli_print_prompt();
            write(STDOUT_FILENO, line, line_ptr - line);
            fflush(stdout);
        } else if (c == 127) {
            if (line_ptr == line)
                continue;
            write(STDOUT_FILENO, "\b \b", 3);
            fflush(stdout);
            line_ptr--;
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
}

