/* theme-selector.c
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

#include "theme-selector.h"

#include "color-scheme.h"

struct _ThemeSelector {
    GtkWidget parent_instance;

    GtkWidget *box;
    GtkCheckButton *red;
    GtkCheckButton *orange;
    GtkCheckButton *yellow;
    GtkCheckButton *green;
    GtkCheckButton *blue;
    GtkCheckButton *purple;
};

enum {
    COLOR_SCHEME_CHANGED,
    N_SIGNALS
};

static guint theme_selector_signals[N_SIGNALS];

G_DEFINE_TYPE (ThemeSelector, theme_selector, GTK_TYPE_WIDGET)

static void
on_checkbutton_toggled (ThemeSelector *selector, GtkToggleButton* self)
{
    const GtkCheckButton *buttons[] = {
        selector->red,
        selector->orange,
        selector->yellow,
        selector->green,
        selector->blue,
        selector->purple
    };

    int color_scheme = 0;
    for (color_scheme = 0 ;
            color_scheme < COLOR_SCHEME_COUNT && buttons[color_scheme] != self;
            color_scheme++);

    g_signal_emit (selector, theme_selector_signals[COLOR_SCHEME_CHANGED], 0, color_scheme); // Emit signal
}

static void
theme_selector_dispose (GObject *object)
{
    ThemeSelector *self = (ThemeSelector *)object;

    g_clear_pointer (&self->box, gtk_widget_unparent);

    G_OBJECT_CLASS (theme_selector_parent_class)->dispose (object);
}

static void
theme_selector_class_init (ThemeSelectorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = theme_selector_dispose;

    theme_selector_signals[COLOR_SCHEME_CHANGED] = g_signal_new ("color-scheme-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
            0,
            NULL,
            NULL,
            NULL,
            G_TYPE_NONE,
            1,
            G_TYPE_INT);

    gtk_widget_class_set_layout_manager_type(widget_class, GTK_TYPE_BIN_LAYOUT);

    gtk_widget_class_set_css_name (widget_class, "themeselector");
    gtk_widget_class_set_template_from_resource (widget_class, "/com/ericlin/stickynote/theme-selector.ui");
    gtk_widget_class_bind_template_callback (widget_class, on_checkbutton_toggled);
    gtk_widget_class_bind_template_child (widget_class, ThemeSelector, box);
    gtk_widget_class_bind_template_child (widget_class, ThemeSelector, yellow);
    gtk_widget_class_bind_template_child (widget_class, ThemeSelector, blue);
    gtk_widget_class_bind_template_child (widget_class, ThemeSelector, green);
    gtk_widget_class_bind_template_child (widget_class, ThemeSelector, red);
    gtk_widget_class_bind_template_child (widget_class, ThemeSelector, purple);
    gtk_widget_class_bind_template_child (widget_class, ThemeSelector, orange);
}

static void
theme_selector_init (ThemeSelector *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}


ThemeSelector *
theme_selector_new (void)
{
    return g_object_new (THEME_TYPE_SELECTOR, NULL);
}