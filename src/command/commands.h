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

#ifndef _COMMANDS_H
#define _COMMANDS_H

#include "parser.h"

/** \file
 * \brief Commands
 */

/* common context */

/** \brief End configuration */
int cmd_end(parser_t *parser, int no, char *args);
/** \brief Exit current context */
int cmd_exit(parser_t *parser, int no, char *args);

/* base context */

/** \brief Enable configuration */
int cmd_enable(parser_t *parser, int no, char *args);
/** \brief Enter configuration */
int cmd_configure(parser_t *parser, int no, char *args);
/** \brief Show stuff */
int cmd_show(parser_t *parser, int no, char *args);
/** \brief Terminate daemon */
int cmd_shutdown(parser_t *parser, int no, char *args);

/* config context */
/** \brief Log file and level */
int cmd_config_log(parser_t *parser, int no, char *args);
/** \brief Bind address */
int cmd_config_bind(parser_t *parser, int no, char *args);
/** \brief Prefix list */
int cmd_config_prefixlist(parser_t *parser, int no, char *args);
/** \brief TRIP routing context */
int cmd_config_trip(parser_t *parser, int no, char *args);

/* prefixlist context */

/** \brief New prefix */
int cmd_config_prefixlist_prefix(parser_t *parser, int no, char *args);

/* trip context */

/** \brief Set Location Server ID */
int cmd_config_trip_lsid(parser_t *parser, int no, char *args);
/** \brief Set timers */
int cmd_config_trip_timers(parser_t *parser, int no, char *args);
/** \brief Configure new peer */
int cmd_config_trip_peer(parser_t *parser, int no, char *args);

#endif /* _COMMANDS_H */

