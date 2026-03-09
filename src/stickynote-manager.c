/* stickynote-manager.c
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

#include <sys/stat.h>

#include "stickynote-manager.h"

#include "stickynote-editor-window.h"

struct _StickynoteManager {
    AdwBin parent_instance;

    GHashTable *metadata_table;

    GApplication *app;
};

enum {
    NOTE_CHANGED,
    N_SIGNALS
};

enum {
    PROP_APP = 1,
    N_PROPS
};

static GParamSpec *obj_props[N_PROPS] = {NULL, };

static guint manager_signals[N_SIGNALS] = {0};

G_DEFINE_FINAL_TYPE(StickynoteManager, stickynote_manager, G_TYPE_OBJECT)

/* ==== Load and save logics ==== */

static inline gboolean
ensure_directories_exist (const gchar *dir_path)
{
	struct stat st;

	if (stat (dir_path, &st) == 0)
	{
		if (S_ISDIR (st.st_mode)) return TRUE; // Directory exists
		else // If the path exists but is not a directory
		{
			g_critical ("%s is not a directory", dir_path);
			return FALSE;
		}
	}

	g_warning ("Directory %s does not exist, creating...", dir_path);

	g_mkdir_with_parents (dir_path, 0755);

	return TRUE;
}

static char *
get_note_dir_realpath (gboolean *is_directory)
{
	gboolean is_valid_dir, *is_valid_dir_ptr;
	is_valid_dir_ptr = is_directory ? is_directory : &is_valid_dir; // If is_directory is NULL, we will allocate memory for it.

	GSettings *setting = g_settings_new ("com.ericlin.stickynote");
	g_autofree gchar *notes_dir = g_settings_get_string (setting, "notes-dir");
	g_autofree gchar *realpath = NULL;

	if (strstr (notes_dir, "../") != NULL) // Check if the path is not a treverse path
	{
		g_critical ("Treverse path is not allowed: %s", notes_dir);
		return NULL;
	}

	if (memcmp (notes_dir, "~/", 2) == 0)
	{
		realpath = g_build_filename (g_get_home_dir (), notes_dir + 2, NULL);
	}
	else if (memcmp (notes_dir, "/", 1) == 0)
	{
		realpath = g_build_filename (notes_dir, NULL);
	}
	else
	{
		realpath = g_build_filename (g_get_current_dir (), notes_dir, NULL);
	}

	*is_valid_dir_ptr = ensure_directories_exist (realpath);

	return g_steal_pointer (&realpath);
}

static gboolean
save_metadata_to_file (Metadata *data, const gchar *content)
{
	if (metadata_get_path (data) == NULL)
	{
		gboolean is_valid_dir;
		g_autofree gchar *notes_dir = get_note_dir_realpath (&is_valid_dir);
		if (notes_dir == NULL || !is_valid_dir) return FALSE;
		g_autofree gchar *file_name = metadata_build_file_name (data);
		g_autofree gchar *path = g_build_filename (notes_dir, file_name, NULL);
		metadata_set_path (data, path);
	}

	if (!metadata_save (data, content))
	{
		g_critical ("Failed to save file");
		return FALSE;
	}

	return TRUE;
}

/* ==== StickynoteManager ==== */

StickynoteManager *
stickynote_manager_new (GApplication *app)
{
    return g_object_new(STICKYNOTE_TYPE_MANAGER, "app", app, NULL);
}

/* This function may be use to implement a real-time update of the notes list, but it is not necessary for now. */
void
stickynote_manager_init_notes (StickynoteManager *self)
{
    g_return_if_fail(STICKYNOTE_IS_MANAGER(self));

	gboolean is_valid_dir;
	g_autofree gchar *notes_dir = get_note_dir_realpath (&is_valid_dir);
	if (notes_dir == NULL || !is_valid_dir) return;

	GDir *dir = g_dir_open (notes_dir, 0, NULL);
	if (dir == NULL) return;

	const gchar *file_name;
	while ((file_name = g_dir_read_name (dir)) != NULL)
	{
		g_autofree gchar *path = g_build_filename (notes_dir, file_name, NULL);
        if (!g_str_has_suffix (file_name, ".md")) continue; // Ignore non-markdown files

		Metadata *data = metadata_new (path);
		if (data == NULL) continue; // Ignore invalid files

        g_hash_table_insert(self->metadata_table, path, data);
        g_signal_emit(self, manager_signals[NOTE_CHANGED], 0, STICKYNOTE_MANAGER_MODE_LOAD, data);
    }
}

void
stickynote_manager_get_notes (StickynoteManager *self)
{
    g_return_if_fail(STICKYNOTE_IS_MANAGER(self));

    GHashTableIter iter;
    g_hash_table_iter_init(&iter, self->metadata_table);
    Metadata *data = NULL;

    while (g_hash_table_iter_next(&iter, NULL, (void **)&data))
    {
        if (data == NULL) continue; // Ignore invalid files

        g_signal_emit(self, manager_signals[NOTE_CHANGED], 0, STICKYNOTE_MANAGER_MODE_LOAD, data);
    }
}

