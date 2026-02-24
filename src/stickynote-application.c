/* stickynote-application.c
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
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "stickynote-application.h"
#include "stickynote-window.h"

struct _StickynoteApplication
{
	AdwApplication parent_instance;

	StickynoteManager *manager;
};

G_DEFINE_FINAL_TYPE (StickynoteApplication, stickynote_application, ADW_TYPE_APPLICATION)

StickynoteApplication *
stickynote_application_new (const char        *application_id,
                            GApplicationFlags  flags)
{
	g_return_val_if_fail (application_id != NULL, NULL);

	return g_object_new (STICKYNOTE_TYPE_APPLICATION,
	                     "application-id", application_id,
	                     "flags", flags,
	                     "resource-base-path", "/com/ericlin/stickynote",
	                     NULL);
}

static void
stickynote_application_activate (GApplication *app)
{
	gtk_source_init();

	GtkWindow *window;

	g_assert (STICKYNOTE_IS_APPLICATION (app));

	StickynoteApplication *self = STICKYNOTE_APPLICATION (app);

	window = gtk_application_get_active_window (GTK_APPLICATION (app));

	if (window == NULL)
		window = stickynote_window_new (app, self->manager);

	gtk_window_present (window);
}

static void
stickynote_application_dispose (GObject *object)
{
	StickynoteApplication *self = STICKYNOTE_APPLICATION (object);

	g_clear_object (&self->manager);

	G_OBJECT_CLASS (stickynote_application_parent_class)->dispose (object);
}

static void
stickynote_application_class_init (StickynoteApplicationClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = stickynote_application_dispose;

	GApplicationClass *app_class = G_APPLICATION_CLASS (klass);
	app_class->activate = stickynote_application_activate;
	
}

static void
stickynote_application_about_action (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
	static const char *developers[] = {"EricLin", NULL};
	StickynoteApplication *self = user_data;
	GtkWindow *window = NULL;

	g_assert (STICKYNOTE_IS_APPLICATION (self));

	window = gtk_application_get_active_window (GTK_APPLICATION (self));

	adw_show_about_dialog (GTK_WIDGET (window),
	                       "application-name", "stickynote",
	                       "application-icon", "com.ericlin.stickynote",
	                       "developer-name", "EricLin",
	                       "translator-credits", _("translator-credits"),
	                       "version", "0.1.0",
	                       "developers", developers,
	                       "copyright", "© 2026 EricLin",
                           "license-type", GTK_LICENSE_GPL_3_0,
						   "website", "https://github.com/EricLin0509/stickynote",
						   "issue-url", "https://github.com/EricLin0509/stickynote/issues",
	                       NULL);
}

static void
stickynote_application_quit_action (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
	StickynoteApplication *self = user_data;

	g_assert (STICKYNOTE_IS_APPLICATION (self));

	g_application_quit (G_APPLICATION (self));
}

static const GActionEntry app_actions[] = {
	{ "quit", stickynote_application_quit_action },
	{ "about", stickynote_application_about_action },
};

static void
stickynote_application_init (StickynoteApplication *self)
{
	self->manager = stickynote_manager_new (G_APPLICATION (self));
	stickynote_manager_init_notes (self->manager);

	g_action_map_add_action_entries (G_ACTION_MAP (self),
	                                 app_actions,
	                                 G_N_ELEMENTS (app_actions),
	                                 self);
		gtk_application_set_accels_for_action (GTK_APPLICATION (self),
	                                       "win.change-title",
	                                       (const char *[]) { "<control>t", NULL });
	gtk_application_set_accels_for_action (GTK_APPLICATION (self),
	                                       "win.save",
	                                       (const char *[]) { "<control>s", NULL });
	gtk_application_set_accels_for_action (GTK_APPLICATION (self),
	                                       "app.quit",
	                                       (const char *[]) { "<control>q", NULL });
}
