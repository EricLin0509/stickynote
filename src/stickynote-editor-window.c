/* stickynote-editor-window.c
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

#define COLOR_SCHEME_IMPLEMENTATION /* For One file header */
#include "color-scheme.h"

#include "stickynote-editor-window.h"
#include "theme-selector.h"

struct _StickynoteEditorWindow
{
	AdwApplicationWindow  parent_instance;

	/* Template widgets */
	AdwNavigationView *navigation_view;
	AdwNavigationPage *editor_page;
	GtkTextView *text_view;
	GtkTextBuffer *text_buffer;
	GtkLabel *char_count_label;
	GtkEmojiChooser *emoji_chooser;
	GtkMenuButton *editor_menu;

	AdwNavigationPage *change_title_page;

	/* Private */
	int last_color_scheme_index;
	Metadata *metadata;
};

G_DEFINE_FINAL_TYPE (StickynoteEditorWindow, stickynote_editor_window, ADW_TYPE_APPLICATION_WINDOW)

enum {
    FILE_SAVED,
    N_SIGNALS
};

static guint stickynote_editor_window_signals[N_SIGNALS];

/* GObject essential methods */

static void
on_emoji_picked_cb (StickynoteEditorWindow *self, const gchar *emoji, GtkEmojiChooser *chooser)
{
	gtk_text_buffer_insert_at_cursor (self->text_buffer, emoji, -1);
}

static void
on_changed_cb (StickynoteEditorWindow *self, GtkTextBuffer *buffer)
{
	const gint total_chars = gtk_text_buffer_get_char_count (buffer);

	g_autofree gchar *char_count_label_text = g_strdup_printf ("Characters %d", total_chars);

	gtk_label_set_text (self->char_count_label, char_count_label_text);
}

static void
on_color_scheme_changed_cb (ThemeSelector *self, int color_scheme_index, StickynoteEditorWindow *editor_window)
{
	if (color_scheme_index > COLOR_SCHEME_COUNT || color_scheme_index < 0)
	{
		g_critical ("Invalid color scheme index: %d", color_scheme_index);
		return;
	}

	gtk_widget_remove_css_class (GTK_WIDGET (editor_window), stickynote_color_scheme[editor_window->last_color_scheme_index]); // Remove the last color scheme class
	editor_window->last_color_scheme_index = color_scheme_index; // Update the last color scheme index

	gtk_widget_add_css_class (GTK_WIDGET (editor_window), stickynote_color_scheme[color_scheme_index]); // Then add the selected color scheme class
}

static void
emit_file_saved_signal (StickynoteEditorWindow *self)
{
	g_return_if_fail (STICKYNOTE_IS_EDITOR_WINDOW (self));

  	GtkTextIter start;
  	GtkTextIter end;
	gtk_text_buffer_get_bounds (self->text_buffer, &start, &end);
	gchar *content = gtk_text_buffer_get_text (self->text_buffer, &start, &end, FALSE);

	metadata_update (self->metadata, self->last_color_scheme_index, NULL, TRUE); // Also uptate the timestamp
	g_signal_emit (self, stickynote_editor_window_signals[FILE_SAVED], 0, self->metadata, content);
}

static void
on_title_apply (StickynoteEditorWindow *self, AdwEntryRow *entry_row)
{
	if (adw_entry_row_get_text_length (entry_row) == 0) return; // If the title is empty, do nothing

	const char *title = gtk_editable_get_text (GTK_EDITABLE (entry_row));

	metadata_update (self->metadata, -1, title, FALSE);

	adw_navigation_view_pop (self->navigation_view);

	adw_navigation_page_set_title (self->editor_page, title);

	emit_file_saved_signal (self);
}

static void
change_title_action (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
	StickynoteEditorWindow *self = user_data;

	adw_navigation_view_push (self->navigation_view, self->change_title_page);
}

