/******************************************************************************
	Copyright (C) 2016-2026 DistroAV <contact@distroav.org>

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
// SenderInfo holds information about a registered NDI sender. Access to
// its fields is provided via thread-safe getters and setters that lock an
// internal mutex.

#include "obs.h"
#include "change-notifier.h"
#include <mutex>
#include <string>
#include <map>
#include <Processing.NDI.Lib.h>
// Lightweight stats snapshot used by the UI / reporting.
struct NDISenderStats {
	size_t receivers = 0;
	uint64_t receiver_changes = 0;
	bool discoverable = true;
	uint64_t discoverable_changes = 0;
	double target_fps = 0.0;
	double target_sps = 0.0;
	int xres = 0;
	int yres = 0;
	NDIlib_FourCC_video_type_e fourcc = NDIlib_FourCC_video_type_max;
	int frame_rate_N = 0;
	int frame_rate_D = 0;
	std::string format_description = "";
	size_t video_frame_count = 0;
	uint64_t last_video_frame_timestamp = 0;
	uint64_t last_video_frame_os = 0;
	uint64_t start_video_frame_timestamp = 0;
	uint64_t start_video_frame_os = 0;
	double tsFPS = 0.;
	double osFPS = 0.;
	double deficit_fps = 0.0;                      // fraction of target FPS that was not achieved
	double deficit_sps = 0.0;                      // fraction of target SPS that was not achieved
	double budget_used_per_frame_blocking = 0.0;   // fraction of time spent in send per frame on average
	double budget_used_per_frame_processing = 0.0; // fraction of time spent in processing per frame on average
	double max_send_block_pct = 0.0; // fraction of the target frame period used by the worst-case send-block call
	double max_process_pct = 0.0;    // fraction of the target frame period used by the worst-case processing call
	size_t audio_sample_count = 0;
	uint64_t last_audio_sample_timestamp = 0;
	uint64_t last_audio_sample_os = 0;
	uint64_t start_audio_sample_timestamp = 0;
	uint64_t start_audio_sample_os = 0;
	double tsSPS = 0.; // samples per second timestamp
	double osSPS = 0.; // samples per second os time

	uint64_t tot_frame_interval = 0; // rolling avg of callback deltas
	uint64_t max_frame_interval = 0; // worst-case in window
	uint64_t avg_frame_interval = 0; // rolling avg of callback deltas
	double jitter_ratio = 0.0;
	uint64_t tot_video_send_block = 0; // time inside NDIlib_send_send_video_*
	uint64_t max_video_send_block = 0; // worst-case send-block time in window
	uint64_t avg_video_send_block = 0; // rolling avg of send block times
	uint64_t tot_video_processing = 0; // time spent in video processing (e.g., color conversion)
	uint64_t max_video_processing = 0; // worst-case processing time in window
	uint64_t avg_video_processing = 0; // rolling avg of video processing times
};

class SenderInfo {
public:
	// Construct with the NDI sender handle so calculate_stats() can query ndiLib.
	explicit SenderInfo(NDIlib_send_instance_t sender);

	// Thread-safe accessors (declarations only).
	size_t get_receivers() const;
	void set_receivers(size_t r);

	bool is_discoverable() const;
	void set_discoverable(bool d);

	// ChangeNotifier registration keyed by tag.
	void registerChangeNotifier(ChangeNotifier *notifier, const std::string &key);
	void unregisterChangeNotifier(ChangeNotifier *notifier, const std::string &key);

	// Emit all registered notifiers.
	void notify_change();

	std::string get_tag() const;
	void set_tag(const std::string &t);

	void set_ndi_name(const std::string &ndi_name);
	std::string get_ndi_name() const;

	// Return a thread-safe copy of the latest snapshot.
	NDISenderStats getReportSnapshot() const;

	// Short textual status for logging/debug UI.
	std::string get_brief_sender_status() const;
	static std::string get_missing_sender_status();

	// Frame/audio reporting - called from the sending code path.
	void send_video_frame(uint64_t t0, uint64_t t1, uint64_t t2, NDIlib_video_frame_v2_t &ndi_frame,
			      video_data *frame);
	void send_audio_frame(uint64_t t0, uint64_t t1, uint64_t t2, NDIlib_audio_frame_v3_t &ndi_frame,
			      uint64_t timestamp);

	// Compute statistics and update the snapshot. Implemented in the .cpp file
	// because it needs access to global externs from plugin-main.h.
	void calculate_stats();

private:
	NDIlib_send_instance_t m_sender;
	mutable std::mutex m_mutex;

	// Map of tag -> ChangeNotifier*
	std::map<std::string, ChangeNotifier *> changeNotifiers;
	std::string tag;
	std::string ndi_name;

	// Running counters updated on send_* calls, and a snapshot copied for UI.
	NDISenderStats running;
	NDISenderStats snapshot;
};
