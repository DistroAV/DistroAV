/*
obs-ndi
Copyright (C) 2016-2018 Stéphane Lepin <steph  name of author

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; If not, see <https://www.gnu.org/licenses/>
*/

#ifndef OBSNDI_H
#define OBSNDI_H

#include <Processing.NDI.Lib.h>

#define OBS_NDI_VERSION "4.6.2"
#define OBS_NDI_ALPHA_FILTER_ID "premultiplied_alpha_filter"

#define ndiblog(level, msg, ...) blog(level, "[obs-ndi] " msg, ##__VA_ARGS__)


extern const NDIlib_v3* ndiLib;

#endif // OBSNDI_H
