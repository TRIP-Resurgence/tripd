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

    logging.c: logging

*/

/** \file */

#include "logging.h"

#include <stdarg.h>
#include <string.h>
#include <time.h>

#define COLOR_MAGENTA   "\e[35m"
#define COLOR_BLUE      "\e[94m"
#define COLOR_YELLOW    "\e[93m"
#define COLOR_RED       "\e[91m"
#define COLOR_END       "\e[0m"


static FILE *g_logf = NULL;
static loglevel_t g_loglevel = LOG_DEBUG;

static const char *loglevel_strs[] = {
    COLOR_RED     "ERROR"   COLOR_END,
    COLOR_YELLOW  "WARNING" COLOR_END,
    COLOR_BLUE    "INFO"    COLOR_END,
    COLOR_MAGENTA "DEBUG"   COLOR_END
};



static const char *
basename(const char *s)
{
    const char *sep = strrchr(s, '/');
    if (!sep)
        return s;
    else
        return sep + 1;
}

static const char *
timestr()
{
    static char timestr[256];
    time_t t = time(NULL);
    strftime(timestr, 256, "%F %T", gmtime(&t));
    return timestr;
}


void
logging_init(FILE *logf, loglevel_t loglevel)
{
    g_logf = logf;
    g_loglevel = loglevel;
}

void
logging_log(loglevel_t level, const char *component, const char *fmt, ...)
{
    char logbuff[4096];
    va_list args;

    if (level > g_loglevel)
        return;

    va_start(args, fmt);
    vsnprintf(logbuff, 4096, fmt, args);
    va_end(args);

    fprintf(g_logf, "[%s %s %s] %s\n", timestr(), loglevel_strs[level],
        component, logbuff);
}


void
logging_log_debug(loglevel_t level, const char *component,
    const char *file, const char *func, int line, const char *fmt,
    ...)
{
    char logbuff[4096];
    va_list args;

    if (level > g_loglevel)
        return;

    va_start(args, fmt);
    vsnprintf(logbuff, 4096, fmt, args);
    va_end(args);

    fprintf(g_logf, "[%s %s %s] %s:%s():%d: %s\n", timestr(),
        loglevel_strs[level], component, basename(file), func, line, logbuff);
}


