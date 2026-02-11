/******************************************************************************
	Copyright (C) 2023 Norihiro Kamae <norihiro@nagater.net>
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

#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <obs.hpp>
#include "../sync-test-output.hpp"

// NDI timing information received from DistroAV ndi_source
// Must match the struct definition in ndi-source.cpp
typedef struct ndi_timing_info_t {
	int64_t ndi_timecode_ns;
	int64_t clock_offset_ns;
	int64_t buffer_ns;
	int64_t presentation_ns;
	int64_t obs_now_ns;
	int64_t ts_ahead_ns;
	int64_t pipeline_latency_ns;
	uint64_t frame_number;
} ndi_timing_info_t;

class SyncTestDock : public QFrame {
	Q_OBJECT

public:
	SyncTestDock(QWidget *parent = nullptr);
	~SyncTestDock();

private:
	QPushButton *resetButton = nullptr;

	// Simplified displays
	QLabel *latencyDisplay = nullptr;
	QLabel *frameDropDisplay = nullptr;
	QLabel *createdDisplay = nullptr;
	QLabel *receivedDisplay = nullptr;
	QLabel *diffDisplay = nullptr;

private:
	OBSOutput sync_test;
	OBSWeakSource ndi_source_ref;

private:
	// A/V sync tracking
	int last_video_ix = -1;
	int last_audio_ix = -1;
	int64_t total_frame_drops = 0;
	int64_t total_frames_seen = 0;

	// Logging tracking
	uint64_t last_log_ts = 0;
	int sync_count_since_log = 0;
	double latency_sum_since_log = 0.0;
	double last_latency_ms = 0.0;

	// NDI timing cache for logging
	int64_t last_ndi_timecode_ns = 0;
	int64_t last_receive_time_ns = 0;
	int64_t last_diff_ns = 0;

private:
	void start_output();
	void on_reset();
	void connect_to_ndi_source();
	void disconnect_from_ndi_source();
	void log_consolidated_status(uint64_t now_ts);

	void on_video_marker_found(video_marker_found_s data);
	void on_audio_marker_found(audio_marker_found_s data);
	void on_sync_found(sync_index data);
	void on_frame_drop_detected(frame_drop_event_s data);
	void on_ndi_timing(ndi_timing_info_t timing);

	static void cb_video_marker_found(void *param, calldata_t *cd);
	static void cb_audio_marker_found(void *param, calldata_t *cd);
	static void cb_sync_found(void *param, calldata_t *cd);
	static void cb_frame_drop_detected(void *param, calldata_t *cd);
	static void cb_ndi_timing(void *param, calldata_t *cd);
};

extern "C" QWidget *create_sync_test_dock();
