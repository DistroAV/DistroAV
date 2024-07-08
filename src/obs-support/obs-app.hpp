/*
obs-ndi
Copyright (C) 2016-2023 Stéphane Lepin <stephane.lepin@gmail.com>
Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/
/**
 * Select methods copied from https://github.com/obsproject/obs-studio/blob/master/UI/obs-app.hpp
 * In some places just the method signature is [mostly] copied.
 * In some places [nearly] the full code implementation is copied.
 */
#pragma once

#include <QString>

#include <obs-frontend-api.h>

config_t *GetGlobalConfig();
const char *Str(const char *lookup);
QString QTStr(const char *lookupVal);
