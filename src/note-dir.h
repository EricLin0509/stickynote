/* note-dir.h
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

#ifndef NOTE_DIR_H
#define NOTE_DIR_H

#include <glib.h>
#include <gio/gio.h>
#include <sys/stat.h>

static inline gboolean
ensure_directories_exist (const gchar *dir_path)
{
	struct stat st;

	if (stat (dir_path, &st) == 0)
	{
		if (S_ISDIR (st.st_mode)) return TRUE; // Directory exists
		else // If the path exists but is not a directory
		{
			g_critical ("%s is not a directory", dir_path);
			return FALSE;
		}
	}

	g_warning ("Directory %s does not exist, creating...", dir_path);

	g_mkdir_with_parents (dir_path, 0755);

	return TRUE;
}

static char *
get_note_dir_realpath (gboolean *is_directory)
{
	gboolean is_valid_dir, *is_valid_dir_ptr;
	is_valid_dir_ptr = is_directory ? is_directory : &is_valid_dir; // If is_directory is NULL, we will allocate memory for it.

	GSettings *setting = g_settings_new ("com.ericlin.stickynote");
	g_autofree gchar *notes_dir = g_settings_get_string (setting, "notes-dir");
    g_object_unref (setting);
	g_autofree gchar *realpath = NULL;

	if (strstr (notes_dir, "../") != NULL) // Check if the path is not a treverse path
	{
		g_critical ("Treverse path is not allowed: %s", notes_dir);
		return NULL;
	}

	if (memcmp (notes_dir, "~/", 2) == 0)
	{
		realpath = g_build_filename (g_get_home_dir (), notes_dir + 2, NULL);
	}
	else if (memcmp (notes_dir, "/", 1) == 0)
	{
		realpath = g_build_filename (notes_dir, NULL);
	}
	else
	{
		realpath = g_build_filename (g_get_current_dir (), notes_dir, NULL);
	}

	*is_valid_dir_ptr = ensure_directories_exist (realpath);

	return g_steal_pointer (&realpath);
}

#endif /* NOTE_DIR_H */