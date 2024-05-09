/*
obs-ndi
Copyright (C) 2016-2024 OBS-NDI Project <obsndi@obsndiproject.com>

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

#ifndef OBSNDI_H
#define OBSNDI_H

#include "plugin-support.h"
#include "Config.h"

#include <Processing.NDI.Lib.h>

#define OBS_NDI_ALPHA_FILTER_ID "premultiplied_alpha_filter"

// Required per `NDI SDK License Agreement.pdf` `3 LICENSING`
// "• Your application must provide a link to http://ndi.video ..."
#define NDI_WEB_URL "https://ndi.video"

#if !(defined(_WIN32) || defined(__APPLE__))
// Linux
#undef NDILIB_REDIST_URL
#define NDILIB_REDIST_URL \
	"https://github.com/obs-ndi/obs-ndi/blob/master/CI/libndi-get.sh"
#endif

void main_output_start(const char *output_name);
void main_output_stop();
bool main_output_is_running();

struct Config;
typedef std::shared_ptr<Config> ConfigPtr;

ConfigPtr GetConfig();

extern const NDIlib_v4 *ndiLib;

#endif // OBSNDI_H
