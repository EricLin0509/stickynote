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

struct _StickynoteWindow
{
	AdwApplicationWindow  parent_instance;

	/* Template widgets */
	GtkButton *new_button;
};

G_DEFINE_FINAL_TYPE (StickynoteWindow, stickynote_window, ADW_TYPE_APPLICATION_WINDOW)

/* GObject essential methods */

static void
stickynote_window_present_editor_window (StickynoteWindow *self)
{
	StickynoteEditorWindow *editor_window = stickynote_editor_window_new (NULL, NULL);
	gtk_window_present (GTK_WINDOW (editor_window));
}

static void
stickynote_window_class_init (StickynoteWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gtk_widget_class_set_template_from_resource (widget_class, "/com/ericlin/stickynote/stickynote-window.ui");
	gtk_widget_class_bind_template_callback (widget_class, stickynote_window_present_editor_window);
	gtk_widget_class_bind_template_child (widget_class, StickynoteWindow, new_button);
}

static void
stickynote_window_init (StickynoteWindow *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));
}
