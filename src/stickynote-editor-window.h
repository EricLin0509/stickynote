/* stickynote-editor-window.h
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

#pragma once

#include <adwaita.h>

#include "metadata.h"

G_BEGIN_DECLS

#define STICKYNOTE_TYPE_EDITOR_WINDOW (stickynote_editor_window_get_type())
#define STICKYNOTE_EDITOR_WINDOW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), STICKYNOTE_TYPE_EDITOR_WINDOW, StickynoteEditorWindowClass))

G_DECLARE_FINAL_TYPE (StickynoteEditorWindow, stickynote_editor_window, STICKYNOTE, EDITOR_WINDOW, AdwApplicationWindow)

/*
 * Creates a new instance of the editor window
 * @param app
 *  The application instance
 * @param data
 *  The metadata
 * @return
 *  A new instance of the editor window
*/
StickynoteEditorWindow *
stickynote_editor_window_new (GApplication *app, Metadata *data);


/*
 * Creates a new instance of the editor window with a file save signal handler
 * @param app
 *  The application instance
 * @param data
 *  The metadata
 * @param file_save_signal_handler
 *  The signal handler for the file save signal
 * @param user_data
 *  The user data for the signal handler
 * @return
 *  A new instance of the editor window
*/
StickynoteEditorWindow *
stickynote_editor_window_new_full (GApplication *app, Metadata *data, GCallback file_save_signal_handler, gpointer user_data);

/* Connects a signal to the editor window */
/*
  * @param self
  *  The editor window instance
  * @param signal_name
  *  The name of the signal to connect
  * @param handler_func
  *  The signal handler function
  * @param user_data
  *  The user data for the signal handler
*/
void
stickynote_editor_window_connect_signal (StickynoteEditorWindow *self, const char *signal_name, GCallback handler_func, gpointer user_data);

/* Disconnects all signals from the editor window */
/*
  * @warning
  * This function only takes effect if the signal is connected by `stickynote_editor_window_connect_signal`.
  * If the signal is connected by other means, it will not be disconnected.
*/
void
stickynote_editor_window_disconnect_all_signals (StickynoteEditorWindow *self);

G_END_DECLS
