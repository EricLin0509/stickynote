/* color-scheme.h
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

#ifndef COLOR_SCHEME_H
#define COLOR_SCHEME_H

#define COLOR_SCHEME_COUNT 6 // This must be updated if you add or remove color schemes

#ifdef COLOR_SCHEME_IMPLEMENTATION
static const char *stickynote_color_scheme[] = {
	"red-stickynote",
	"orange-stickynote",
	"yellow-stickynote",
	"green-stickynote",
	"blue-stickynote",
	"purple-stickynote"
};
#endif

#endif /* COLOR_SCHEME_H */