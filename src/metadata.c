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

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "metadata.h"

#define TITLE_METADATA_STRING "title"
#define COLOR_SCHEME_METADATA_STRING "color_scheme"

typedef enum {
    STATE_START,
    STATE_METADATA,
    STATE_END
} ParserState;

typedef struct Metadata {
    const char *path;
    const char *title;
    int color_scheme;
    int content_offset; // The offset of the user content in the file
} Metadata;

typedef struct ParserContext {
    Metadata *metadata;
    
    ParserState state;
} ParserContext;

static void
skip_whitespace(char **line)
{
    while (**line != '\0' && (**line ==' ' || **line == '\t')) line++;
}

static void
handle_state_start(ParserContext *context, char **line)
{
    if (strstr(*line, "---\n") == *line) // Ensure line is at the start of metadata
    {
        *line += 4; // Skip "---\n"
        context->state = STATE_METADATA;
    }
    else
    {
        g_warning ("Unexpected line: %s", *line);
        context->state = STATE_END;
    }
}

static void
handle_state_metadata(ParserContext *context, char **line)
{
    if (strstr(*line, TITLE_METADATA_STRING) != NULL)
    {
        *line += strlen(TITLE_METADATA_STRING) + 1;
        skip_whitespace(line);
        char *title_end = strchr(*line, '\n');
        context->metadata->title = g_strndup(*line, title_end - *line);
        *line = title_end + 1;
    }
    else if (strstr(*line, COLOR_SCHEME_METADATA_STRING) != NULL)
    {
        *line += strlen(COLOR_SCHEME_METADATA_STRING) + 1;
        skip_whitespace(line);
        sscanf (*line, "%d", &context->metadata->color_scheme);
        char *color_scheme_end = strchr(*line, '\n');
        *line = color_scheme_end + 1;   
    }
    else if (strstr(*line, "---\n") == *line) // Ensure line is at the end of metadata
    {
        *line += 4; // Skip "---\n"
        context->state = STATE_END;
    }
    else // Try to skip other metadata lines
    {
        char *line_end = strchr(*line, '\n');
        if (line_end == NULL) // If no newline, set state to end
        {
            *line += strlen(*line);
            context->state = STATE_END;
        }
        else *line = line_end + 1; // Skip the line
    }
}

static void
parse_metadata(ParserContext *context, const char *path)
{
    g_return_if_fail(context != NULL && path != NULL);

    int file_fd = open(path, O_RDONLY);
    if (file_fd < 0)
    {
        g_critical("Failed to open file: %s", path);
        return;
    }

    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0) // Get actual file size
    {
        g_critical("Failed to get file stat: %s", path);
        close(file_fd);
        return;
    }

    char *file_content = mmap(NULL, file_stat.st_size, PROT_READ, MAP_PRIVATE, file_fd, 0);
    if (file_content == MAP_FAILED)
    {
        g_critical("Failed to mmap file: %s", path);
        close(file_fd);
        return;
    }
    char *line_start = file_content;

    while (context->state != STATE_END && line_start < file_content + file_stat.st_size)
    {
        switch (context->state)
        {
        case STATE_START:
            handle_state_start(context, &line_start);
            break;
        case STATE_METADATA:
            handle_state_metadata(context, &line_start);
            break;
        case STATE_END:
        default:
            break;
        }
    }
    context->metadata->content_offset = line_start - file_content;

    munmap(file_content, file_stat.st_size);
    close(file_fd);
}