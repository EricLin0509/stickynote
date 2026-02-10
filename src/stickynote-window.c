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

#include "config.h"

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
	GtkListBox *list_box;

	/* Private */
	GApplication *app;
	GHashTable *metadata_table;
};

G_DEFINE_FINAL_TYPE (StickynoteWindow, stickynote_window, ADW_TYPE_APPLICATION_WINDOW)

/* GObject essential methods */

static int
gtk_listbox_sort_func (GtkListBoxRow *row1, GtkListBoxRow *row2, gpointer user_data)
{
	const char *time_str1 = adw_action_row_get_subtitle (ADW_ACTION_ROW (row1));
	const char *time_str2 = adw_action_row_get_subtitle (ADW_ACTION_ROW (row2));

	return memcmp (time_str1, time_str2, strlen (TIME_STRING_FORMAT)); // Because the TIME_STRING_FORMAT is fixed, we can use memcmp to compare the timestamps.
}

static void
on_stickynote_saved (StickynoteEditorWindow *editor_window, Metadata *data, StickynoteWindow *self)
{
	if (data == NULL) return;

	const char *title = NULL;
	const char *timestamp = NULL;
	metadata_get_data (data, NULL, &title, &timestamp);

	int year, month, day, hour, minute, second;
	timestamp_parse (timestamp, &year, &month, &day, &hour, &minute, &second);

	g_autofree gchar *date_str = g_strdup_printf (TIME_STRING_FORMAT, year, month, day, hour, minute, second); // Convert the timestamp to a string

	GtkWidget *row = metadata_get_user_data (data);

	if (row == NULL)
	{
		row = adw_action_row_new ();
		adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
		adw_action_row_set_subtitle (ADW_ACTION_ROW (row), date_str);
		g_hash_table_insert (self->metadata_table, row, data);
		metadata_add_user_data (data, row);
		gtk_list_box_prepend (self->list_box, row);
	}
	else
	{
		adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
		adw_action_row_set_subtitle (ADW_ACTION_ROW (row), date_str);
	}

	gtk_list_box_invalidate_sort (self->list_box); // Sort the list again to reflect the new row.
}

static void
stickynote_window_create_new_note (StickynoteWindow *self)
{
	Metadata *data = metadata_new(NULL, NULL);
	StickynoteEditorWindow *editor_window = stickynote_editor_window_new (self->app, data);
	g_signal_connect (editor_window, "file-saved", G_CALLBACK (on_stickynote_saved), self);
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
	gtk_widget_class_bind_template_callback (widget_class, stickynote_window_create_new_note);
	gtk_widget_class_bind_template_child (widget_class, StickynoteWindow, toolbar);
	gtk_widget_class_bind_template_child (widget_class, StickynoteWindow, new_button);
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
}
