#pragma once

#include <map>
#include <string>
#include <chrono>
#include <obs-module.h>
#include "plugin-main.h"
#include "plugin-support.h"

void obs_sync_debug_log(const char *checkpoint, const char *source_ndi_name,
			uint64_t timecode, uint64_t timestamp);
void obs_sync_debug_log_video_time(const char *message,
				   const char *source_ndi_name,
				   NDIlib_video_frame_v2_t *video_frame);
void obs_sync_debug_log_audio_time(const char *message,
				   const char *source_ndi_name,
				   NDIlib_audio_frame_v2_t *audio_frame);
void obs_sync_debug_log_audio_time(const char *message,
				   const char *source_ndi_name,
				   NDIlib_audio_frame_v3_t *audio_frame);
void obs_clear_last_log_time();

// Uncomment to enable measuring audio and video time differences
// using black/white frames and audio on white frames
// #define SYNC_DEBUG 1
#ifdef SYNC_DEBUG
#define OBS_SYNC_DEBUG_LOG_VIDEO_TIME(message, source_ndi_name, video_frame) \
	obs_sync_debug_log_video_time(message, source_ndi_name, video_frame)
#else
#define OBS_SYNC_DEBUG_LOG_VIDEO_TIME(message, source_ndi_name, video_frame) \
	do {                                                                 \
	} while (0)
#endif
#ifdef SYNC_DEBUG
#define OBS_SYNC_DEBUG_LOG_AUDIO_TIME(message, source_ndi_name, audio_frame2) \
	obs_sync_debug_log_audio_time(message, source_ndi_name, audio_frame2)
#else
#define OBS_SYNC_DEBUG_LOG_AUDIO_TIME(message, source_ndi_name, audio_frame2) \
	do {                                                                  \
	} while (0)
#endif