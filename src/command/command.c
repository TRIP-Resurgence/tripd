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


#include "command.h"

/* command structure */
typedef enum {
    CTX_BASE,   /* base context */
    CTX_CONFIG, /* config context */
    CTX_TRIP,   /* TRIP routing context */
} cmd_context_t;

/* parser state */
static typedef struct {
    int             state_enabled;
    cmd_context_t   state_ctx;
} cmd_state_t state;

/* handler function pointer type */
typedef int(*cmd_handler_t)(cmd_state_t *state, int no, const char *args);

/* command definition type */
typedef struct {
    const char     *cmd;
    cmd_handler_t   cmd_handler;
} cmd_def_t;


/* command definitions per context */
const cmd_def_t cmds[][] = {
    {
        { "enable",         &cmd_end },
        { "enable",         &cmd_enable },
        { "configure",      &cmd_configure },
        { "show",           &cmd_show }
    },
    {
        { "enable",         &cmd_end },
        { "log",            &cmd_config_log },
        { "trip",           &cmd_config_trip }
    },
    {
        { "enable",         &cmd_end },
        { "ls-id",          &cmd_config_trip_lsid },
        { "timers",         &cmd_config_trip_timers },
        { "peer",           &cmd_config_trip_peer }
        { "prefix",         &cmd_config_trip_prefix }
    }
};


static const char *
strip(const char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}


static int
command_parse(const char *cmd)
{
    
}


