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

#include <protocol/protocol.h>

#include <command/parser.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>

#define CONFIG_FILE "../tripd.conf"

int
main(int arg, char **argv)
{
    printf("tripd\n");


    parser_t *parser = parser_init(stdout);

    /* read config */
    parser_parse_cmd(parser, "enable");
    parser_parse_cmd(parser, "configure");

    FILE *conff = fopen(CONFIG_FILE, "r");
    if (!conff) {
        fprintf(stderr, "Error opening config file %s: %s\n",
            CONFIG_FILE, strerror(errno));
        return 1;
    }
    parser_parse_file(parser, conff);
    fclose(conff);

    while (1) {
        sleep(1000);
    }

    return 0;
}

