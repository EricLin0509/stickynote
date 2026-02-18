/* stickynote-row.c
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

#include "stickynote-row.h"

#define COLOR_SCHEME_IMPLEMENTATION
#include "color-scheme.h"

struct _StickynoteRow {
    GtkWidget parent_instance;

    AdwActionRow *action_row;
    AdwSplitButton *edit_button;

    gint last_color_scheme;
};

enum {
    EDIT_REQUEST,
    N_SIGNALS
};

static guint stickynote_row_signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE(StickynoteRow, stickynote_row, GTK_TYPE_LIST_BOX_ROW)

const char *
stickynote_row_get_title (StickynoteRow *row)
{
    g_return_val_if_fail (STICKYNOTE_IS_ROW (row), NULL);

    return adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row->action_row));
}

const char *
stickynote_row_get_subtitle (StickynoteRow *row)
{
    g_return_val_if_fail (STICKYNOTE_IS_ROW (row), NULL);

    return adw_action_row_get_subtitle (row->action_row);
}

void
stickynote_row_set_title (StickynoteRow *row, const char *title)
{
    g_return_if_fail (STICKYNOTE_IS_ROW (row));

    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row->action_row), title);
}

void
stickynote_row_set_subtitle (StickynoteRow *row, const char *subtitle)
{
    g_return_if_fail (STICKYNOTE_IS_ROW (row));

    adw_action_row_set_subtitle (row->action_row, subtitle);
}

void
stickynote_row_update_color_scheme (StickynoteRow *row, int color_scheme)
{
    g_return_if_fail (STICKYNOTE_IS_ROW (row) && color_scheme >= 0 && color_scheme < COLOR_SCHEME_COUNT);

    if (color_scheme == row->last_color_scheme) return;

    if (row->last_color_scheme >= 0 && row->last_color_scheme < COLOR_SCHEME_COUNT)
        gtk_widget_remove_css_class (GTK_WIDGET (row), stickynote_color_scheme[row->last_color_scheme]);
    
    gtk_widget_add_css_class (GTK_WIDGET (row), stickynote_color_scheme[color_scheme]);

    row->last_color_scheme = color_scheme;
}

static void
stickynote_row_dispose (GObject *object)
{
    StickynoteRow *self = STICKYNOTE_ROW (object);

    GtkWidget *row = GTK_WIDGET (self->action_row);

    g_clear_pointer (&row, gtk_widget_unparent);

    G_OBJECT_CLASS (stickynote_row_parent_class)->dispose (object);
}

static void
stickynote_row_class_init(StickynoteRowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = stickynote_row_dispose;

    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gtk_widget_class_set_template_from_resource (widget_class, "/com/ericlin/stickynote/stickynote-row.ui");
    gtk_widget_class_bind_template_child (widget_class, StickynoteRow, action_row);
    gtk_widget_class_bind_template_child (widget_class, StickynoteRow, edit_button);

    stickynote_row_signals[EDIT_REQUEST] = g_signal_new ("edit-request",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_NONE,
            0);
}

static void
stickynote_row_on_edit_clicked (AdwSplitButton *button, StickynoteRow *row)
{
    g_signal_emit (row, stickynote_row_signals[EDIT_REQUEST], 0);
}

static void
stickynote_row_init(StickynoteRow *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    self->last_color_scheme = -1; // initialize to invalid color scheme

    g_signal_connect (self->edit_button, "clicked", G_CALLBACK (stickynote_row_on_edit_clicked), self);
}

GtkWidget *
stickynote_row_new (const char *title, const char *subtitle)
{
    StickynoteRow *self = g_object_new (STICKYNOTE_TYPE_ROW, NULL);

    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->action_row), title);
    adw_action_row_set_subtitle (self->action_row, subtitle);
    return GTK_WIDGET (self);
}