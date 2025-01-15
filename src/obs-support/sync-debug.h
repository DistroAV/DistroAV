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
				   uint64_t timestamp, uint8_t *data);
void obs_sync_debug_log_audio_time(const char *message,
				   const char *source_ndi_name,
				   uint64_t timestamp, float *data,
				   int no_samples, int sample_rate);
void obs_clear_last_log_time();

// Uncomment to enable measuring audio and video time differences
// using black/white frames and audio on white frames
#define SYNC_DEBUG 1
#ifdef SYNC_DEBUG
#define OBS_SYNC_DEBUG_LOG_VIDEO_TIME(message, source_ndi_name, timestamp, \
				      data)                                \
	obs_sync_debug_log_video_time(message, source_ndi_name, timestamp, data)
#else
#define OBS_SYNC_DEBUG_LOG_VIDEO_TIME(message, source_ndi_name, timestamp, \
				      data)                                \
	do {                                                               \
	} while (0)
#endif
#ifdef SYNC_DEBUG
#define OBS_SYNC_DEBUG_LOG_AUDIO_TIME(message, source_ndi_name, timestamp, \
				      data, no_samples, sample_rate)       \
	obs_sync_debug_log_audio_time(message, source_ndi_name, timestamp, \
				      data, no_samples, sample_rate)
#else
#define OBS_SYNC_DEBUG_LOG_AUDIO_TIME(message, source_ndi_name, timestamp, \
				      data, no_samples, sample_rate)       \
	do {                                                               \
	} while (0)
#endif
