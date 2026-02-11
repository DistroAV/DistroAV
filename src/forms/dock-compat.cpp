/******************************************************************************
	Copyright (C) 2023 Norihiro Kamae <norihiro@nagater.net>
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

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QWidget>
#include "dock-compat.hpp"

#define QT_UTF8(str) QString::fromUtf8(str, -1)

OBSDock::OBSDock(QMainWindow *main) : QDockWidget(main) {}

#if LIBOBS_API_VER <= MAKE_SEMANTIC_VERSION(29, 1, 3)
extern "C" bool obs_frontend_add_dock_by_id_compat(const char *id, const char *title, void *widget)
{
	auto *main = (QMainWindow *)obs_frontend_get_main_window();
	auto *dock = new OBSDock(main);

	dock->setWidget((QWidget *)widget);
	dock->setWindowTitle(QT_UTF8(title));
	dock->setObjectName(QT_UTF8(id));
	obs_frontend_add_dock(dock);
	dock->setFloating(true);
	dock->setVisible(false);

	return true;
}
#endif
