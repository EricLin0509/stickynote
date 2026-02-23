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

#include "color-scheme.h"

#define TIMESTAMP_PARSE_IMPLEMENTATION
#include "timestamp.h"

#define EDITOR_WINDOW_NAME "stickynote-editor-window"

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
	AdwDialog *alert_dialog;
	size_t note_num;
	size_t editor_window_num; // If this number is greater than 0, it means that the app is in the process of creating a new note.
};

G_DEFINE_FINAL_TYPE (StickynoteWindow, stickynote_window, ADW_TYPE_APPLICATION_WINDOW)

/* GObject essential methods */

static void
stickynote_window_notify_should_hide (StickynoteWindow *self)
{
	if (self->editor_window_num > 0) // If there are still editor windows opened, set `hide_on_close` to TRUE
	{
		gtk_window_set_hide_on_close (GTK_WINDOW (self), TRUE);
		return;
	}

	// If there are no editor windows opened, set `hide_on_close` to FALSE and present the window.
	gtk_window_set_hide_on_close (GTK_WINDOW (self), FALSE);
	gtk_window_present (GTK_WINDOW (self));
}

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
gtk_list_box_remove_row (StickynoteWindow *self, StickynoteRow *row)
{
	g_return_if_fail (STICKYNOTE_IS_WINDOW (self) && STICKYNOTE_IS_ROW (row));

	g_hash_table_remove (self->metadata_table, row);
	gtk_list_box_remove (self->list_box, GTK_WIDGET (row));
}

static void
stickynote_window_open_note (StickynoteWindow *self, gpointer user_data);

static inline void
perform_delete (StickynoteWindow *self, StickynoteRow *row)
{
	Metadata *data = g_hash_table_lookup (self->metadata_table, row);

	if (data == NULL) return;

	if (!metadata_delete_file (data)) return; // Failed to delete the file

	self->note_num--;

	gtk_list_box_remove_row (self, row); // Remove the row from the list.
	gtk_list_box_invalidate_sort (self->list_box); // Sort the list again to reflect the deletion.

	stickynote_window_show_note_status (self);
}

static void
stickynote_window_alert_choice (AdwAlertDialog *dialog, GAsyncResult *result, gpointer user_data)
{
	StickynoteRow *row = user_data;

	StickynoteWindow *window = STICKYNOTE_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (dialog), STICKYNOTE_TYPE_WINDOW));

	const char *choice = adw_alert_dialog_choose_finish (dialog, result);

	if (memcmp (choice, "cancel", 6) == 0) return; // User clicked 'Cancel'

	perform_delete (window, row); // Delete the note
}

static void
stickynote_window_delete_note (StickynoteWindow *self, gpointer user_data)
{
	GtkWindow *editor_window = g_object_get_data (G_OBJECT (user_data), EDITOR_WINDOW_NAME);

	if (editor_window != NULL)
	{
		gtk_window_present (editor_window); // Bring the editor window to the front.
		return;
	}

	adw_alert_dialog_choose (ADW_ALERT_DIALOG (self->alert_dialog), GTK_WIDGET (self), NULL,
                        (GAsyncReadyCallback) stickynote_window_alert_choice, user_data); // Show the alert dialog
}

static void
gtk_list_box_update_rows (StickynoteWindow *self, Metadata *data, GtkWidget **row_out)
{
	g_return_if_fail (STICKYNOTE_IS_WINDOW (self) && data != NULL);

	const char *title = NULL;
	const char *timestamp = NULL;
	int color_scheme = 0;
	gboolean has_file = FALSE;
	metadata_get_data (data, &color_scheme, &title, &timestamp);
	has_file = metadata_get_path (data) != NULL;

	if (!has_file) // File hasn't been created yet, set the title to "Unsaved Note"
		title = gettext ("Unsaved Note");
	else if (title == NULL || *title == '\0') // If the title is empty, set it to "Untitled"
		title = gettext ("Untitled");

	int year, month, day, hour, minute, second;
	g_autofree gchar *date_str = NULL;
	if (timestamp != NULL)
	{
		timestamp_parse (timestamp, &year, &month, &day, &hour, &minute, &second);
		date_str = g_strdup_printf (TIME_STRING_FORMAT, year, month, day, hour, minute, second); // Convert the timestamp to a string
	}

	GtkWidget *row = metadata_get_user_data (data);

	if (row == NULL) // A new note is added
	{
		self->note_num++;
		row = stickynote_row_new (title, date_str);
		g_signal_connect_swapped (row, "edit-request", G_CALLBACK (stickynote_window_open_note), self);
		g_signal_connect_swapped (row, "delete-request", G_CALLBACK (stickynote_window_delete_note), self);
		g_hash_table_insert (self->metadata_table, row, data);
		metadata_add_user_data (data, row);
		gtk_list_box_prepend (self->list_box, row);
		if (row_out != NULL) *row_out = row; // Return the new row if it's not NULL.
	}
	else
	{
		stickynote_row_set_title (STICKYNOTE_ROW (row), title);
		stickynote_row_set_subtitle (STICKYNOTE_ROW (row), date_str);
	}

	if (!has_file)
		stickynote_row_disable_menu (STICKYNOTE_ROW (row));
	else
		stickynote_row_enable_menu (STICKYNOTE_ROW (row));

	if (color_scheme >= 0 && color_scheme < COLOR_SCHEME_COUNT)
		stickynote_row_update_color_scheme (STICKYNOTE_ROW (row), color_scheme);
}

