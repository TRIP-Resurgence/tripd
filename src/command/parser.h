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

#include <stdio.h>

/** \file
 * \brief Command parser
 */

/** \brief Command context structure */
typedef enum {
    CTX_BASE,       /**< Base context */
    CTX_CONFIG,     /**< Config context */
    CTX_PREFIXLIST, /**< Prefix list context */
    CTX_TRIP,       /**< TRIP routing context */
} cmd_context_t;

/** \brief Parser state */
typedef struct {
    int                 enabled;
    cmd_context_t       ctx;

    uint32_t            itad; /**< TRIP context ITAD */
} parser_state_t;

/** \brief Parser object */
typedef struct {
    parser_state_t      state;
    FILE               *outf;

    struct sockaddr_in6 listen_addr;
    manager_t          *manager;
} parser_t;


/** \brief Skip white characters */
char *strip(char *s);

/** \brief Initialize parser state and output file */
parser_t *parser_init(FILE *outf);
/** \brief Parse single command */
int parser_parse_cmd(parser_t *parser, char *cmd);
/** \brief Parse file */
int parser_parse_file(parser_t *parser, FILE *f);


#endif /* _PARSER_H */

