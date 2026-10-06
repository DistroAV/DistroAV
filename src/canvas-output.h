/******************************************************************************
	Copyright (C) 2016-2024 DistroAV <contact@distroav.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, see <https://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once
#include <QString>
#include <QList>
#include <QPair>

void canvas_output_deinit();
void canvas_output_init();
void canvas_output_register_signals();
void canvas_output_unregister_signals();
QString canvas_output_last_error();

// List of {name, uuid} for all public canvases that have video
QList<QPair<QString, QString>> canvas_output_list_canvases();
