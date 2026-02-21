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

#include "config.h"
#include "note-dir.h"

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
	AdwDialog *alert_dialog;
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

static inline void
perform_delete (StickynoteWindow *self, StickynoteRow *row)
{
	Metadata *data = g_hash_table_lookup (self->metadata_table, row);

	if (data == NULL) return;

	if (!metadata_delete_file (data)) return; // Failed to delete the file

	self->note_num--;

	g_hash_table_remove (self->metadata_table, row);
	gtk_list_box_remove (self->list_box, GTK_WIDGET (row));
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
	adw_alert_dialog_choose (ADW_ALERT_DIALOG (self->alert_dialog), GTK_WIDGET (self), NULL,
                        (GAsyncReadyCallback) stickynote_window_alert_choice, user_data); // Show the alert dialog
}

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
		g_signal_connect_swapped (row, "delete-request", G_CALLBACK (stickynote_window_delete_note), self);
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
on_stickynote_saved (StickynoteEditorWindow *editor_window, Metadata *data, StickynoteWindow *self)
{
	if (data == NULL) return;

	gtk_list_box_update_rows (self, data);

	stickynote_window_show_note_status (self);

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