static gboolean
on_stickynote_window_save_note (StickynoteEditorWindow *window, gboolean is_original, Metadata *metadata, const gchar *content, gpointer user_data)
{
    StickynoteManager *manager = STICKYNOTE_MANAGER(user_data);

    if (is_original)
    {
        const gchar *path = metadata_get_path(metadata);
        g_hash_table_replace(manager->metadata_table, (void *)path, metadata); // Update the metadata in the manager
        g_signal_emit(manager, manager_signals[NOTE_CHANGED], 0, STICKYNOTE_MANAGER_MODE_SAVE, metadata); // Emit the signal to update the notes list
        return TRUE; // Do not save the original note, it is already saved in the metadata table.
    }

    return stickynote_manager_save_note(manager, metadata, content);
}

void
stickynote_manager_edit_note (StickynoteManager *self, Metadata *metadata)
{
    g_return_if_fail(STICKYNOTE_IS_MANAGER(self));

    if (metadata == NULL) metadata = metadata_new (NULL); // Create a new metadata object if it is NULL

    StickynoteEditorWindow *window = g_object_get_data(G_OBJECT(metadata), "stickynote-editor-window");

    if (window == NULL)
    {
        window = stickynote_editor_window_new_full(self->app, metadata, G_CALLBACK(on_stickynote_window_save_note), self);
        stickynote_editor_window_connect_signal(window, "file-export", G_CALLBACK(stickynote_manager_export_note), metadata);
    }
    
    gtk_window_present(GTK_WINDOW(window));
}

gboolean
stickynote_manager_save_note (StickynoteManager *self, Metadata *metadata, const gchar *content)
{
    g_return_val_if_fail(STICKYNOTE_IS_MANAGER(self) && META_IS_DATA (metadata), FALSE);

    if (!save_metadata_to_file(metadata, content)) return FALSE;

    const gchar *path = metadata_get_path(metadata);
    g_hash_table_replace(self->metadata_table, (void *)path, metadata); // Update the metadata in the manager

    g_signal_emit(self, manager_signals[NOTE_CHANGED], 0, STICKYNOTE_MANAGER_MODE_SAVE, metadata);

    return TRUE;
}

gboolean
stickynote_manager_delete_note (StickynoteManager *self, Metadata *metadata)
{
    g_return_val_if_fail(STICKYNOTE_IS_MANAGER(self) && META_IS_DATA (metadata), FALSE);

    GtkWindow *window = g_object_get_data(G_OBJECT(metadata), "stickynote-editor-window");
    if (GTK_IS_WINDOW(window))
        gtk_window_destroy(window); // Destroy the editor window if it is open

    if (!metadata_delete_file(metadata)) return FALSE;

    g_signal_emit(self, manager_signals[NOTE_CHANGED], 0, STICKYNOTE_MANAGER_MODE_DELETE, metadata);

    g_hash_table_remove(self->metadata_table, metadata_get_path(metadata));

    return TRUE;
}

static void
on_dialog_response (GObject* source_object, GAsyncResult *result, gpointer user_data)
{
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);
    Metadata *metadata = user_data;
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish (dialog, result, &error);

    if (!G_IS_FILE (file))
    {
        if (error->code == GTK_DIALOG_ERROR_DISMISSED)
            g_warning ("User cancelled the file save dialog");
        else
            g_critical ("Failed to save file: %s", error->message);

        g_clear_error (&error);
        return;
    }
    g_clear_error (&error);

    g_autofree gchar *path = g_file_get_path (file);
    g_object_unref (file);

    if (path == NULL) return;

    metadata_export (metadata, path);
}

void
stickynote_manager_export_note (GtkWindow *window, Metadata *metadata)
{
    g_return_if_fail(GTK_IS_WINDOW(window) && META_IS_DATA (metadata));

    GtkFileDialog *dialog = gtk_file_dialog_new ();

    gtk_file_dialog_save (dialog, window, NULL, on_dialog_response, metadata);
}

/* ==== GObject ==== */

static void
stickynote_manager_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    StickynoteManager *self = STICKYNOTE_MANAGER(object);

    switch (prop_id)
    {
        case PROP_APP:
            self->app = g_value_get_object(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
stickynote_manager_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    StickynoteManager *self = STICKYNOTE_MANAGER(object);

    switch (prop_id)
    {
        case PROP_APP:
            g_value_set_object(value, self->app);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
stickynote_manager_dispose(GObject *object)
{
    StickynoteManager *self = STICKYNOTE_MANAGER(object);

    g_hash_table_remove_all(self->metadata_table);
    g_clear_pointer(&self->metadata_table, g_hash_table_unref);

    G_OBJECT_CLASS(stickynote_manager_parent_class)->dispose(object);
}

static void
stickynote_manager_class_init(StickynoteManagerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = stickynote_manager_dispose;

    object_class->set_property = stickynote_manager_set_property;
    object_class->get_property = stickynote_manager_get_property;
    obj_props[PROP_APP] = g_param_spec_object("app", NULL, NULL, G_TYPE_APPLICATION, G_PARAM_READWRITE);
    g_object_class_install_properties(object_class, N_PROPS, obj_props);

    manager_signals[NOTE_CHANGED] = g_signal_new("note-changed",
                                                 G_TYPE_FROM_CLASS(klass),
                                                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                                                 0,
                                                 NULL,
                                                 NULL,
                                                 NULL,
                                                 G_TYPE_NONE,
                                                 2,
                                                 G_TYPE_INT, // StickynoteManagerMode
                                                 G_TYPE_OBJECT); // The metadata object
}

static void
stickynote_manager_init(StickynoteManager *self)
{
    self->metadata_table = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_object_unref);
}

