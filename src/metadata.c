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

#define TIMESTAMP_GET_TIME_IMPLEMENTATION
#include "timestamp.h"

/* Keyswords for metadata */
#define TIMESTAMP_METADATA_STRING "date"
#define TITLE_METADATA_STRING "title"
#define COLOR_SCHEME_METADATA_STRING "color_scheme"

/* Parser bismasks */
#define TIMESTAMP_PARSED_MASK 0x01
#define TITLE_PARSED_MASK 0x02
#define COLOR_SCHEME_PARSED_MASK 0x04

struct _Metadata {
    GObject parent_instance;

    const char *timestamp;
    const char *path;
    const char *title;
    int color_scheme;
    int content_offset; // The offset of the user content in the file

    void *user_data;
};

G_DEFINE_FINAL_TYPE(Metadata, metadata, G_TYPE_OBJECT);

typedef enum {
    STATE_START,
    STATE_METADATA,
    STATE_METADATA_TIMESTAMP,
    STATE_METADATA_TITLE,
    STATE_METADATA_COLOR_SCHEME,
    STATE_END
} ParserState;

typedef struct ParserContext {
    Metadata *metadata;
    
    ParserState state;
    unsigned short parsed_mask;
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

static void
skip_line(char **line)
{
    char *line_end = strchr(*line, '\n');
    if (line_end == NULL) // If no newline, set state to end
    {
        *line += strlen(*line);
        return;
    }
    else *line = line_end + 1; // Skip the line
}

/* Handle `STATE_START` state */
static void
handle_state_start(ParserContext *context, char **line)
{
    if (strstr(*line, "---\n") == *line) // Ensure line is at the start of metadata
    {
        *line += 4; // Skip "---\n"
        context->state = STATE_METADATA;
        return;
    }

    g_warning ("Unexpected line: %s", *line);
    skip_line(line);
}

/* Handle `STATE_METADATA` state */
static void
handle_state_metadata(ParserContext *context, char **line)
{
    if ((context->parsed_mask & TIMESTAMP_PARSED_MASK) == 0 && // Only parse timestamp if not parsed yet
        (strstr(*line, TIMESTAMP_METADATA_STRING) == *line))
    {
        context->state = STATE_METADATA_TIMESTAMP;
        return;
    }
    else if ((context->parsed_mask & TITLE_PARSED_MASK) == 0 &&
        (strstr(*line, TITLE_METADATA_STRING) == *line))
    {
        context->state = STATE_METADATA_TITLE;
        return;
    }
    else if ((context->parsed_mask & COLOR_SCHEME_PARSED_MASK) == 0 &&
        (strstr(*line, COLOR_SCHEME_METADATA_STRING) == *line))
    {
        context->state = STATE_METADATA_COLOR_SCHEME;
        return;
    }
    else if (strstr(*line, "---\n") == *line) // Ensure line is at the end of metadata
    {
        *line += 4; // Skip "---\n"
        context->state = STATE_END;
        return;
    }

    skip_line(line); // Skip the line if it is not match any metadata keyword
}

/* Handle `STATE_METADATA_TIMESTAMP` state */
static void
handle_state_metadata_timestamp(ParserContext *context, char **line)
{
    g_return_if_fail(context != NULL);

    *line += strlen(TIMESTAMP_METADATA_STRING) + 1;
    skip_whitespace(line);
    char *timestamp_end = strchr(*line, '\n');
    context->metadata->timestamp = g_strndup(*line, timestamp_end - *line);
    *line = timestamp_end + 1;

    context->parsed_mask |= TIMESTAMP_PARSED_MASK;
    context->state = STATE_METADATA;
}

/* Handle `STATE_METADATA_TITLE` state */
static void
handle_state_metadata_title(ParserContext *context, char **line)
{
    g_return_if_fail(context != NULL);

    *line += strlen(TITLE_METADATA_STRING) + 1;
    skip_whitespace(line);
    char *title_end = strchr(*line, '\n');
    context->metadata->title = g_strndup(*line, title_end - *line);
    *line = title_end + 1;

    context->parsed_mask |= TITLE_PARSED_MASK;
    context->state = STATE_METADATA;
}

/* Handle `STATE_METADATA_COLOR_SCHEME` state */
static void
handle_state_metadata_color_scheme(ParserContext *context, char **line)
{
    g_return_if_fail(context != NULL);

    *line += strlen(COLOR_SCHEME_METADATA_STRING) + 1;
    skip_whitespace(line);
    sscanf (*line, "%d", &context->metadata->color_scheme);
    char *color_scheme_end = strchr(*line, '\n');
    *line = color_scheme_end + 1;

    context->parsed_mask |= COLOR_SCHEME_PARSED_MASK;
    context->state = STATE_METADATA;
}

static gboolean
parse_metadata(ParserContext *context, const char *path)
{
    g_return_val_if_fail(context != NULL && path != NULL, FALSE);

    metadata_set_path(context->metadata, path);

    int file_fd = open(context->metadata->path, O_RDONLY);
    if (file_fd < 0)
    {
        g_critical("Failed to open file: %s", context->metadata->path);
        return FALSE;
    }

    ssize_t file_size = get_file_size(file_fd);
    if (file_size == -1)
    {
        g_critical("Failed to get file size");
        close(file_fd);
        return FALSE;
    }

    char *file_content = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, file_fd, 0);
    if (file_content == MAP_FAILED)
    {
        g_critical("Failed to mmap file: %s", context->metadata->path);
        close(file_fd);
        return FALSE;
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
            case STATE_METADATA_TIMESTAMP:
                handle_state_metadata_timestamp(context, &line_start);
                break;
            case STATE_METADATA_TITLE:
                handle_state_metadata_title(context, &line_start);
                break;
            case STATE_METADATA_COLOR_SCHEME:
                handle_state_metadata_color_scheme(context, &line_start);
                break;
            case STATE_END:
            default:
                break;
        }
    }
    context->metadata->content_offset = line_start - file_content;

    munmap(file_content, file_size);
    close(file_fd);

    if (context->state != STATE_END) // If the parser didn't reach the STATE_END state, it means there is an error
    {
        g_critical("Unexpected end of file");
        return FALSE;
    }

    return TRUE;
}

