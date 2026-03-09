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
	AdwDialog *alert_dialog;
	GWeakRef manager;
	size_t note_num;
	size_t editor_window_num; // If this number is greater than 0, it means that the app is in the process of creating a new note.
	guint note_changed_id;
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
gtk_list_box_remove_row (StickynoteWindow *self, StickynoteRow *row)
{
	g_return_if_fail (STICKYNOTE_IS_WINDOW (self) && STICKYNOTE_IS_ROW (row));

	self->note_num--;
	gtk_list_box_remove (self->list_box, GTK_WIDGET (row));
}

static void
stickynote_window_open_note (StickynoteWindow *self, gpointer user_data);

static void
stickynote_window_alert_choice (AdwAlertDialog *dialog, GAsyncResult *result, gpointer user_data)
{
	StickynoteRow *row = user_data;

	StickynoteWindow *window = STICKYNOTE_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (dialog), STICKYNOTE_TYPE_WINDOW));

	const char *choice = adw_alert_dialog_choose_finish (dialog, result);

	if (memcmp (choice, "cancel", 6) == 0) return; // User clicked 'Cancel'

	StickynoteManager *manager = g_weak_ref_get (&window->manager);
	g_return_if_fail (STICKYNOTE_IS_MANAGER (manager));

	Metadata *data = g_object_get_data (G_OBJECT (row), "metadata");
	stickynote_manager_delete_note (manager, data);
}

static void
stickynote_window_delete_note (StickynoteWindow *self, gpointer user_data)
{
	g_return_if_fail (STICKYNOTE_IS_WINDOW (self));

	adw_alert_dialog_choose (ADW_ALERT_DIALOG (self->alert_dialog), GTK_WIDGET (self), NULL,
                        (GAsyncReadyCallback) stickynote_window_alert_choice, user_data); // Show the alert dialog
}

static void
stickynote_window_export_note (StickynoteWindow *self, gpointer user_data)
{
	g_return_if_fail (STICKYNOTE_IS_WINDOW (self));

	GtkWidget *row = user_data;
	g_return_if_fail (STICKYNOTE_IS_ROW (row));

	Metadata *data = g_object_get_data (G_OBJECT (row), "metadata");

	stickynote_manager_export_note (GTK_WINDOW (self), data);
}

static void
gtk_list_box_update_rows (StickynoteWindow *self, Metadata *data)
{
	g_return_if_fail (STICKYNOTE_IS_WINDOW (self) && data != NULL);

	const char *title = NULL;
	const char *timestamp = NULL;
	int color_scheme = 0;
	metadata_get_data (data, &color_scheme, &title, &timestamp);

	if (title == NULL || *title == '\0') // If the title is empty, set it to "Untitled"
		title = gettext ("Untitled");

	int year, month, day, hour, minute, second;
	g_autofree gchar *date_str = NULL;
	if (timestamp != NULL)
	{
		timestamp_parse (timestamp, &year, &month, &day, &hour, &minute, &second);
		date_str = g_strdup_printf (TIME_STRING_FORMAT, year, month, day, hour, minute, second); // Convert the timestamp to a string
	}

	GtkWidget *row = g_object_get_data (G_OBJECT (data), "row");

	if (row == NULL) // A new note is added
	{
		self->note_num++;
		row = stickynote_row_new (title, date_str);
		g_signal_connect_swapped (row, "edit-request", G_CALLBACK (stickynote_window_open_note), self);
		g_signal_connect_swapped (row, "delete-request", G_CALLBACK (stickynote_window_delete_note), self);
		g_signal_connect_swapped (row, "export-request", G_CALLBACK (stickynote_window_export_note), self);

		/* Make connections with each other */
		g_object_set_data (G_OBJECT (row), "metadata", data);
		g_object_set_data (G_OBJECT (data), "row", row);
		gtk_list_box_prepend (self->list_box, row);
	}
	else
	{
		stickynote_row_set_title (STICKYNOTE_ROW (row), title);
		stickynote_row_set_subtitle (STICKYNOTE_ROW (row), date_str);
	}

	if (color_scheme >= 0 && color_scheme < COLOR_SCHEME_COUNT)
		stickynote_row_update_color_scheme (STICKYNOTE_ROW (row), color_scheme);
}

/* This callback MUST be called by `g_signal_connect_swapped` */
static void
stickynote_window_open_note (StickynoteWindow *self, gpointer user_data)
{
	GtkWidget *action_widget = user_data;
	StickynoteManager *manager = g_weak_ref_get (&self->manager);
	g_return_if_fail (STICKYNOTE_IS_MANAGER (manager));
	Metadata *data = NULL;

	if (STICKYNOTE_IS_ROW (action_widget)) // User clicked the row, get the metadata from the row.
	{
		data = g_object_get_data (G_OBJECT (action_widget), "metadata");
		g_return_if_fail (data != NULL); // Check if the data is valid
	}

	stickynote_manager_edit_note (manager, data);
}

static void
handle_note_manager_changed (StickynoteManager *manager, StickynoteManagerMode mode, Metadata *data, gpointer user_data)
{
	StickynoteWindow *self = STICKYNOTE_WINDOW (user_data);

	switch (mode)
	{
		case STICKYNOTE_MANAGER_MODE_LOAD:
		case STICKYNOTE_MANAGER_MODE_SAVE:
			gtk_list_box_update_rows (self, data);
			break;
		case STICKYNOTE_MANAGER_MODE_DELETE:
			StickynoteRow *row = g_object_get_data (G_OBJECT (data), "row");
			gtk_list_box_remove_row (self, row);
			break;
		default:
			break;
	}

	gtk_list_box_invalidate_sort (self->list_box); // Sort the list by timestamp.
	stickynote_window_show_note_status (self);
}

static void
stickynote_window_dispose (GObject *object)
{
	StickynoteWindow *self = STICKYNOTE_WINDOW (object);

	StickynoteManager *manager = g_weak_ref_get (&self->manager);
	if (STICKYNOTE_IS_MANAGER (manager))
		g_signal_handler_disconnect (manager, self->note_changed_id);

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

	gtk_list_box_invalidate_sort (self->list_box); // Sort the list by timestamp.

	stickynote_init_alert_dialog (self);
}

GtkWindow *
stickynote_window_new (GApplication *app, StickynoteManager *manager)
{
	g_return_val_if_fail (STICKYNOTE_IS_MANAGER (manager), NULL);

	StickynoteWindow *self = g_object_new (STICKYNOTE_TYPE_WINDOW, "application", app, NULL);

	g_weak_ref_init (&self->manager, manager);
	self->note_changed_id = g_signal_connect (manager, "note-changed", G_CALLBACK (handle_note_manager_changed), self);
	stickynote_manager_get_notes (manager);

	return GTK_WINDOW (self);
}