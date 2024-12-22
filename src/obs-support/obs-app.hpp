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
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
******************************************************************************/

/******************************************************************************
Select methods copied from https://github.com/obsproject/obs-studio/blob/master/UI/obs-app.hpp
In some places just the method signature is [mostly] copied.
In some places [nearly] the full code implementation is copied.
******************************************************************************/

#pragma once

#include "qt_wrapper.hpp"

#include <obs-config.h>
#include <obs-frontend-api.h>
#include <obs-module.h>

inline config_t *GetAppConfig()
{
#if LIBOBS_API_MAJOR_VER >= 31
	return obs_frontend_get_app_config();
#else
	return obs_frontend_get_global_config();
#endif
}

inline config_t *GetUserConfig()
{
#if LIBOBS_API_MAJOR_VER >= 31
	return obs_frontend_get_user_config();
#else
	return obs_frontend_get_global_config();
#endif
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
