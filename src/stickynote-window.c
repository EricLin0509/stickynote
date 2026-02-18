/* stickynote-window.c
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

#include <glib/gi18n.h>
#include <sys/stat.h>

#include "config.h"

#include "stickynote-row.h"
#include "stickynote-window.h"
#include "stickynote-editor-window.h"

#define TIMESTAMP_PARSE_IMPLEMENTATION
#include "timestamp.h"

struct _StickynoteWindow
{
	AdwApplicationWindow  parent_instance;

	/* Template widgets */
	GtkWidget *toolbar;
	GtkButton *new_button;
	AdwStatusPage *status_page;
	GtkListBox *list_box;

	/* Private */
	GApplication *app;
	GHashTable *metadata_table;
	size_t note_num;
};

G_DEFINE_FINAL_TYPE (StickynoteWindow, stickynote_window, ADW_TYPE_APPLICATION_WINDOW)

/* GObject essential methods */

static void
stickynote_window_show_note_status (StickynoteWindow *self)
{
	g_return_if_fail (STICKYNOTE_IS_WINDOW (self));

	if (self->note_num == 0)
	{
		adw_status_page_set_title (self->status_page, gettext ("No notes found"));
		adw_status_page_set_description (self->status_page, gettext ("Create a new note by clicking the 'New' button"));
	}
	else
	{
		adw_status_page_set_title (self->status_page, gettext ("StickyNote"));
		const gchar *should_include_plural = (self->note_num > 1) ? "s" : "";
		g_autofree gchar *description = g_strdup_printf (gettext ("%zu note%s in total"), self->note_num, should_include_plural);
		adw_status_page_set_description (self->status_page, description);
	}
}

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

static gboolean
save_metadata_to_file (Metadata *data, const gchar *content)
{
	metadata_update (data, -1, NULL, TRUE); // Only update the timestamp

	if (metadata_get_path (data) == NULL)
	{
		gboolean is_valid_dir;
		g_autofree gchar *notes_dir = get_note_dir_realpath (&is_valid_dir);
		if (notes_dir == NULL || !is_valid_dir) return FALSE;
		g_autofree gchar *file_name = metadata_build_file_name (data);
		g_autofree gchar *path = g_build_filename (notes_dir, file_name, NULL);
		metadata_set_path (data, path);
	}

	if (!metadata_save (data, content))
	{
		g_critical ("Failed to save file");
		return FALSE;
	}

	return TRUE;
}

static int
gtk_listbox_sort_func (GtkListBoxRow *row1, GtkListBoxRow *row2, gpointer user_data)
{
	const char *time_str1 = stickynote_row_get_subtitle (STICKYNOTE_ROW (row1));
	if (time_str1 == NULL) return 1; // If the first row has no timestamp, it should be placed at the end of the list.
	const char *time_str2 = stickynote_row_get_subtitle (STICKYNOTE_ROW (row2));
	if (time_str2 == NULL) return -1; // If the second row has no timestamp, it should be placed at the beginning of the list.

	return memcmp (time_str2, time_str1, TIME_STRING_LENGTH); // Because the TIME_STRING_FORMAT is fixed, we can use memcmp to compare the timestamps.
}

static void
stickynote_window_open_note (StickynoteWindow *self, gpointer user_data);

static void
gtk_list_box_update_rows (StickynoteWindow *self, Metadata *data)
{
	g_return_if_fail (STICKYNOTE_IS_WINDOW (self) && data != NULL);

	const char *title = NULL;
	const char *timestamp = NULL;
	int color_scheme = 0;
	metadata_get_data (data, &color_scheme, &title, &timestamp);

	int year, month, day, hour, minute, second;
	if (timestamp != NULL)
		timestamp_parse (timestamp, &year, &month, &day, &hour, &minute, &second);

	g_autofree gchar *date_str = g_strdup_printf (TIME_STRING_FORMAT, year, month, day, hour, minute, second); // Convert the timestamp to a string

	GtkWidget *row = metadata_get_user_data (data);

	if (row == NULL) // A new note is added
	{
		self->note_num++;
		row = stickynote_row_new (title, date_str);
		g_signal_connect_swapped (row, "edit-request", G_CALLBACK (stickynote_window_open_note), self);
		g_hash_table_insert (self->metadata_table, row, data);
		metadata_add_user_data (data, row);
		gtk_list_box_prepend (self->list_box, row);
	}
	else
	{
		stickynote_row_set_title (STICKYNOTE_ROW (row), title);
		stickynote_row_set_subtitle (STICKYNOTE_ROW (row), date_str);
	}

	stickynote_row_update_color_scheme (STICKYNOTE_ROW (row), color_scheme);
}

