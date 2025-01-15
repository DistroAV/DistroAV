#include "obs-support/sync-debug.h"
#include <util/platform.h>

// Shared map to store the last log time for each message
static std::map<std::string, std::chrono::steady_clock::time_point> lastLogTime;

// Function to clear the lastLogTime map
void obs_clear_last_log_time()
{
	if (lastLogTime.size() == 0)
		return;

	lastLogTime.clear();
	obs_log(LOG_INFO, "Cleared lastLogTime map.");
}

// Function to log messages with a cooldown for each message
void obs_sync_debug_log(const char *message, const char *source_ndi_name,
			uint64_t timecode, uint64_t timestamp)
{
	if ((LOG_LEVEL != LOG_VERBOSE) || (lastLogTime.size() >= 100))
		return;

	auto now = std::chrono::steady_clock::now();

	std::string key = std::string(message) + " [" +
			  std::string(source_ndi_name) + "]";
	auto it = lastLogTime.find(key);
	if (it == lastLogTime.end() ||
	    std::chrono::duration_cast<std::chrono::seconds>(now - it->second)
			    .count() >= 5) {

		std::string timecodeStr = "N/A";
		if (timecode > (uint64_t)0) {
			timecodeStr =
				(timecode == LLONG_MAX)
					? "MAX"
					: std::to_string(timecode / 1000000);
		}

		std::string timestampStr = "N/A";

		if (timestamp > (uint64_t)0) {
			timestampStr =
				(timestamp == LLONG_MAX)
					? "MAX"
					: std::to_string(timestamp / 1000000);
		}

		obs_log(LOG_VERBOSE, "tc=%14s ts=%14s: %s", timecodeStr.c_str(),
			timestampStr.c_str(), key.c_str());
		lastLogTime[key] = now;
	}
}

std::map<std::string, bool> audio_on;
std::map<std::string, int64_t> audio_on_time;
std::map<std::string, bool> white_on;
std::map<std::string, int64_t> white_on_time;

int64_t obs_sync_white_time(int64_t time, uint8_t *p_data)
{
	uint8_t pixel0 = p_data[0];
	uint8_t pixel1 = p_data[1];
	bool white = (((pixel0 == 128) && (pixel1 == 235)) ||
		      ((pixel0 == 255) && (pixel1 == 255)));
	return white ? time : 0;
}
int64_t obs_sync_audio_time(int64_t time, float *p_data, int nsamples,
			    int samplerate)
{
	int64_t return_time = 0;
	int sample = 0;
	while (sample < nsamples) {
		auto sample_amp = p_data[sample];
		if (sample_amp != 0.0f) {
			int64_t ns_per_sample = 1000000000 / samplerate;
			return_time = time + sample * ns_per_sample;
			float sample_amp_prev = 0.0f;
			if (sample > 0)
				sample_amp_prev = p_data[sample - 1];
			return return_time;
		}
		sample++;
	}
	return return_time;
}

void obs_sync_debug_log_video_time(const char *message,
				   const char *source_ndi_name,
				   uint64_t timestamp, uint8_t *data)
{
	if ((LOG_LEVEL != LOG_VERBOSE) || (white_on.size() >= 100))
		return;
	std::string key = std::string(message) + " [" +
			  std::string(source_ndi_name) + "]";

	auto wo = white_on.find(key);
	if (wo == white_on.end()) {
		white_on[key] = false;
		wo = white_on.find(key);
	}

	// If white frame is going from off to on, log the frame time, audio time and diff
	auto white_time = obs_sync_white_time(timestamp, data);
	if (!wo->second && (white_time > 0)) {
		white_on[key] = true;
		white_on_time[key] = white_time;

		auto diff = white_on_time[key] - audio_on_time[key];

		std::string wtimeStr = "N/A";
		if (white_on_time[key] > (int64_t)0) {
			wtimeStr = (white_on_time[key] == LLONG_MAX)
					   ? "MAX"
					   : std::to_string(white_on_time[key] /
							    1000000);
		}

		std::string atimeStr = "N/A";
		if (audio_on_time[key] > (int64_t)0) {
			atimeStr = (audio_on_time[key] == LLONG_MAX)
					   ? "MAX"
					   : std::to_string(audio_on_time[key] /
							    1000000);
		}

		std::string dtimeStr = "N/A";
		if ((audio_on_time[key] > (int64_t)0) &&
		    (white_on_time[key] > (int64_t)0)) {

			dtimeStr = ((audio_on_time[key] == LLONG_MAX) ||
				    (white_on_time[key] == LLONG_MAX))
					   ? "MAX"
					   : std::to_string(diff / 1000000);
		}
		obs_log(LOG_VERBOSE,
			"~___~___ Sync Test Data Found: AT %14s WT %14s: %5s %s",
			atimeStr.c_str(), wtimeStr.c_str(), dtimeStr.c_str(),
			key.c_str());

	} else if (wo->second && (white_time == 0)) {
		white_on[key] = false;
	}
}
void obs_sync_debug_log_audio_time(const char *message,
				   const char *source_ndi_name,
				   uint64_t timestamp, float *data,
				   int no_samples, int sample_rate)
{
	if ((LOG_LEVEL != LOG_VERBOSE) || (audio_on.size() >= 100))
		return;
	std::string key = std::string(message) + " [" +
			  std::string(source_ndi_name) + "]";

	auto ao = audio_on.find(key);
	if (ao == audio_on.end()) {
		audio_on[key] = false; // set audio off
		ao = audio_on.find(key);
	}

	// If audio on, log the frame time
	auto audio_time =
		obs_sync_audio_time(timestamp, data, no_samples, sample_rate);
	if (!ao->second && (audio_time > 0)) {
		ao->second = true; // set audio on
		audio_on_time[key] = audio_time;
	} else if (ao->second && (audio_time == 0)) {
		ao->second = false;
	}
}
