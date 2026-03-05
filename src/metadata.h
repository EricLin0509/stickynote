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
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TYPE_METADATA (metadata_get_type())

G_DECLARE_FINAL_TYPE(Metadata, metadata, META, DATA, GObject)

/* Create a new metadata object from a file path */
/*
 * @param path The file path to the metadata file
 *
 * @note If `path` is NULL, it will create a new empty metadata object
*/
Metadata *
metadata_new(const gchar *path);

/* Copy a metadata object */
/*
  *@warning This function copy a metadata object and modify the old one as copy one
*/
Metadata *
metadata_copy(Metadata *metadata);

/* Build a file name */
gchar *
metadata_build_file_name(Metadata *metadata);

/* Delete the metadata file */
/*
  * @warning This function will not clear the memory of the metadata object,
  *          you should call `metadata_clear` to free the memory
*/
gboolean
metadata_delete_file(Metadata *metadata);

/* Get the content offset of the metadata file */
int
metadata_get_content_offset(Metadata *metadata);

/* Load the content from the metadata file */
gchar *
metadata_load(Metadata *metadata);

/* Load the content directly to the GtkTextBuffer */
void
metadata_load_direct(Metadata *metadata, GtkTextBuffer *buffer);

/* Get metadata information */
/*
 * @param metadata The metadata object to get data from
 * @param [OUT] color_scheme The color scheme to get
 * @param [OUT] title The title to get
 * @param [OUT] timestamp The timestamp to get
 *
*/
void
metadata_get_data(Metadata *metadata, int *color_scheme, gchar const **title, gchar const **timestamp);

/* Get the file path */
const gchar *
metadata_get_path(Metadata *metadata);

/* Set the file path */
void
metadata_set_path(Metadata *metadata, const gchar *path);

/* Update the metadata object with the given color scheme and title */
/*
 * @param metadata The metadata object to update
 * @param color_scheme The color scheme to set
 * @param title The title to set
 * @param update_timestamp Whether to update the timestamp or not
 *
 * @note If `title` is NULL, it will not update the title
 * @note If `color_scheme` is less than 0, it will not update the color scheme
*/
void
metadata_update(Metadata *metadata, int color_scheme, const gchar *title, gboolean update_timestamp);

/* Save the content to the metadata file */
/*
 * @param metadata The metadata object to save
 * @param content The content to save to the metadata file
 * 
 * @return TRUE if the save is successful, FALSE otherwise
*/
gboolean
metadata_save(Metadata *metadata, const gchar *content);

/* Export the file to a different location */
/*
  @param metadata The metadata object to export
  @param new_file_path The new file path to export to
*/
gboolean
metadata_export(Metadata *metadata, const gchar *new_file_path);

G_END_DECLS

#endif /* METADATA_H */