static void
on_stickynote_saved (StickynoteEditorWindow *editor_window, Metadata *data, gchar *content, StickynoteWindow *self)
{
	if (data == NULL) return;

	if (!save_metadata_to_file (data, content)) return;

	stickynote_window_show_note_status (self);

	gtk_list_box_update_rows (self, data);

	gtk_list_box_invalidate_sort (self->list_box); // Sort the list again to reflect the new row.
}

/* This callback MUST be called by `g_signal_connect_swapped` */
static void
stickynote_window_open_note (StickynoteWindow *self, gpointer user_data)
{
	GtkWidget *action_widget = user_data;
	Metadata *data = NULL;

	if (STICKYNOTE_IS_ROW (action_widget)) // User clicked the row, get the metadata from the row.
		data = g_hash_table_lookup (self->metadata_table, action_widget);
	else if (GTK_IS_BUTTON (action_widget)) // User clicked the 'New' button, create a new note.
		data = metadata_new (NULL);

	g_return_if_fail (data != NULL); // Check if the data is valid

	StickynoteEditorWindow *editor_window = stickynote_editor_window_new_full (self->app, data, G_CALLBACK (on_stickynote_saved), self);
	gtk_window_present (GTK_WINDOW (editor_window));
}

static void
stickynote_window_dispose (GObject *object)
{
	StickynoteWindow *self = STICKYNOTE_WINDOW (object);
	
	g_hash_table_unref (self->metadata_table);

	g_clear_pointer (&self->toolbar, gtk_widget_unparent);

	G_OBJECT_CLASS (stickynote_window_parent_class)->dispose (object);
}

static void
stickynote_window_class_init (StickynoteWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = stickynote_window_dispose;

	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gtk_widget_class_set_template_from_resource (widget_class, "/com/ericlin/stickynote/stickynote-window.ui");
	gtk_widget_class_bind_template_callback (widget_class, stickynote_window_open_note);
	gtk_widget_class_bind_template_child (widget_class, StickynoteWindow, toolbar);
	gtk_widget_class_bind_template_child (widget_class, StickynoteWindow, new_button);
	gtk_widget_class_bind_template_child (widget_class, StickynoteWindow, status_page);
	gtk_widget_class_bind_template_child (widget_class, StickynoteWindow, list_box);
}

static void
clear_hash_table_element (gpointer data)
{
	Metadata *metadata = data;

	if (metadata == NULL) return;

	metadata_clear (&metadata);
}

static void
stickynote_window_init_notes (StickynoteWindow *self)
{
	gboolean is_valid_dir;
	g_autofree gchar *notes_dir = get_note_dir_realpath (&is_valid_dir);
	if (notes_dir == NULL || !is_valid_dir) return;

	GDir *dir = g_dir_open (notes_dir, 0, NULL);
	if (dir == NULL) return;

	self->note_num = 0;
	const gchar *file_name;
	while ((file_name = g_dir_read_name (dir)) != NULL)
	{
		g_autofree gchar *path = g_build_filename (notes_dir, file_name, NULL);
		Metadata *data = metadata_new (path);

		if (data == NULL) continue; // Ignore invalid files

		gtk_list_box_update_rows (self, data);
	}

	stickynote_window_show_note_status (self);
	gtk_list_box_invalidate_sort (self->list_box);
}

static void
stickynote_window_init (StickynoteWindow *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));

	gtk_list_box_set_sort_func (self->list_box, gtk_listbox_sort_func, NULL, NULL);

	self->app = g_application_get_default ();

	self->metadata_table = g_hash_table_new_full (
											g_direct_hash,
											g_direct_equal,
											NULL,
											(GDestroyNotify) clear_hash_table_element
											);

	stickynote_window_init_notes (self);
}
