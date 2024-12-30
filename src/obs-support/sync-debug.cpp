#include "obs-support/sync-debug.h"

// Shared map to store the last log time for each message
static std::map<std::string, std::chrono::steady_clock::time_point> lastLogTime;

// Function to clear the lastLogTime map
void obs_clear_last_log_time()
{
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
