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
    while (**line != '\0' && (**line ==' ' || **line == '\t')) (*line)++;
}

static ssize_t
get_file_size(int file_fd)
{
    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0) // Get actual file size
    {
        g_critical("Failed to get file stat");
        return -1;
    }
    return file_stat.st_size;
}

/* Handle `STATE_START` state */
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

/* Handle `STATE_METADATA` state */
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
parse_metadata(ParserContext *context)
{
    g_return_if_fail(context != NULL && context->metadata->path != NULL);

    int file_fd = open(context->metadata->path, O_RDONLY);
    if (file_fd < 0)
    {
        g_critical("Failed to open file: %s", context->metadata->path);
        return;
    }

    ssize_t file_size = get_file_size(file_fd);
    if (file_size == -1)
    {
        close(file_fd);
        return;
    }

    char *file_content = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, file_fd, 0);
    if (file_content == MAP_FAILED)
    {
        g_critical("Failed to mmap file: %s", context->metadata->path);
        close(file_fd);
        return;
    }
    char *line_start = file_content;

    while (context->state != STATE_END && line_start < file_content + file_size)
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

    munmap(file_content, file_size);
    close(file_fd);
}

Metadata *
metadata_new(const gchar *path)
{
    Metadata *metadata = g_new0(Metadata, 1);
    if (path == NULL) return metadata; // Return empty metadata if path is NULL

    /* If path is not NULL, parse metadata */
    metadata->path = g_strdup(path);
    ParserContext context = {
        .metadata = metadata,
        .state = STATE_START
    };
    parse_metadata(&context);

    return metadata;
}

/* Load the content from the metadata file */
gchar *
metadata_load(Metadata *metadata)
{
    g_return_val_if_fail(metadata != NULL && metadata->path != NULL, NULL);

    int file_fd = open(metadata->path, O_RDONLY);
    if (file_fd < 0)
    {
        g_critical("Failed to open file: %s", metadata->path);
        return NULL;
    }

    ssize_t file_size = get_file_size(file_fd);
    if (file_size == -1)
    {
        close(file_fd);
        return NULL;
    }

    char *file_content = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, file_fd, 0);
    if (file_content == MAP_FAILED)
    {
        g_critical("Failed to mmap file: %s", metadata->path);
        close(file_fd);
        return NULL;
    }

    gchar *content = g_strndup(file_content + metadata->content_offset, file_size - metadata->content_offset);

    munmap(file_content, file_size);
    close(file_fd);

    return content;
}

void
metadata_clear(Metadata **metadata)
{
    g_return_if_fail(metadata != NULL && *metadata != NULL);

    g_free((void *)((*metadata)->path));
    g_free((void *)((*metadata)->title));
    g_free(*metadata);
    *metadata = NULL;
}

void
metadata_update(Metadata *metadata, int color_scheme, const gchar *title)
{
    g_return_if_fail(metadata != NULL);

    if (color_scheme > 0) metadata->color_scheme = color_scheme;
    if (title != NULL)
    {
        g_free((void *)((metadata)->title));
        metadata->title = g_strdup(title);
    }
}

void
metadata_save(Metadata *metadata, const gchar *content)
{
    g_return_if_fail(metadata != NULL && metadata->path != NULL);

    int file_fd = open(metadata->path, O_RDWR | O_CREAT, 0644);

    if (file_fd < 0)
    {
        g_critical("Failed to open file: %s", metadata->path);
        return;
    }

    char *header_content = g_strdup_printf("---\n%s: %s\n%s: %d\n---\n",
                                             TITLE_METADATA_STRING, metadata->title,
                                             COLOR_SCHEME_METADATA_STRING, metadata->color_scheme);
    int header_size = strlen(header_content);
    int content_size = strlen(content);
    int total_content_size = header_size + content_size; // No need to write '\0' at the end because file has an EOF marker

    if (ftruncate(file_fd, total_content_size) < 0) // Truncate file to the correct size
    {
        g_critical("Failed to truncate file: %s", metadata->path);
        close(file_fd);
        return;
    }

    char *map = mmap(NULL, total_content_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_fd, 0);

    if (map == MAP_FAILED)
    {
        g_critical("Failed to mmap file: %s", metadata->path);
        close(file_fd);
        return;
    }

    memcpy(map, header_content, header_size); // First write the header
    memcpy(map + header_size, content, content_size); // Then write the content

    msync(map, total_content_size, MS_SYNC); // Write to disk
    munmap(map, total_content_size); // Unmap the file
    close(file_fd); // Close the file
}