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

    tripd.c: daemon

*/

/** \file */

#include <protocol/protocol.h>

#include <command/parser.h>
#include <command/commands.h>
#include <logging/logging.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>

#define DEFAULT_CONFIG_PATH "/usr/local/etc/tripd.conf"


static parser_t *g_parser = NULL;


/** \brief Print CLI usage */
void
print_usage(char *name)
{
    printf("usage: %s [config file]\n", name);
}

/** \brief SIGINT handler
 *
 * Shutdowns daemon graceously calling shutdown command
 */
void
sigint_handler(int dummy)
{
    cmd_shutdown(g_parser, 0, NULL);
    exit(0);
}

/** \brief Entry point */
int
main(int argc, char **argv)
{
    printf(
        "tripd  Copyright (C) 2025  TRIP Resurgence Project\n"
        "This program comes with ABSOLUTELY NO WARRANTY;\n"
        "This is free software, and you are welcome to redistribute it\n"
        "under certain conditions; type `show license' for details.\n\n");

    if (argc > 2) {
        print_usage(*argv);
        return 1;
    }

    const char *config_path = DEFAULT_CONFIG_PATH;

    if (argc == 2)
        config_path = argv[1];

    signal(SIGINT, sigint_handler);


    logging_init(stderr, LOG_DEBUG);

    g_parser = parser_init(stdout);

    /* read config */
    parser_parse_cmd(g_parser, "enable");
    parser_parse_cmd(g_parser, "configure");

    FILE *conff = fopen(config_path, "r");
    if (!conff) {
        fprintf(stderr, "error opening config file %s: %s\n",
            config_path, strerror(errno));
        return 1;
    }
    parser_parse_file(g_parser, conff);
    fclose(conff);

    while (1) {
        sleep(1000);
    }

    return 0;
}