static void
file_saved_action (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
	StickynoteEditorWindow *self = user_data;

	const gchar *title = NULL;
	metadata_get_data (self->metadata, NULL, &title, NULL);

	if (title == NULL)
	{
		change_title_action (NULL, NULL, self);
		return;
	}

	emit_file_saved_signal (self);
}

static const GActionEntry window_actions[] = {
	{ "save", file_saved_action },
	{ "change-title", change_title_action }
};

static void
stickynote_editor_window_dispose (GObject *object)
{
	StickynoteEditorWindow *self = STICKYNOTE_EDITOR_WINDOW (object);

	GtkWidget *navigation_view = GTK_WIDGET (self->navigation_view);

	g_clear_pointer (&navigation_view, gtk_widget_unparent);

	if (metadata_get_path (self->metadata) == NULL)
		metadata_clear (&self->metadata); // Only clear the metadata if the file isn't exist

	G_OBJECT_CLASS (stickynote_editor_window_parent_class)->dispose (object);
}

static void
stickynote_editor_window_class_init (StickynoteEditorWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = stickynote_editor_window_dispose;

	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	stickynote_editor_window_signals[FILE_SAVED] = g_signal_new ("file-save",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_NONE,
            2,
            G_TYPE_POINTER, // The metadata pointer
			G_TYPE_POINTER); // The content pointer

	gtk_widget_class_set_template_from_resource (widget_class, "/com/ericlin/stickynote/stickynote-editor-window.ui");
	gtk_widget_class_bind_template_callback (widget_class, on_emoji_picked_cb);
	gtk_widget_class_bind_template_callback (widget_class, on_changed_cb);
	gtk_widget_class_bind_template_callback (widget_class, on_title_apply);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, navigation_view);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, editor_page);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, text_view);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, text_buffer);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, char_count_label);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, emoji_chooser);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, editor_menu);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, change_title_page);
}

static void
stickynote_editor_window_init (StickynoteEditorWindow *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));

	GtkPopover *popover = gtk_menu_button_get_popover (self->editor_menu);

	ThemeSelector *theme_selector = theme_selector_new ();

  	gtk_popover_menu_add_child (GTK_POPOVER_MENU (popover),
                              GTK_WIDGET (theme_selector),
                              "theme");

	g_signal_connect (theme_selector, "color-scheme-changed", G_CALLBACK (on_color_scheme_changed_cb), self);

    g_action_map_add_action_entries (G_ACTION_MAP (self),
	                                 window_actions,
	                                 G_N_ELEMENTS (window_actions),
	                                 self);
}

StickynoteEditorWindow *
stickynote_editor_window_new (GApplication *app, Metadata *data)
{
	g_return_val_if_fail (data != NULL && data != NULL, NULL);

	StickynoteEditorWindow *self = g_object_new (STICKYNOTE_TYPE_EDITOR_WINDOW, "application", app, NULL);
	self->metadata = data;

	const char *title = NULL;
	int color_scheme = -1;

	metadata_get_data (data, &color_scheme, &title, NULL);

	adw_navigation_page_set_title (self->editor_page, title ? title : gettext("Untitled"));

	if (color_scheme == -1) // If no color scheme define, use random color
	{
		int random_number = g_random_int_range (0, COLOR_SCHEME_COUNT * 6); // Multiply by 6 to get a better ramdom distribution
		color_scheme = random_number % COLOR_SCHEME_COUNT;
		metadata_update(data, color_scheme, NULL, FALSE);
	}

	if (metadata_get_content_offset (data) > 0)
	{
		metadata_load_direct (data, self->text_buffer);
	}

	gtk_widget_add_css_class (GTK_WIDGET (self), stickynote_color_scheme[color_scheme]);

	self->last_color_scheme_index = color_scheme;

	return self;
}

StickynoteEditorWindow *
stickynote_editor_window_new_full (GApplication *app, Metadata *data, GCallback file_save_signal_handler, gpointer user_data)
{
	StickynoteEditorWindow *self = stickynote_editor_window_new (app, data);

	g_signal_connect (self, "file-save", file_save_signal_handler, user_data);

	return self;
}