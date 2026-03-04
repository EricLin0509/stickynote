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
	AdwToastOverlay *toast_overlay;
	AdwNavigationView *navigation_view;
	AdwNavigationPage *editor_page;
	GtkTextView *text_view;
	GtkTextBuffer *text_buffer;
	GtkLabel *char_count_label;
	GtkEmojiChooser *emoji_chooser;
	GtkMenuButton *editor_menu;

	AdwNavigationPage *set_title_page;
	AdwEntryRow *title_entry;
	GtkButton *save_button;

	/* Private */
	int last_color_scheme_index;
	gboolean has_unsaved_changes;
	GHashTable *signal_ids;
	GWeakRef metadata; // Weak reference to the metadata object
};

G_DEFINE_FINAL_TYPE (StickynoteEditorWindow, stickynote_editor_window, ADW_TYPE_APPLICATION_WINDOW)

enum {
    FILE_SAVED,
    N_SIGNALS
};

static guint stickynote_editor_window_signals[N_SIGNALS];

enum {
	PROP_METADATA = 1,
	N_PROPS
};

static GParamSpec *stickynote_editor_window_props[N_PROPS] = { NULL, };

static void
send_simple_toast_message (StickynoteEditorWindow *self, const char *message)
{
	g_return_if_fail (STICKYNOTE_IS_EDITOR_WINDOW (self));

	AdwToast *toast = adw_toast_new (message);
	adw_toast_set_timeout (toast, 5); // 5 seconds timeout

	adw_toast_overlay_add_toast (self->toast_overlay, toast);
}

static void
emit_file_saved_signal (StickynoteEditorWindow *self)
{
	g_return_if_fail (STICKYNOTE_IS_EDITOR_WINDOW (self));

	Metadata *metadata = g_weak_ref_get (&self->metadata);
	g_return_if_fail (META_IS_DATA (metadata));

  	GtkTextIter start;
  	GtkTextIter end;
	gtk_text_buffer_get_bounds (self->text_buffer, &start, &end);
	const gchar *content = gtk_text_buffer_get_text (self->text_buffer, &start, &end, FALSE);

	metadata_update (metadata, self->last_color_scheme_index, NULL, TRUE); // Also uptate the timestamp

	gboolean save_result = FALSE;
	g_signal_emit (self, stickynote_editor_window_signals[FILE_SAVED], 0, metadata, content, &save_result);

	if (!save_result) // If the signal handler returns FALSE, it means save failed
	{
		send_simple_toast_message (self, gettext ("Failed to save the note"));
		return;
	}

	self->has_unsaved_changes = FALSE; // Mark the file as saved
	gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.save-copy", TRUE);
	send_simple_toast_message (self, gettext ("Note saved successfully"));
}

static void
on_emoji_picked_cb (StickynoteEditorWindow *self, const gchar *emoji, GtkEmojiChooser *chooser)
{
	gtk_text_buffer_insert_at_cursor (self->text_buffer, emoji, -1);
}

static void
on_changed_cb (StickynoteEditorWindow *self, GtkTextBuffer *buffer)
{
	self->has_unsaved_changes = TRUE;

	const gint total_chars = gtk_text_buffer_get_char_count (buffer);

	g_autofree gchar *char_count_label_text = g_strdup_printf ("Characters %d", total_chars);

	gtk_label_set_text (self->char_count_label, char_count_label_text);
}

static gboolean
on_stickynote_window_close (StickynoteEditorWindow *window, gpointer user_data)
{
    Metadata *metadata = g_weak_ref_get (&window->metadata);

    if (!META_IS_DATA (metadata)) return FALSE;

    g_object_set_data(G_OBJECT(metadata), "stickynote-editor-window", NULL); // Remove the window from the manager

    if (metadata_get_path(metadata) == NULL) // If the note is not saved, clear the metadata
        g_clear_object (&metadata);

    return FALSE;
}

static void
check_title_valid (StickynoteEditorWindow *self, GtkEditable* editable)
{
	const char *title = gtk_editable_get_text (editable);

	if (strlen (title) == 0)
	{
		gtk_widget_set_sensitive (GTK_WIDGET (self->save_button), FALSE);
		return;
	}

	if (memcmp (title, " ", 1) == 0) // If the title starts with spaces, it's not a valid title
	{
		gtk_widget_set_sensitive (GTK_WIDGET (self->save_button), FALSE);
		adw_preferences_row_set_title (ADW_PREFERENCES_ROW (editable), gettext("Title cannot start with spaces"));
		return;
	}

	if (strstr (title, "/") != NULL) // If the title contains a slash, it's not a valid title
	{
		gtk_widget_set_sensitive (GTK_WIDGET (self->save_button), FALSE);
		adw_preferences_row_set_title (ADW_PREFERENCES_ROW (editable), gettext("Title cannot contain '/'"));
		return;
	}

	gtk_widget_set_sensitive (GTK_WIDGET (self->save_button), TRUE);
	adw_preferences_row_set_title (ADW_PREFERENCES_ROW (editable), gettext("Enter the title here"));
}