gchar *
metadata_build_file_name(Metadata *metadata)
{
    g_return_val_if_fail(metadata != NULL, NULL);

    if (metadata->title == NULL)
        metadata->title = g_strdup("Untitled"); // Set default title if it is NULL

    return g_strdup_printf("%s_%s.md", metadata->timestamp, metadata->title);
}

/* Get the content offset of the metadata file */
int
metadata_get_content_offset(Metadata *metadata)
{
    return metadata->content_offset;
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

/* Load the content directly to the GtkTextBuffer */
void
metadata_load_direct(Metadata *metadata, GtkTextBuffer *buffer)
{
    g_return_if_fail(metadata != NULL && metadata->path != NULL && buffer != NULL);

    int file_fd = open(metadata->path, O_RDONLY);
    if (file_fd < 0)
    {
        g_critical("Failed to open file: %s", metadata->path);
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
        g_critical("Failed to mmap file: %s", metadata->path);
        close(file_fd);
        return;
    }

    gtk_text_buffer_set_text(buffer, file_content + metadata->content_offset, file_size - metadata->content_offset);

    munmap(file_content, file_size);
    close(file_fd);
}

gboolean
metadata_delete_file(Metadata *metadata)
{
    g_return_val_if_fail(metadata != NULL && metadata->path != NULL, FALSE);

    if (unlink(metadata->path) < 0)
    {
        g_critical("Failed to delete file: %s", metadata->path);
        return FALSE;
    }

    return TRUE;
}

void
metadata_get_data(Metadata *metadata, int *color_scheme, gchar const **title, gchar const **timestamp)
{
    g_return_if_fail(metadata != NULL);

    if (color_scheme != NULL) *color_scheme = metadata->color_scheme;
    if (title != NULL) *title = metadata->title;
    if (timestamp != NULL) *timestamp = metadata->timestamp;
}

const gchar *
metadata_get_path(Metadata *metadata)
{
    g_return_val_if_fail(metadata != NULL, NULL);

    return metadata->path;
}

void
metadata_set_path(Metadata *metadata, const gchar *path)
{
    g_return_if_fail(metadata != NULL && path != NULL);

    if (metadata->path != NULL) g_free((void *)metadata->path);

    metadata->path = g_strdup(path);
}

void
metadata_update(Metadata *metadata, int color_scheme, const gchar *title, gboolean update_timestamp)
{
    g_return_if_fail(metadata != NULL);

    if (color_scheme >= 0) metadata->color_scheme = color_scheme;
    if (title != NULL)
    {
        if (metadata->title != NULL) g_free((void *)(metadata->title));
        metadata->title = g_strdup(title);
    }
    if (update_timestamp)
    {
        char *timestamp = timestamp_get_time();
        if (metadata->timestamp != NULL) g_free((void *)(metadata->timestamp)); // Free the old timestamp
        metadata->timestamp = timestamp; // Update the timestamp
    }
}

gboolean
metadata_save(Metadata *metadata, const gchar *content)
{
    g_return_val_if_fail(metadata != NULL && metadata->path != NULL, FALSE);

    int file_fd = open(metadata->path, O_RDWR | O_CREAT, 0644);

    if (file_fd < 0)
    {
        g_critical("Failed to open file: %s", metadata->path);
        return FALSE;
    }

    char *header_content = g_strdup_printf("---\n%s: %s\n%s: %s\n%s: %d\n---\n",
                                             TIMESTAMP_METADATA_STRING, metadata->timestamp,
                                             TITLE_METADATA_STRING, metadata->title,
                                             COLOR_SCHEME_METADATA_STRING, metadata->color_scheme);
    int header_size = strlen(header_content);
    metadata->content_offset = header_size; // Set the content offset to the end of the header
    int content_size = strlen(content);
    int total_content_size = header_size + content_size; // No need to write '\0' at the end because file has an EOF marker

    if (ftruncate(file_fd, total_content_size) < 0) // Truncate file to the correct size
    {
        g_critical("Failed to truncate file: %s", metadata->path);
        close(file_fd);
        return FALSE;
    }

    char *map = mmap(NULL, total_content_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_fd, 0);

    if (map == MAP_FAILED)
    {
        g_critical("Failed to mmap file: %s", metadata->path);
        close(file_fd);
        return FALSE;
    }

    memcpy(map, header_content, header_size); // First write the header
    memcpy(map + header_size, content, content_size); // Then write the content

    msync(map, total_content_size, MS_SYNC); // Write to disk
    munmap(map, total_content_size); // Unmap the file
    close(file_fd); // Close the file

    return TRUE;
}

void
metadata_add_user_data(Metadata *metadata, void *data)
{
    g_return_if_fail(metadata != NULL);

    metadata->user_data = data;
}

void *
metadata_get_user_data(Metadata *metadata)
{
    g_return_val_if_fail(metadata != NULL, NULL);

    return metadata->user_data;
}

/* ==== GObject initialization methods ==== */

static void
metadata_dispose(GObject *object)
{
    Metadata *metadata = META_DATA(object);

    g_clear_pointer((void **)(&(metadata)->path), g_free);
    g_clear_pointer((void **)(&(metadata)->timestamp), g_free);
    g_clear_pointer((void **)(&(metadata)->title), g_free);

    G_OBJECT_CLASS (metadata_parent_class)->dispose (object);
}

static void
metadata_class_init(MetadataClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = metadata_dispose;
}

static void
metadata_init(Metadata *metadata)
{
    metadata->color_scheme = -1;
    metadata->path = NULL;
    metadata->timestamp = NULL;
    metadata->title = NULL;
    metadata->content_offset = 0;
    metadata->user_data = NULL;
}

Metadata *
metadata_new(const gchar *path)
{
    Metadata *metadata = g_object_new(TYPE_METADATA, NULL);

    if (path == NULL) return metadata; // Return empty metadata if path is NULL

    /* If path is not NULL, parse metadata */
    ParserContext context = {
        .metadata = metadata,
        .parsed_mask = 0,
        .state = STATE_START
    };

    if (!parse_metadata(&context, path))
    {
        g_critical("Failed to parse metadata");
        g_object_unref(metadata);
        return NULL;
    }

    return metadata;
}