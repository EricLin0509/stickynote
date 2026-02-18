/* stickynote-row.h
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

#ifndef STICKYNOTE_ROW_H
#define STICKYNOTE_ROW_H

#include <adwaita.h>

G_BEGIN_DECLS

#define STICKYNOTE_TYPE_ROW stickynote_row_get_type ()

G_DECLARE_FINAL_TYPE (StickynoteRow, stickynote_row, STICKYNOTE, ROW, GtkListBoxRow)

GtkWidget *
stickynote_row_new (const char *title, const char *subtitle);

const char *
stickynote_row_get_title (StickynoteRow *row);

const char *
stickynote_row_get_subtitle (StickynoteRow *row);

void
stickynote_row_set_title (StickynoteRow *row, const char *title);

void
stickynote_row_set_subtitle (StickynoteRow *row, const char *subtitle);

void
stickynote_row_update_color_scheme (StickynoteRow *row, int color_scheme);

G_END_DECLS

#endif /* STICKYNOTE_ROW_H */