static void
on_stickynote_saved (StickynoteEditorWindow *editor_window, Metadata *data, const gchar *content, StickynoteWindow *self)
{
	if (data == NULL) return;

	if (!save_metadata_to_file (data, content)) return; // Failed to save the file

	GtkWidget *row = NULL;
	gtk_list_box_update_rows (self, data, &row);

	if (STICKYNOTE_IS_ROW (row))
		g_object_set_data (G_OBJECT (row), EDITOR_WINDOW_NAME, editor_window); // Set the editor window data to the row.

	stickynote_window_show_note_status (self);

	gtk_list_box_invalidate_sort (self->list_box); // Sort the list again to reflect the new row.
}

static gboolean
on_editor_window_closed (StickynoteEditorWindow *self, Metadata *data)
{
	g_return_val_if_fail (STICKYNOTE_IS_EDITOR_WINDOW (self) && data != NULL, TRUE);

	StickynoteRow *row = metadata_get_user_data (data);

	g_return_val_if_fail (STICKYNOTE_IS_ROW (row), TRUE);

	StickynoteWindow *window = STICKYNOTE_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (row), STICKYNOTE_TYPE_WINDOW));

	if (window == NULL) return TRUE; // Don't close the window if it's not a stickynote window.

	if (metadata_get_path (data) != NULL)
	{
		g_object_set_data (G_OBJECT (row), EDITOR_WINDOW_NAME, NULL); // Clear the editor window data
	}
	else
	{
		gtk_list_box_remove_row (window, row); // Remove the row from the list if the note is not ever saved.
		window->note_num--; // Decrement the note number.
	}

	window->editor_window_num--;
	stickynote_window_notify_should_hide (window); // Notify the window that the editor window is closed.

	return FALSE;
}

/* This callback MUST be called by `g_signal_connect_swapped` */
static void
stickynote_window_open_note (StickynoteWindow *self, gpointer user_data)
{
	GtkWidget *action_widget = user_data;
	GtkWidget *row = NULL;
	Metadata *data = NULL;
	StickynoteEditorWindow *editor_window = NULL;

	if (STICKYNOTE_IS_ROW (action_widget)) // User clicked the row, get the metadata from the row.
	{
		row = GTK_WIDGET (action_widget);
		data = g_hash_table_lookup (self->metadata_table, row);
		editor_window = g_object_get_data (G_OBJECT (row), EDITOR_WINDOW_NAME);
	}
	else if (GTK_IS_BUTTON (action_widget)) // User clicked the 'New' button, create a new note.
		data = metadata_new (NULL);

	g_return_if_fail (data != NULL); // Check if the data is valid

	if (editor_window == NULL)
	{
		if (row == NULL) gtk_list_box_update_rows (self, data, &row); // If the row is NULL, it means that a new note is added.
		editor_window = stickynote_editor_window_new_full (self->app, data, G_CALLBACK (on_stickynote_saved), self);
		stickynote_editor_window_connect_signal (editor_window, "close-request", G_CALLBACK (on_editor_window_closed), data);
		g_object_set_data (G_OBJECT (row), EDITOR_WINDOW_NAME, editor_window); // Only set the editor window data if the action_widget is a row.

		self->editor_window_num++;
		stickynote_window_notify_should_hide (self); // Notify the window that a new editor window is opened.
	}
	
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

		gtk_list_box_update_rows (self, data, NULL);
	}

	stickynote_window_show_note_status (self);
	gtk_list_box_invalidate_sort (self->list_box);
}

static inline void
stickynote_init_alert_dialog (StickynoteWindow *self)
{
	self->alert_dialog = adw_alert_dialog_new (gettext ("Delete Note?"), 
		gettext ("This will permanently delete the note. Are you sure?"));

	g_object_ref_sink (self->alert_dialog); // Keep a reference to the dialog

  	adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (self->alert_dialog),
                                "cancel",  gettext("Cancel"),
                                "delete", gettext("Delete"),
                                NULL);

	adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (self->alert_dialog),
                                        "delete",
                                        ADW_RESPONSE_DESTRUCTIVE);

	adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (self->alert_dialog), "cancel");
  	adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (self->alert_dialog), "cancel");

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

	stickynote_init_alert_dialog (self);
}
