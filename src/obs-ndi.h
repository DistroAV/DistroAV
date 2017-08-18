/*
obs-ndi (NDI I/O in OBS Studio)
Copyright (C) 2016 Stéphane Lepin <stephane.lepin@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library. If not, see <https://www.gnu.org/licenses/>
*/

#ifndef OBSNDI_H
#define OBSNDI_H

#include <Processing.NDI.Lib.h>

#define OBS_NDI_VERSION "4.1.0"
#define OBS_NDI_ALPHA_FILTER_ID "premultiplied_alpha_filter"

#define blog(level, msg, ...) blog(level, "[obs-ndi] " msg, ##__VA_ARGS__)

void main_output_start(const char* output_name);
void main_output_stop();
bool main_output_is_running();

extern const NDIlib_v3* ndiLib;

#endif // OBSNDI_H
