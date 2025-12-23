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

*/

#ifndef _PARSER_H
#define _PARSER_H

#include <functions/manager.h>


/* command structure */
typedef enum {
    CTX_BASE,   /* base context */
    CTX_CONFIG, /* config context */
    CTX_TRIP,   /* TRIP routing context */
} cmd_context_t;

/* parser state */
typedef struct {
    int             state_enabled;
    cmd_context_t   state_ctx;
} parser_state_t;

typedef struct {
    parser_state_t      parser_state;
    FILE               *outf;

    struct sockaddr_in6 parser_listen_addr;
    manager_t          *parser_manager;
} parser_t;


parser_t *parser_init(FILE *outf);
int parser_parse_cmd(parser_t *parser, const char *cmd);
int parser_parse_file(parser_t *parser, FILE *f);


#endif /* _PARSER_H */

