#include "obs-support/sync-debug.h"
#include <util/platform.h>

std::map<std::string, bool> audio_on_map;
std::map<std::string, int64_t> audio_on_time_map;
std::map<std::string, int64_t> audio_off_time_map;
std::map<std::string, int> audio_sync_count_map;
std::map<std::string, bool> white_on_map;
std::map<std::string, int64_t> white_on_time_map;
std::map<std::string, int64_t> white_off_time_map;
std::map<std::string, int> video_sync_count_map;

int64_t obs_sync_white_time(int64_t time, uint8_t *p_data)
{
	uint8_t pixel0 = p_data[0];
	uint8_t pixel1 = p_data[1];
	bool white = (((pixel0 == 128) && (pixel1 == 235)) || ((pixel0 == 255) && (pixel1 == 255)));
	return white ? time : 0;
}

int64_t obs_sync_audio_on_time(int64_t time, float *p_data, int nsamples, int samplerate)
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

int64_t obs_sync_audio_off_time(int64_t time, float *p_data, int nsamples, int samplerate)
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

void obs_sync_debug_log(const char *message, int64_t timestamp, int64_t *audio_on_time, int64_t *audio_off_time,
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

void obs_sync_debug_log_video_time(const char *message, const char *source_ndi_name, uint64_t timestamp, uint8_t *data)
{
	std::string key = std::string(message) + " [" + std::string(source_ndi_name) + "]";

	bool *white_on = nullptr;
	int64_t *white_on_time = nullptr;
	int64_t *white_off_time = nullptr;
	int *video_sync_count = nullptr;

	auto wo = white_on_map.find(key);
	if (wo == white_on_map.end()) {
		white_on_map[key] = false;
		white_on_time_map[key] = -1;
		white_off_time_map[key] = -1;
		video_sync_count_map[key] = 0;
	}

	white_on = &white_on_map[key];
	white_on_time = &white_on_time_map[key];
	white_off_time = &white_off_time_map[key];
	video_sync_count = &video_sync_count_map[key];

	bool *audio_on = nullptr;
	int64_t *audio_on_time = nullptr;
	int64_t *audio_off_time = nullptr;
	int *audio_sync_count = nullptr;

	auto ao = audio_on_map.find(key);
	if (ao == audio_on_map.end()) {
		audio_on_map[key] = false;
		audio_on_time_map[key] = -1;
		audio_off_time_map[key] = -1;
		audio_sync_count_map[key] = 0;
	}

	audio_on = &audio_on_map[key];
	audio_on_time = &audio_on_time_map[key];
	audio_off_time = &audio_off_time_map[key];
	audio_sync_count = &audio_sync_count_map[key];
	int64_t white_time = obs_sync_white_time(timestamp, data);

	if (!*white_on && (white_time > 0)) {
		*white_on = true;
		if (*white_on_time != -1)
			(*video_sync_count)++;
		*white_on_time = white_time;
		//obs_log(LOG_DEBUG,"%s White on at %lld",key.c_str(),white_on_time);
	} else if (*white_on && (white_time == 0)) {
		*white_off_time = timestamp;
		*white_on = false;
		//obs_log(LOG_DEBUG, "%s White off at %lld", key.c_str(), white_off_time);
	} else if (*white_on_time == -1)
		*white_on_time = 0;

	obs_sync_debug_log(key.c_str(), timestamp, audio_on_time, audio_off_time, *audio_sync_count, white_on_time,
			   white_off_time, *video_sync_count);
}
void obs_sync_debug_log_audio_time(const char *message, const char *source_ndi_name, uint64_t timestamp, float *data,
				   int no_samples, int sample_rate)
{
	std::string key = std::string(message) + " [" + std::string(source_ndi_name) + "]";

	bool *white_on = nullptr;
	int64_t *white_on_time = nullptr;
	int64_t *white_off_time = nullptr;
	int *video_sync_count = nullptr;

	auto wo = white_on_map.find(key);
	if (wo == white_on_map.end()) {
		white_on_map[key] = false;
		white_on_time_map[key] = -1;
		white_off_time_map[key] = -1;
		video_sync_count_map[key] = 0;
	}

	white_on = &white_on_map[key];
	white_on_time = &white_on_time_map[key];
	white_off_time = &white_off_time_map[key];
	video_sync_count = &video_sync_count_map[key];

	bool *audio_on = nullptr;
	int64_t *audio_on_time = nullptr;
	int64_t *audio_off_time = nullptr;
	int *audio_sync_count = nullptr;

	auto ao = audio_on_map.find(key);
	if (ao == audio_on_map.end()) {
		audio_on_map[key] = false;
		audio_on_time_map[key] = -1;
		audio_off_time_map[key] = -1;
		audio_sync_count_map[key] = 0;
	}

	audio_on = &audio_on_map[key];
	audio_on_time = &audio_on_time_map[key];
	audio_off_time = &audio_off_time_map[key];
	audio_sync_count = &audio_sync_count_map[key];
	int64_t audio_time = obs_sync_audio_on_time(timestamp, data, no_samples, sample_rate);

	if (!*audio_on && (audio_time > 0)) {
		*audio_on = true;
		if (*audio_on_time != -1)
			(*audio_sync_count)++;
		*audio_on_time = audio_time;
		//obs_log(LOG_DEBUG, "%s Audio on at %lld", key.c_str(), audio_on_time);
	} else if (*audio_on) {
		audio_time = obs_sync_audio_off_time(timestamp, data, no_samples, sample_rate);
		if (audio_time > 0) {
			*audio_off_time = audio_time;
			*audio_on = false;
			//obs_log(LOG_DEBUG, "%s Audio off at %lld", key.c_str(), audio_off_time);
		}
	} else if (*audio_on_time == -1)
		*audio_on_time = 0;

	obs_sync_debug_log(key.c_str(), timestamp, audio_on_time, audio_off_time, *audio_sync_count, white_on_time,
			   white_off_time, *video_sync_count);
}
