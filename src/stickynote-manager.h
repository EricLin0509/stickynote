/* stickynote-manager.h
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

#include <adwaita.h>

#include "metadata.h"

#ifndef STICKYNOTE_MANAGER_H
#define STICKYNOTE_MANAGER_H

G_BEGIN_DECLS

typedef enum {
    STICKYNOTE_MANAGER_MODE_LOAD,
    STICKYNOTE_MANAGER_MODE_SAVE,
    STICKYNOTE_MANAGER_MODE_DELETE
} StickynoteManagerMode; // for "note-changed" callbacks

#define STICKYNOTE_TYPE_MANAGER (stickynote_manager_get_type ())

G_DECLARE_FINAL_TYPE (StickynoteManager, stickynote_manager, STICKYNOTE, MANAGER, GObject)

StickynoteManager *
stickynote_manager_new (GApplication *app);

/* initialize the notes from the database */
/*
  * This function is used to initialize the notes from the database and update the UI
  * with the notes.
*/
void
stickynote_manager_init_notes (StickynoteManager *self);

/* get all notes from the database */
/*
  * Simliar to `stickynote_manager_init_notes`, but this function is used to
  * get all notes from the database and update the UI.
*/
void
stickynote_manager_get_notes (StickynoteManager *self);

void
stickynote_manager_edit_note (StickynoteManager *self, Metadata *metadata);

void
stickynote_manager_save_note (StickynoteManager *self, Metadata *metadata, const char *content);

void
stickynote_manager_delete_note (StickynoteManager *self, Metadata *metadata);

G_END_DECLS

#endif /* STICKYNOTE_MANAGER_H */