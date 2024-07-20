/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/
/**
Select methods copied from https://github.com/obsproject/obs-studio/blob/master/UI/obs-app.hpp
In some places just the method signature is [mostly] copied.
In some places [nearly] the full code implementation is copied.
 */
#pragma once

#include <QString>

#include <obs-frontend-api.h>
#include <obs-module.h>

// Copied from https://github.com/obsproject/obs-studio/blob/master/UI/qt-wrappers.hpp
#define QT_UTF8(str) QString::fromUtf8(str, -1)
#define QT_TO_UTF8(str) str.toUtf8().constData()

// Changed to use obs_frontend_get_global_config instead of ((OBSApp*)App())->GetGlobalConfig
inline config_t *GetGlobalConfig()
{
	return obs_frontend_get_global_config();
}

// Changed to use obs_module_text instead of ((OBSApp*)App())->GetString
inline const char *Str(const char *lookup)
{
	return obs_module_text(lookup);
}

inline QString QTStr(const char *lookupVal)
{
	return QString::fromUtf8(Str(lookupVal));
}
