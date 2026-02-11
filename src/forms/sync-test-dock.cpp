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

#include <obs-module.h>
#include <inttypes.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>
#include <QMainWindow>
#include <obs-frontend-api.h>
#include "sync-test-dock.hpp"

// Format nanoseconds as time-of-day HH:MM:SS.mmm (UTC)
static QString format_time_ns(int64_t ns)
{
	// Convert from nanoseconds to milliseconds, then to time-of-day
	int64_t total_ms = ns / 1000000;

	// Get time-of-day (milliseconds since midnight)
	int64_t tod_ms = total_ms % 86400000LL;  // 86400000 = 24*60*60*1000
	if (tod_ms < 0) tod_ms += 86400000LL;    // Handle negative values

	int64_t ms = tod_ms % 1000;
	int64_t total_sec = tod_ms / 1000;
	int64_t sec = total_sec % 60;
	int64_t min = (total_sec / 60) % 60;
	int64_t hours = (total_sec / 3600) % 24;

	return QStringLiteral("%1:%2:%3.%4")
		.arg(hours, 2, 10, QChar('0'))
		.arg(min, 2, 10, QChar('0'))
		.arg(sec, 2, 10, QChar('0'))
		.arg(ms, 3, 10, QChar('0'));
}

SyncTestDock::SyncTestDock(QWidget *parent) : QFrame(parent)
{
	QVBoxLayout *mainLayout = new QVBoxLayout();
	QGridLayout *topLayout = new QGridLayout();

	int y = 0;

	// Reset button
	resetButton = new QPushButton(obs_module_text("NDIPlugin.SyncDock.Reset"), this);
	mainLayout->addWidget(resetButton);
	connect(resetButton, &QPushButton::clicked, this, &SyncTestDock::on_reset);

	QLabel *label;

	// A/V Sync Latency
	label = new QLabel(obs_module_text("NDIPlugin.SyncDock.AVSync"), this);
	label->setProperty("class", "text-large");
	topLayout->addWidget(label, y, 0);

	latencyDisplay = new QLabel("-", this);
	latencyDisplay->setObjectName("latencyDisplay");
	latencyDisplay->setProperty("class", "text-large");
	topLayout->addWidget(latencyDisplay, y++, 1);

	// Frame Drops
	label = new QLabel(obs_module_text("NDIPlugin.SyncDock.Drops"), this);
	topLayout->addWidget(label, y, 0);

	frameDropDisplay = new QLabel("-", this);
	frameDropDisplay->setObjectName("frameDropDisplay");
	topLayout->addWidget(frameDropDisplay, y++, 1);

	// Separator line (visual only)
	QFrame *line = new QFrame(this);
	line->setFrameShape(QFrame::HLine);
	line->setFrameShadow(QFrame::Sunken);
	topLayout->addWidget(line, y++, 0, 1, 2);

	// Created time (NDI timecode from sender)
	label = new QLabel(obs_module_text("NDIPlugin.SyncDock.Created"), this);
	topLayout->addWidget(label, y, 0);

	createdDisplay = new QLabel("-", this);
	createdDisplay->setObjectName("createdDisplay");
	topLayout->addWidget(createdDisplay, y++, 1);

	// Received time (wall clock when received)
	label = new QLabel(obs_module_text("NDIPlugin.SyncDock.Received"), this);
	topLayout->addWidget(label, y, 0);

	receivedDisplay = new QLabel("-", this);
	receivedDisplay->setObjectName("receivedDisplay");
	topLayout->addWidget(receivedDisplay, y++, 1);

	// Diff (network latency)
	label = new QLabel(obs_module_text("NDIPlugin.SyncDock.Diff"), this);
	topLayout->addWidget(label, y, 0);

	diffDisplay = new QLabel("-", this);
	diffDisplay->setObjectName("diffDisplay");
	topLayout->addWidget(diffDisplay, y++, 1);

	mainLayout->addLayout(topLayout);
	setLayout(mainLayout);

	QTimer::singleShot(0, this, [this]() {
		start_output();
		connect_to_ndi_source();
	});
}

SyncTestDock::~SyncTestDock()
{
	disconnect_from_ndi_source();

	if (sync_test) {
		obs_output_stop(sync_test);
		sync_test = nullptr;
	}
}

extern "C" QWidget *create_sync_test_dock()
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	return static_cast<QWidget *>(new SyncTestDock(main_window));
}

