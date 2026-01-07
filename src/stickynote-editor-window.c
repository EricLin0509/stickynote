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
	GtkTextView *text_view;
	GtkTextBuffer *text_buffer;
	GtkLabel *char_count_label;
	GtkEmojiChooser *emoji_chooser;

	GtkMenuButton *editor_menu;

	int last_color_scheme_index;
};

G_DEFINE_FINAL_TYPE (StickynoteEditorWindow, stickynote_editor_window, ADW_TYPE_APPLICATION_WINDOW)

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
	if (color_scheme_index > COLOR_SCHEME_COUNT && color_scheme_index < 0)
	{
		g_critical ("Invalid color scheme index: %d", color_scheme_index);
		return;
	}

	gtk_widget_remove_css_class (GTK_WIDGET (editor_window), stickynote_color_scheme[editor_window->last_color_scheme_index]); // Remove the last color scheme class
	editor_window->last_color_scheme_index = color_scheme_index; // Update the last color scheme index

	gtk_widget_add_css_class (GTK_WIDGET (editor_window), stickynote_color_scheme[color_scheme_index]); // Then add the selected color scheme class
}

static void
stickynote_editor_window_class_init (StickynoteEditorWindowClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gtk_widget_class_set_template_from_resource (widget_class, "/com/ericlin/stickynote/stickynote-editor-window.ui");
	gtk_widget_class_bind_template_callback (widget_class, on_emoji_picked_cb);
	gtk_widget_class_bind_template_callback (widget_class, on_changed_cb);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, text_view);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, text_buffer);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, char_count_label);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, emoji_chooser);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, editor_menu);

	g_type_ensure (THEME_TYPE_SELECTOR);
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
}

/*
 * Creates a new instance of the editor window
 *@param title
 *  The title of the window
 *@param color_scheme
 *  The color scheme to use for the window, or NULL to use random colors
 * @return
 *  A new instance of the editor window
*/
StickynoteEditorWindow *
stickynote_editor_window_new (const char *title, const char *color_scheme)
{
	GApplication *app = g_application_get_default (); // Because this must be a subwindow, we can get the default application instance

	StickynoteEditorWindow *self = g_object_new (STICKYNOTE_TYPE_EDITOR_WINDOW, "application", app, NULL);

	gtk_window_set_title (GTK_WINDOW (self), title ? title : gettext("Untitled"));

	int random_number = g_random_int_range (0, COLOR_SCHEME_COUNT * 6); // Multiply by 6 to get a better ramdom distribution
	int index = random_number % COLOR_SCHEME_COUNT;

	color_scheme = color_scheme ? color_scheme : stickynote_color_scheme[index]; // Choose a random color scheme if none is specified

	gtk_widget_add_css_class (GTK_WIDGET (self), color_scheme);

	self->last_color_scheme_index = index;

	return self;
}