static void
on_title_apply (StickynoteEditorWindow *self, GtkButton *button)
{
	const char *title = gtk_editable_get_text (GTK_EDITABLE (self->title_entry));

	Metadata *metadata = g_weak_ref_get (&self->metadata);
	g_return_if_fail (META_IS_DATA (metadata));

	metadata_update (metadata, -1, title, FALSE);

	adw_navigation_view_pop (self->navigation_view);

	adw_navigation_page_set_title (self->editor_page, title);

	emit_file_saved_signal (self);
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
	gtk_widget_add_css_class (GTK_WIDGET (editor_window), stickynote_color_scheme[color_scheme_index]); // Then add the selected color scheme class

	editor_window->last_color_scheme_index = color_scheme_index; // Update the last color scheme index
	editor_window->has_unsaved_changes = TRUE; // Mark the file as unsaved
}

static void
file_saved_action (GtkWidget  *widget,
								const char *action_name,
                                GVariant      *parameter)
{
	StickynoteEditorWindow *self = STICKYNOTE_EDITOR_WINDOW (widget);

	Metadata *metadata = g_weak_ref_get (&self->metadata);
	g_return_if_fail (META_IS_DATA (metadata));

	const gchar *title = NULL;
	metadata_get_data (metadata, NULL, &title, NULL);

	if (title == NULL || *title == '\0')
	{
		if (adw_navigation_view_get_visible_page (self->navigation_view) == self->editor_page) // Only push the set title page if the current page is the editor page
			adw_navigation_view_push (self->navigation_view, self->set_title_page); // If the title is empty, push the set title page
		return;
	}

	if (!self->has_unsaved_changes) return; // If there's no changes, return

	emit_file_saved_signal (self);
}

static void
file_saved_copy_action (GtkWidget  *widget,
								const char *action_name,
                                GVariant      *parameter)
{
	StickynoteEditorWindow *self = STICKYNOTE_EDITOR_WINDOW (widget);

	Metadata *metadata_old = g_weak_ref_get (&self->metadata);
	g_return_if_fail (META_IS_DATA (metadata_old));

	const char *title = NULL;

	g_object_set_data(G_OBJECT(metadata_old), "stickynote-editor-window", NULL); // Remove the window from the manager

	Metadata *metadata_new = metadata_copy (metadata_old);
	g_object_set_data (G_OBJECT(metadata_new), "stickynote-editor-window", self); // Add the window to the new metadata object
	metadata_get_data (metadata_new, NULL, &title, NULL); // Get the new title
	g_weak_ref_set (&self->metadata, metadata_new); // Update the weak reference to the new metadata object
	adw_navigation_page_set_title (self->editor_page, title); // Update the title of the editor page

	emit_file_saved_signal (self);
}

static void
stickynote_editor_window_set_metadata (StickynoteEditorWindow *self, Metadata *metadata)
{
	g_return_if_fail (STICKYNOTE_IS_EDITOR_WINDOW (self) && META_IS_DATA (metadata));

	g_object_set_data (G_OBJECT (metadata), "stickynote-editor-window", self); // Add the window to the metadata object
	g_weak_ref_set (&self->metadata, metadata);

	if (metadata_get_path (metadata) == NULL)
		gtk_widget_action_set_enabled (GTK_WIDGET (self), "win.save-copy", FALSE);

	const char *title = NULL;
	int color_scheme = -1;

	metadata_get_data (metadata, &color_scheme, &title, NULL);

	adw_navigation_page_set_title (self->editor_page, (title && *title) ? title : gettext("Untitled"));

	if (color_scheme == -1) // If no color scheme define, use random color
	{
		int random_number = g_random_int_range (0, COLOR_SCHEME_COUNT * 6); // Multiply by 6 to get a better ramdom distribution
		color_scheme = random_number % COLOR_SCHEME_COUNT;
	}

	if (metadata_get_content_offset (metadata) > 0)
		metadata_load_direct (metadata, self->text_buffer);

	gtk_widget_add_css_class (GTK_WIDGET (self), stickynote_color_scheme[color_scheme]);

	self->last_color_scheme_index = color_scheme;
	self->has_unsaved_changes = FALSE;

	g_object_notify_by_pspec (G_OBJECT (self), stickynote_editor_window_props[PROP_METADATA]); // Notify the change of the metadata property
}

/* GObject essential methods */

