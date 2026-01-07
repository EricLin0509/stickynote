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

G_BEGIN_DECLS

#define STICKYNOTE_TYPE_EDITOR_WINDOW (stickynote_editor_window_get_type())

G_DECLARE_FINAL_TYPE (StickynoteEditorWindow, stickynote_editor_window, STICKYNOTE, EDITOR_WINDOW, AdwApplicationWindow)

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
stickynote_editor_window_new (const char *title, const char *color_scheme);

G_END_DECLS
