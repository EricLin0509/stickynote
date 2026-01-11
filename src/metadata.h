/* metadata.h
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

/* A hard-coded YAML style front matter parser for stickynote app */

#ifndef METADATA_H
#define METADATA_H

#include <glib.h>

typedef struct Metadata Metadata;

/* Create a new metadata object from a file path */
/*
 * @param path The file path to the metadata file
 *
 * @note If `path` is NULL, it will create a new empty metadata object
*/
Metadata *
metadata_new(const gchar *path);

/* Free the memory of a metadata object */
void
metadata_clear(Metadata **metadata);

/* Load the content from the metadata file */
gchar *
metadata_load(Metadata *metadata);

/* Update the metadata object with the given color scheme and title */
/*
 * @param metadata The metadata object to update
 * @param color_scheme The color scheme to set
 * @param title The title to set
 *
 * @note If `title` is NULL, it will not update the title
 * @note If `color_scheme` is less than 0, it will not update the color scheme
*/
void
metadata_update(Metadata *metadata, int color_scheme, const gchar *title);

/* Save the content to the metadata file */
/*
 * @param metadata The metadata object to save
 * @param content The content to save to the metadata file
*/
void
metadata_save(Metadata *metadata, const gchar *content);

#endif /* METADATA_H */