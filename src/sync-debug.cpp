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

#include "sync-debug.h"
#include <util/platform.h>
#include <mutex>
#include <utility>

// Consolidated per-key state
struct sync_key_state {
	std::mutex mutex; // protects the fields below for this key

	// audio state
	bool audio_on = false;
	int64_t audio_on_time = -1;
	int64_t audio_off_time = -1;
	int audio_sync_count = 0;

	// white/video state
	bool white_on = false;
	int64_t white_on_time = -1;
	int64_t white_off_time = -1;
	int video_sync_count = 0;
};

static std::map<std::string, sync_key_state> key_state_map;
// Guard to protect creation of per-key entries
static std::mutex key_state_map_guard;

int64_t sync_white_time(int64_t time, uint8_t *p_data)
{
	uint8_t pixel0 = p_data[0];
	uint8_t pixel1 = p_data[1];
	bool white = (((pixel0 == 128) && (pixel1 == 235)) || ((pixel0 == 255) && (pixel1 == 255)));
	return white ? time : 0;
}

int64_t sync_audio_on_time(int64_t time, float *p_data, int nsamples, int samplerate)
{
	int64_t return_time = -1;
	int64_t sample = 0;
	float last_amp = 0.0f;
	while (sample < nsamples) {
		float sample_amp = p_data[sample];
		if (sample_amp != last_amp) {
			int64_t ns_per_sample = 1000000000 / samplerate;
			return_time = time + (sample * ns_per_sample);
			return return_time;
		}
		sample++;
	}
	return return_time;
}

int64_t sync_audio_off_time(int64_t time, float *p_data, int nsamples, int samplerate)
{
	int64_t return_time = -1;
	int64_t sample = 0;
	float last_amp = 0.0f;
	while (sample < nsamples) {
		float sample_amp = p_data[sample];
		if (sample_amp == last_amp) {
			int64_t ns_per_sample = 1000000000 / samplerate;
			return_time = time + (sample * ns_per_sample);
			return return_time;
		}
		sample++;
	}
	return return_time;
}
const int64_t max_offset = 2000000000LL; //2 seconds

void sync_debug_log(const char *message, int64_t timestamp, int64_t *audio_on_time, int64_t *audio_off_time,
		    int audio_sync_count, int64_t *white_on_time, int64_t *white_off_time, int video_sync_count)
{
	if (timestamp > std::max<int64_t>(*audio_on_time, *white_on_time) + max_offset) {
		if (*white_on_time > 0 && *audio_on_time > 0 && audio_sync_count > 0 && video_sync_count > 0) {
			int64_t diff = *white_on_time - *audio_on_time;
			obs_log(LOG_DEBUG,
				"Sync A/V   AT: %15lld AW: %5lld AC: %3d VT: %15lld VW: %5lld VC: %3d Delta: %5lld %s",
				*audio_on_time / 1000000,
				*audio_off_time > 0 ? (*audio_off_time - *audio_on_time) / 1000000 : -1,
				audio_sync_count, *white_on_time / 1000000,
				*white_off_time > 0 ? (*white_off_time - *white_on_time) / 1000000 : -1,
				video_sync_count, diff / 1000000, message);
			*audio_on_time = 0;
			*audio_off_time = 0;
			*white_on_time = 0;
			*white_off_time = 0;
		}
		if (*white_on_time > 0 && video_sync_count > 0) {
			obs_log(LOG_DEBUG,
				"Sync Video AT: --------------- AW: ----- AC: --- VT: %15lld VW: %5lld VC: %3d Delta: ----- %s",
				*white_on_time / 1000000,
				*white_off_time > 0 ? (*white_off_time - *white_on_time) / 1000000 : -1,
				video_sync_count, message);
			*white_on_time = 0;
			*white_off_time = 0;
		}

		if (*audio_on_time > 0 && audio_sync_count > 0) {
			obs_log(LOG_DEBUG,
				"Sync Audio AT: %15lld AW: %5lld AC: %3d VT: --------------- VW: ----- VC: --- Delta: ----- %s",
				*audio_on_time / 1000000,
				*audio_off_time > 0 ? (*audio_off_time - *audio_on_time) / 1000000 : -1,
				audio_sync_count, message);
			*audio_on_time = 0;
			*audio_off_time = 0;
		}
	}
}

void sync_debug_log_video_time(const char *message, const char *source_ndi_name, uint64_t timestamp, uint8_t *data)
{
	std::string key = std::string(message) + " [" + std::string(source_ndi_name) + "]";

	// Ensure an entry exists for this key
	{
		std::lock_guard<std::mutex> g(key_state_map_guard);
		if (key_state_map.find(key) == key_state_map.end())
			key_state_map.try_emplace(key);
	}

	// Lock the per-key mutex for the remainder of the function
	sync_key_state &st = key_state_map[key];
	std::unique_lock<std::mutex> key_lock(st.mutex);

	int64_t white_time = sync_white_time(timestamp, data);

	if (!st.white_on && (white_time > 0)) {
		st.white_on = true;
		if (st.white_on_time != -1)
			(st.video_sync_count)++;
		st.white_on_time = white_time;
	} else if (st.white_on && (white_time == 0)) {
		st.white_off_time = timestamp;
		st.white_on = false;
	} else if (st.white_on_time == -1)
		st.white_on_time = 0;

	// Call sync_debug_log while holding the per-key lock to ensure consistent reads/writes
	sync_debug_log(key.c_str(), timestamp, &st.audio_on_time, &st.audio_off_time, st.audio_sync_count,
		       &st.white_on_time, &st.white_off_time, st.video_sync_count);
}

void sync_debug_log_audio_time(const char *message, const char *source_ndi_name, uint64_t timestamp, float *data,
			       int no_samples, int sample_rate)
{
	std::string key = std::string(message) + " [" + std::string(source_ndi_name) + "]";

	// Ensure an entry exists for this key
	{
		std::lock_guard<std::mutex> g(key_state_map_guard);
		if (key_state_map.find(key) == key_state_map.end())
			key_state_map.try_emplace(key);
	}

	// Lock the per-key mutex for the remainder of the function
	sync_key_state &st = key_state_map[key];
	std::unique_lock<std::mutex> key_lock(st.mutex);

	int64_t audio_time = sync_audio_on_time(timestamp, data, no_samples, sample_rate);

	if (!st.audio_on && (audio_time > 0)) {
		st.audio_on = true;
		if (st.audio_on_time != -1)
			(st.audio_sync_count)++;
		st.audio_on_time = audio_time;
	} else if (st.audio_on) {
		audio_time = sync_audio_off_time(timestamp, data, no_samples, sample_rate);
		if (audio_time > 0) {
			st.audio_off_time = audio_time;
			st.audio_on = false;
		}
	} else if (st.audio_on_time == -1)
		st.audio_on_time = 0;

	// Call sync_debug_log while holding the per-key lock to ensure consistent reads/writes
	sync_debug_log(key.c_str(), timestamp, &st.audio_on_time, &st.audio_off_time, st.audio_sync_count,
		       &st.white_on_time, &st.white_off_time, st.video_sync_count);
}