#define CD_TO_LOCAL(type, name, get_func) \
	type name;                        \
	if (!get_func(cd, #name, &name))  \
		return;

void SyncTestDock::cb_video_marker_found(void *param, calldata_t *cd)
{
	auto *dock = (SyncTestDock *)param;

	CD_TO_LOCAL(video_marker_found_s *, data, calldata_get_ptr);
	video_marker_found_s found = *data;

	QMetaObject::invokeMethod(dock, [dock, found]() { dock->on_video_marker_found(found); });
};

void SyncTestDock::cb_audio_marker_found(void *param, calldata_t *cd)
{
	auto *dock = (SyncTestDock *)param;

	CD_TO_LOCAL(audio_marker_found_s *, data, calldata_get_ptr);
	audio_marker_found_s found = *data;

	QMetaObject::invokeMethod(dock, [dock, found]() { dock->on_audio_marker_found(found); });
};

void SyncTestDock::cb_sync_found(void *param, calldata_t *cd)
{
	auto *dock = (SyncTestDock *)param;

	CD_TO_LOCAL(sync_index *, data, calldata_get_ptr);
	sync_index found = *data;

	QMetaObject::invokeMethod(dock, [dock, found]() { dock->on_sync_found(found); });
}

void SyncTestDock::cb_frame_drop_detected(void *param, calldata_t *cd)
{
	auto *dock = (SyncTestDock *)param;

	CD_TO_LOCAL(frame_drop_event_s *, data, calldata_get_ptr);
	frame_drop_event_s found = *data;

	QMetaObject::invokeMethod(dock, [dock, found]() { dock->on_frame_drop_detected(found); });
}

void SyncTestDock::cb_ndi_timing(void *param, calldata_t *cd)
{
	auto *dock = (SyncTestDock *)param;

	CD_TO_LOCAL(ndi_timing_info_t *, data, calldata_get_ptr);
	ndi_timing_info_t timing = *data;

	QMetaObject::invokeMethod(dock, [dock, timing]() { dock->on_ndi_timing(timing); });
}

void SyncTestDock::start_output()
{
	OBSOutputAutoRelease o = obs_output_create(SYNC_TEST_OUTPUT_ID, "sync-test-output", nullptr, nullptr);
	if (!o) {
		blog(LOG_ERROR, "[distroav] Failed to create sync-test-output.");
		return;
	}

	last_video_ix = last_audio_ix = -1;
	total_frame_drops = 0;
	total_frames_seen = 0;
	last_log_ts = 0;
	sync_count_since_log = 0;
	latency_sum_since_log = 0.0;
	last_latency_ms = 0.0;

	auto *sh = obs_output_get_signal_handler(o);
	signal_handler_connect(sh, "video_marker_found", cb_video_marker_found, this);
	signal_handler_connect(sh, "audio_marker_found", cb_audio_marker_found, this);
	signal_handler_connect(sh, "sync_found", cb_sync_found, this);
	signal_handler_connect(sh, "frame_drop_detected", cb_frame_drop_detected, this);

	bool success = obs_output_start(o);

	if (!success)
		blog(LOG_WARNING, "[distroav] sync-test-output failed to start");

	sync_test = o;
}

void SyncTestDock::on_reset()
{
	if (sync_test) {
		obs_output_stop(sync_test);
		sync_test = nullptr;
	}

	latencyDisplay->setText("-");
	frameDropDisplay->setText("-");
	createdDisplay->setText("-");
	receivedDisplay->setText("-");
	diffDisplay->setText("-");

	last_ndi_timecode_ns = 0;
	last_receive_time_ns = 0;
	last_diff_ns = 0;

	disconnect_from_ndi_source();
	start_output();
	connect_to_ndi_source();
}

void SyncTestDock::on_video_marker_found(struct video_marker_found_s data)
{
	last_video_ix = data.qr_data.index;
	total_frames_seen++;

	if (total_frame_drops == 0 && total_frames_seen > 0)
		frameDropDisplay->setText(QStringLiteral("0 (0.0%)"));
}

void SyncTestDock::on_audio_marker_found(struct audio_marker_found_s data)
{
	last_audio_ix = data.index;
}

void SyncTestDock::on_sync_found(sync_index data)
{
	int64_t ts = (int64_t)data.audio_ts - (int64_t)data.video_ts;
	double latency_ms = ts * 1e-6;
	last_latency_ms = latency_ms;
	latencyDisplay->setText(QStringLiteral("%1 ms").arg(latency_ms, 0, 'f', 1));

	sync_count_since_log++;
	latency_sum_since_log += latency_ms;

	// Don't log here - let on_ndi_timing handle consolidated logging
	// to avoid duplicate logs with different time bases
}

void SyncTestDock::on_frame_drop_detected(frame_drop_event_s data)
{
	total_frame_drops = (int64_t)data.total_dropped;
	total_frames_seen = (int64_t)data.total_received;
	double drop_rate = 0.0;
	int64_t total = total_frames_seen + total_frame_drops;
	if (total > 0)
		drop_rate = (double)total_frame_drops * 100.0 / (double)total;
	frameDropDisplay->setText(QStringLiteral("%1 (%2%)").arg(total_frame_drops).arg(drop_rate, 0, 'f', 1));
}

void SyncTestDock::on_ndi_timing(ndi_timing_info_t timing)
{
	// Created time: NDI timecode (capture time on sender)
	createdDisplay->setText(format_time_ns(timing.ndi_timecode_ns));

	// Received time: wall clock when we received the frame
	// This is ndi_timecode + pipeline_latency (since pipeline_latency = wall_now - ndi_timecode)
	int64_t receive_time_ns = timing.ndi_timecode_ns + timing.pipeline_latency_ns;
	receivedDisplay->setText(format_time_ns(receive_time_ns));

	// Diff: network latency (pipeline_latency)
	int64_t diff_ns = timing.pipeline_latency_ns;
	int64_t diff_ms = diff_ns / 1000000;
	int64_t diff_us = diff_ns / 1000;
	diffDisplay->setText(QStringLiteral("%1ms (%2us)").arg(diff_ms).arg(diff_us));

	// Cache for logging
	last_ndi_timecode_ns = timing.ndi_timecode_ns;
	last_receive_time_ns = receive_time_ns;
	last_diff_ns = diff_ns;

	// Log consolidated status every second
	log_consolidated_status(timing.obs_now_ns);
}

void SyncTestDock::log_consolidated_status(uint64_t now_ts)
{
	// Only log once per second using OBS monotonic clock
	if (last_log_ts == 0)
		last_log_ts = now_ts;

	// Ensure we have valid timing data and enough time has passed
	if (now_ts < last_log_ts || now_ts - last_log_ts < 1000000000ULL)
		return;

	// Calculate average latency
	double avg_latency = sync_count_since_log > 0
		? latency_sum_since_log / sync_count_since_log
		: last_latency_ms;

	// Calculate drop rate
	int64_t total = total_frames_seen + total_frame_drops;
	double drop_rate = total > 0 ? (double)total_frame_drops * 100.0 / (double)total : 0.0;

	// Format times for logging
	int64_t diff_ms = last_diff_ns / 1000000;
	int64_t diff_us = last_diff_ns / 1000;

	// Format created/received as time-of-day HH:MM:SS.mmm
	int64_t created_tod = (last_ndi_timecode_ns / 1000000) % 86400000LL;
	int64_t received_tod = (last_receive_time_ns / 1000000) % 86400000LL;

	int c_h = (int)(created_tod / 3600000);
	int c_m = (int)((created_tod % 3600000) / 60000);
	int c_s = (int)((created_tod % 60000) / 1000);
	int c_ms = (int)(created_tod % 1000);

	int r_h = (int)(received_tod / 3600000);
	int r_m = (int)((received_tod % 3600000) / 60000);
	int r_s = (int)((received_tod % 60000) / 1000);
	int r_ms = (int)(received_tod % 1000);

	blog(LOG_INFO, "[distroav] SYNC: av=%.1fms drops=%" PRId64 "/%" PRId64 "(%.1f%%) "
		"created=%02d:%02d:%02d.%03d received=%02d:%02d:%02d.%03d diff=%" PRId64 "ms(%" PRId64 "us)",
		avg_latency,
		total_frame_drops, total,
		drop_rate,
		c_h, c_m, c_s, c_ms,
		r_h, r_m, r_s, r_ms,
		diff_ms, diff_us);

	// Reset counters
	sync_count_since_log = 0;
	latency_sum_since_log = 0.0;
	last_log_ts = now_ts;
}

void SyncTestDock::connect_to_ndi_source()
{
	// Already connected?
	if (ndi_source_ref)
		return;

	// Find the first NDI source and connect to its ndi_timing signal
	obs_enum_sources([](void *param, obs_source_t *source) {
		auto *dock = (SyncTestDock *)param;

		const char *source_id = obs_source_get_id(source);
		if (source_id && strcmp(source_id, "ndi_source") == 0) {
			// Found an NDI source, connect to its signal
			auto *sh = obs_source_get_signal_handler(source);
			signal_handler_connect(sh, "ndi_timing", cb_ndi_timing, dock);

			// Store weak reference for later disconnection
			dock->ndi_source_ref = obs_source_get_weak_source(source);

			blog(LOG_INFO, "[distroav] SyncDock: Connected to NDI source '%s' for timing signals",
			     obs_source_get_name(source));
			return false; // Stop enumeration
		}
		return true; // Continue enumeration
	}, this);

	// If no NDI source found, retry after a delay (sources may not be loaded yet)
	if (!ndi_source_ref) {
		blog(LOG_DEBUG, "[distroav] SyncDock: No NDI source found, will retry in 2 seconds");
		QTimer::singleShot(2000, this, [this]() { connect_to_ndi_source(); });
	}
}

void SyncTestDock::disconnect_from_ndi_source()
{
	if (!ndi_source_ref)
		return;

	OBSSourceAutoRelease source = obs_weak_source_get_source(ndi_source_ref);
	if (source) {
		auto *sh = obs_source_get_signal_handler(source);
		signal_handler_disconnect(sh, "ndi_timing", cb_ndi_timing, this);
		blog(LOG_DEBUG, "[distroav] SyncDock: Disconnected from NDI source timing signals");
	}

	ndi_source_ref = nullptr;
}
