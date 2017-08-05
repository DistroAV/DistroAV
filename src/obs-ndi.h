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

#ifdef _WIN32
    #define PROCESSINGNDILIB_DEPRECATED __declspec(deprecated)
    #define PROCESSINGNDILIB_API extern "C" __declspec(dllimport)
#else
    #define PROCESSINGNDILIB_DEPRECATED
    #define PROCESSINGNDILIB_API extern "C" __attribute((visibility("default")))
#endif

#include <Processing.NDI.compat.h>
#include <Processing.NDI.structs.h>

#include <Processing.NDI.Find.h>
#include <Processing.NDI.Recv.h>
#include <Processing.NDI.Send.h>
#include <Processing.NDI.Routing.h>

#include <Processing.NDI.utilities.h>
#include <Processing.NDI.DynamicLoad.h>

#define OBS_NDI_VERSION "4.0.1"

#define blog(level, msg, ...) blog(level, "[obs-ndi] " msg, ##__VA_ARGS__)

void main_output_start(const char* output_name);
void main_output_stop();
bool main_output_is_running();

extern const NDIlib_v2* ndiLib;

#endif // OBSNDI_H
