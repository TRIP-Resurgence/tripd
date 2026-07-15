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

/** \file
 * \brief Main entry point
 */

#include <protocol/protocol.h>
#include <command/parser.h>
#include <command/commands.h>
#include <command/cli.h>
#include <logging/logging.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>

#define DEFAULT_CONFIG_PATH TRIPD_CONFIG


static parser_t *g_parser = NULL;


/** \brief Print CLI usage */
void
print_usage(char *name)
{
    printf("usage: %s [options]\n"
        "  --daemon|-d          daemonize (fork, detach controlling terminal)\n"
        "  --console|-c         provide control console on calling terminal\n"
        "  --config|-C file     config file\n"
        , name);
}

/** \brief SIGINT handler
 *
 * Shutdowns daemon graceously calling shutdown command
 */
void
sigint_handler(int dummy)
{
    g_parser->state.enabled = 1;
    cmd_shutdown(g_parser, 0, NULL);
    exit(0);
}

/** \brief Entry point */
int
main(int argc, char **argv)
{
    signal(SIGINT, sigint_handler);

    printf(
        "tripd " TRIPD_VERSION "\n"
        "Copyright (C) 2025  TRIP Resurgence Project\n"
        "This program comes with ABSOLUTELY NO WARRANTY;\n"
        "This is free software, and you are welcome to redistribute it\n"
        "under certain conditions; type `show license' for details.\n\n");

    char *name = argv[0]; argv++;
    int std_console = 0, daemonize = 0;
    const char *config_path = DEFAULT_CONFIG_PATH;

    while (*argv) {
        if (strcmp(*argv, "--daemon") == 0 || strcmp(*argv, "-d") == 0) {
            argv++;
            daemonize = 1;
        } else if (strcmp(*argv, "--console") == 0 || strcmp(*argv, "-c") == 0) {
            argv++;
            std_console = 1;
        } else if (strcmp(*argv, "--config") == 0 || strcmp(*argv, "-C") == 0) {
            argv++;
            if (*argv) {
                config_path = *argv;
                argv++;
            } else
                goto arg_err;
        } else {
            goto arg_err;
        }
    }

    logging_init(stderr, LOG_DEBUG);    /* initialize in stderr debug */

    /* fork off if prompted */
    if (daemonize) {
        if (daemon(1, 1) < 0)
            fprintf(stderr, "daemon() failed: %s\n", strerror(errno));
    }

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

    if (parser_parse_file(g_parser, conff) < 0)
        return 1;
    
    fclose(conff);
    
    parser_parse_cmd(g_parser, "end");

    if (std_console) {
        /* run interactive command line interface */
        if (cli_run(g_parser))
            return 1;
    }

    /* TODO: control socket */
    while (1)
        sleep(1);

    return 0;

arg_err:
    print_usage(name);
    return 1;
}

