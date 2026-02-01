/* metadata.c
 *
 * Copyright 2025 EricLin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <glib.h>

#define TIMESTAMP_FORMAT "%Y-%m-%dT%H:%M:%S"

#ifdef TIMESTAMP_GET_TIME_IMPLEMENTATION

static inline char *
timestamp_get_time(void)
{
    GDateTime *time_now = g_date_time_new_now_local();
    gchar *timestamp = g_date_time_format(time_now, TIMESTAMP_FORMAT);
    g_date_time_unref(time_now);
    return timestamp;
}

#endif /* TIMESTAMP_GET_TIME_IMPLEMENTATION */

#ifdef TIMESTAMP_PARSE_IMPLEMENTATION

#include <stdio.h>

/* Parse a timestamp string and extract the year, month, day, hour, minute, and second. */
static inline void
timestamp_parse(const char *timestamp, int *year, int *month, int *day, int *hour, int *minute, int *second)
{
    if (sscanf(timestamp, "%4d-%2d-%2dT%2d:%2d:%2d", year, month, day, hour, minute, second) != 6)
    {
        g_critical("Invalid timestamp format: %s", timestamp);
        *year = *month = *day = *hour = *minute = *second = 0;
    }
}

#endif /* TIMESTAMP_PARSE_IMPLEMENTATION */

#endif /* TIMESTAMP_H */