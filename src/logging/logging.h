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

/** \file
 * \brief logging utilities
 */


#ifndef _LOGGING_H
#define _LOGGING_H

#include <stdio.h>


/** \brief error levels */
typedef enum {
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG
} loglevel_t;


/** \brief initialize log file and log level */
void logging_init(FILE *logf, loglevel_t loglevel);

/** \brief log */
void logging_log(loglevel_t level, const char *component,
    const char *format, ...);

/** \brief log with debug info */
void logging_log_debug(loglevel_t level, const char *component,
    const char *file, const char *func, int line, const char *format,
    ...);


/** \brief log an error */
#define ERROR(format, ...)   logging_log_debug(LOG_ERROR, _COMPONENT_, \
    __FILE__, __func__, __LINE__, format, ##__VA_ARGS__);
/** \brief log a warning */
#define WARNING(format, ...) logging_log(LOG_INFO, _COMPONENT_, format, \
    ##__VA_ARGS__);
/** \brief log informational event */
#define INFO(format, ...)    logging_log(LOG_INFO, _COMPONENT_, format, \
    ##__VA_ARGS__);
/** \brief log for debugging purposes */
#define DEBUG(format, ...)   logging_log_debug(LOG_DEBUG, _COMPONENT_, \
    __FILE__, __func__, __LINE__, format, ##__VA_ARGS__);

#endif /* _LOGGING_H */