static void
stickynote_editor_window_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	StickynoteEditorWindow *self = STICKYNOTE_EDITOR_WINDOW (object);

	switch (prop_id)
	{
		case PROP_METADATA:
			stickynote_editor_window_set_metadata (self, g_value_get_object (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
stickynote_editor_window_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	StickynoteEditorWindow *self = STICKYNOTE_EDITOR_WINDOW (object);

	switch (prop_id)
	{
		case PROP_METADATA:
			g_value_set_object (value, g_weak_ref_get (&self->metadata));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
stickynote_editor_window_dispose (GObject *object)
{
	StickynoteEditorWindow *self = STICKYNOTE_EDITOR_WINDOW (object);

	g_weak_ref_clear (&self->metadata);

	GtkWidget *navigation_view = GTK_WIDGET (self->toast_overlay);

	g_clear_pointer (&navigation_view, gtk_widget_unparent);

	G_OBJECT_CLASS (stickynote_editor_window_parent_class)->dispose (object);
}

static void
stickynote_editor_window_class_init (StickynoteEditorWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = stickynote_editor_window_dispose;

	object_class->set_property = stickynote_editor_window_set_property;
	object_class->get_property = stickynote_editor_window_get_property;

	stickynote_editor_window_props[PROP_METADATA] =  g_param_spec_object ("metadata",
																									"Metadata",
																									"The metadata object",
																									TYPE_METADATA,
																									G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, N_PROPS, stickynote_editor_window_props);

	stickynote_editor_window_signals[FILE_SAVED] = g_signal_new ("file-save",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_BOOLEAN, // To check whether the file is saved successfully
            2,
            G_TYPE_POINTER, // The metadata pointer
			G_TYPE_POINTER); // The content pointer

	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gtk_widget_class_set_template_from_resource (widget_class, "/com/ericlin/stickynote/stickynote-editor-window.ui");
	gtk_widget_class_bind_template_callback (widget_class, on_emoji_picked_cb);
	gtk_widget_class_bind_template_callback (widget_class, on_changed_cb);
	gtk_widget_class_bind_template_callback (widget_class, on_title_apply);
	gtk_widget_class_bind_template_callback (widget_class, check_title_valid);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, toast_overlay);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, navigation_view);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, editor_page);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, text_view);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, text_buffer);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, char_count_label);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, emoji_chooser);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, editor_menu);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, set_title_page);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, title_entry);
	gtk_widget_class_bind_template_child (widget_class, StickynoteEditorWindow, save_button);

	gtk_widget_class_install_action (widget_class, "win.save", NULL, file_saved_action);
	gtk_widget_class_install_action (widget_class, "win.save-copy", NULL, file_saved_copy_action);
}

static void
stickynote_editor_window_init (StickynoteEditorWindow *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));

	g_weak_ref_init (&self->metadata, NULL);

	self->signal_ids = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

	GtkPopover *popover = gtk_menu_button_get_popover (self->editor_menu);

	ThemeSelector *theme_selector = theme_selector_new ();

  	gtk_popover_menu_add_child (GTK_POPOVER_MENU (popover),
                              GTK_WIDGET (theme_selector),
                              "theme");

	g_signal_connect (theme_selector, "color-scheme-changed", G_CALLBACK (on_color_scheme_changed_cb), self);

	stickynote_editor_window_connect_signal (self, "close-request", G_CALLBACK (on_stickynote_window_close), NULL);
}

StickynoteEditorWindow *
stickynote_editor_window_new (GApplication *app, Metadata *data)
{
	g_return_val_if_fail (META_IS_DATA (data), NULL);

	StickynoteEditorWindow *self = g_object_new (STICKYNOTE_TYPE_EDITOR_WINDOW, "application", app, "metadata", data, NULL);

	return self;
}

StickynoteEditorWindow *
stickynote_editor_window_new_full (GApplication *app, Metadata *data, GCallback file_save_signal_handler, gpointer user_data)
{
	StickynoteEditorWindow *self = stickynote_editor_window_new (app, data);

	stickynote_editor_window_connect_signal (self, "file-save", file_save_signal_handler, user_data);

	return self;
}

void
stickynote_editor_window_connect_signal (StickynoteEditorWindow *self, const char *signal_name, GCallback handler_func, gpointer user_data)
{
	g_return_if_fail (STICKYNOTE_IS_EDITOR_WINDOW (self));

	guint *signal_id = g_hash_table_lookup (self->signal_ids, signal_name);

	if (signal_id != NULL)
	{
		g_warning ("Signal %s already connected", signal_name);
		return;
	}

	signal_id = g_new0 (guint, 1);

	*signal_id = g_signal_connect (self, signal_name, handler_func, user_data);

	g_hash_table_insert (self->signal_ids, (void *)signal_name, signal_id);
}

static inline void
signal_disconnect_ghfunc (gpointer key, gpointer value, gpointer user_data)
{
	g_signal_handler_disconnect (user_data, *(guint *)value);
}

void
stickynote_editor_window_disconnect_all_signals (StickynoteEditorWindow *self)
{
	g_return_if_fail (STICKYNOTE_IS_EDITOR_WINDOW (self));

	if (self->signal_ids == NULL) return;

	g_hash_table_foreach (self->signal_ids, (GHFunc)signal_disconnect_ghfunc, self);

	g_hash_table_remove_all (self->signal_ids);
	self->signal_ids = NULL;
}