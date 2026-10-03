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
// ReceiverInfo holds information about a registered NDI receiver. Access to
// its fields is provided via thread-safe getters and setters that lock an
// internal mutex.
#include "obs.h"
#include "change-notifier.h"
#include <mutex>
#include <string>
#include <map>
#include <limits>
#include <Processing.NDI.Lib.h>

struct NDIReceiverStats {
	size_t video_frames_dropped = 0;
	size_t audio_samples_dropped = 0;
	size_t video_queue_size = 0;
	size_t audio_queue_size = 0;
	size_t video_frame_count = 0;
	double target_fps = 0.0;
	double target_sps = 0.0;
	int xres = 0;
	int yres = 0;
	NDIlib_FourCC_video_type_e fourcc = NDIlib_FourCC_video_type_max;
	int frame_rate_N = 0;
	int frame_rate_D = 0;
	std::string format_description = "";
	std::string obs_source_name = "";

	int64_t av_drift_ns = 0; // current selected/output-style A/V projected drift
	int64_t av_drift_min_ns = std::numeric_limits<int64_t>::infinity();
	int64_t av_drift_max_ns = std::numeric_limits<int64_t>::lowest();
	double av_drift_ns_per_hour = 0.0;
	// True once the drift regression spans enough real time to be a trustworthy rate
	// estimate (see kMinDriftSampleSpanSeconds). Before that, av_drift_ns_per_hour is
	// still being accumulated and callers should display "Sampling..." instead.
	bool av_drift_ready = false;

	uint64_t last_video_frame_timestamp = 0;
	uint64_t last_video_frame_os = 0;
	uint64_t start_video_frame_timestamp = 0;
	uint64_t start_video_frame_os = 0;
	double tsFPS = 0.;
	double osFPS = 0.;
	double deficit_fps = 0.0;                      // fraction of target FPS that was not achieved
	double deficit_sps = 0.0;                      // fraction of target SPS that was not achieved
	double budget_used_per_frame_capture = 0.0;    // fraction of time spent in send per frame on average
	double budget_used_per_frame_processing = 0.0; // fraction of time spent in processing per frame on average
	double max_capture_pct = 0.0; // fraction of the target frame period used by the worst-case capture call
	double max_process_pct = 0.0; // fraction of the target frame period used by the worst-case processing call

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
	uint64_t tot_video_capture = 0; // time inside NDIlib_receive_capture_video_*
	uint64_t max_video_capture = 0;
	uint64_t avg_video_capture = 0;    // rolling avg of video capture times
	uint64_t tot_video_processing = 0; // time inside NDIlib_receive_capture_video_*
	uint64_t max_video_processing = 0;
	uint64_t avg_video_processing = 0; // rolling avg of video processing times
};

// Formats av_drift_ns_per_hour as "<ms/hr>" with 2 decimals, or "Sampling..." while
// av_drift_ready is still false. Shared by every place that displays/logs the drift
// rate so they can't disagree with each other (e.g. the ASCII report's column-width
// computation and its row rendering).
std::string format_av_drift_ms_per_hour(const NDIReceiverStats &stats);

class ReceiverInfo {
public:
	// Construct with the NDI receiver handle
	explicit ReceiverInfo(obs_source_t *ndi_source);

	// ChangeNotifier registration keyed by tag.
	void registerChangeNotifier(ChangeNotifier *notifier, const std::string &key);
	void unregisterChangeNotifier(ChangeNotifier *notifier, const std::string &key);

	// Emit all registered notifiers.
	void notify_change();

	// NDI name helpers
	void set_ndi_name(const std::string &ndi_name);
	std::string get_ndi_name() const;

	// Set receiver instance. When set to a new non-null receiver (i.e. the
	// underlying NDI receiver was just (re)created), also resets accumulated
	// stats, since any previously running/reported stats no longer apply.
	void set_receiver(NDIlib_recv_instance_t receiver);

	// Snapshot accessor
	NDIReceiverStats getReportSnapshot() const;

	// Short textual status for logging/debug UI.
	std::string get_brief_receiver_status() const;
	static std::string get_missing_receiver_status();

	// Callbacks used by the receiver processing code.
	void receive_video_frame(uint64_t t0, uint64_t t1, uint64_t t2, NDIlib_video_frame_v2_t &ndi_frame);
	void receive_audio_frame(uint64_t t0, uint64_t t1, uint64_t t2, NDIlib_audio_frame_v3_t &ndi_frame);

	// Compute statistics and update snapshot (implementation in .cpp)
	void calculate_stats();
	void add_drift_sample(uint64_t wall_ns, int64_t drift_ns);

private:
	// Clears running/snapshot stats and AV-drift tracking state. Caller must hold m_mutex.
	void reset_stats_locked();

	// Debug instrumentation: logs the raw inputs behind one A/V-drift sample so an
	// out-of-range drift value can be traced back to the timestamps that produced it.
	// Caller must hold m_mutex.
	void log_drift_sample_locked(const char *trigger, uint64_t video_ts_ns, uint64_t video_wall_ns,
				     uint64_t audio_ts_ns, uint64_t audio_wall_ns, int64_t drift_ns) const;

	mutable std::mutex m_mutex;
	obs_source_t *m_source = nullptr;
	NDIlib_recv_instance_t m_receiver;

	// Map of tag -> ChangeNotifier*
	std::map<std::string, ChangeNotifier *> changeNotifiers;
	std::string ndi_name;

	// AV drift tracking
	uint64_t last_video_ts_ns_ = 0;
	uint64_t last_video_wall_ns_ = 0;
	uint64_t last_audio_ts_ns_ = 0;
	uint64_t last_audio_wall_ns_ = 0;
	bool have_av_drift_ = false;
	double reg_n_ = 0.0;
	double reg_sum_x_ = 0.0;
	double reg_sum_y_ = 0.0;
	double reg_sum_xy_ = 0.0;
	double reg_sum_xx_ = 0.0;
	uint64_t reg_start_wall_ns_ = 0;

	// Running counters and published snapshot
	NDIReceiverStats snapshot;
	NDIReceiverStats running;
};
