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

#ifndef _CLI_H
#define _CLI_H

/** \file
 * \brief Command line interface
 */

#include "parser.h"

/** \brief Print prompt */
void cli_print_prompt();

/** \brief Command loop */
void cli_run(parser_t *parser);

/** \brief Reset terminal */
void cli_reset();

#endif /* _CLI_H */

