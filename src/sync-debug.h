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

#include <map>
#include <string>
#include <chrono>
#include <obs-module.h>
#include "plugin-main.h"
#include "plugin-support.h"

void sync_debug_log_video_time(const char *message, const char *source_ndi_name, uint64_t timestamp, uint8_t *data);
void sync_debug_log_audio_time(const char *message, const char *source_ndi_name, uint64_t timestamp, float *data,
			       int no_samples, int sample_rate);

// Uncomment to enable measuring audio and video time differences
// using black/white frames and audio on white frames
//#define SYNC_DEBUG 1
#ifdef SYNC_DEBUG
#define SYNC_DEBUG_LOG_VIDEO_TIME(message, source_ndi_name, timestamp, data)                              \
    if (data) sync_debug_log_video_time(message, source_ndi_name, timestamp, data)
#else
#define SYNC_DEBUG_LOG_VIDEO_TIME(message, source_ndi_name, timestamp, data)                                \
	do {} while (0)
#endif
#ifdef SYNC_DEBUG
#define SYNC_DEBUG_LOG_AUDIO_TIME(message, source_ndi_name, timestamp, data, no_samples, sample_rate)       \
	if (data) sync_debug_log_audio_time(message, source_ndi_name, timestamp, \
				      data, no_samples, sample_rate)
#else
#define SYNC_DEBUG_LOG_AUDIO_TIME(message, source_ndi_name, timestamp, data, no_samples, sample_rate)       \
	do {} while (0)
#endif
