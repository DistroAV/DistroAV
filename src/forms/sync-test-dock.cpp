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
	int64_t total_ms = ns / 1000000;
	int64_t tod_ms = total_ms % 86400000LL;
	if (tod_ms < 0) tod_ms += 86400000LL;

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
	mainLayout->setSpacing(4);
	mainLayout->setContentsMargins(8, 8, 8, 8);

	// Reset button
	resetButton = new QPushButton(obs_module_text("NDIPlugin.SyncDock.Reset"), this);
	mainLayout->addWidget(resetButton);
	connect(resetButton, &QPushButton::clicked, this, &SyncTestDock::on_reset);

	// Top metrics grid
	QGridLayout *metricsLayout = new QGridLayout();
	metricsLayout->setSpacing(4);

	QLabel *label;

	// A/V Sync Latency
	label = new QLabel(obs_module_text("NDIPlugin.SyncDock.AVSync"), this);
	label->setProperty("class", "text-large");
	metricsLayout->addWidget(label, 0, 0);

	latencyDisplay = new QLabel("-", this);
	latencyDisplay->setProperty("class", "text-large");
	metricsLayout->addWidget(latencyDisplay, 0, 1);

	// Frame Drops
	label = new QLabel(obs_module_text("NDIPlugin.SyncDock.Drops"), this);
	metricsLayout->addWidget(label, 1, 0);

	frameDropDisplay = new QLabel("-", this);
	metricsLayout->addWidget(frameDropDisplay, 1, 1);

	mainLayout->addLayout(metricsLayout);

	// Separator
	QFrame *line1 = new QFrame(this);
	line1->setFrameShape(QFrame::HLine);
	line1->setFrameShadow(QFrame::Sunken);
	mainLayout->addWidget(line1);

	// Pipeline layout
	QGridLayout *pipelineLayout = new QGridLayout();
	pipelineLayout->setSpacing(2);
	pipelineLayout->setColumnStretch(1, 1);

	int row = 0;

	// NDI Section Header
	label = new QLabel("── NDI ──", this);
	label->setStyleSheet("color: #888;");
	pipelineLayout->addWidget(label, row++, 0, 1, 2, Qt::AlignCenter);

	// Creation (NDI)
	label = new QLabel("Creation", this);
	pipelineLayout->addWidget(label, row, 0);
	creationTimeDisplay = new QLabel("-", this);
	pipelineLayout->addWidget(creationTimeDisplay, row++, 1, Qt::AlignRight);

	// Arrow + Network delay
	networkDelayDisplay = new QLabel("     |", this);
	networkDelayDisplay->setAlignment(Qt::AlignCenter);
	pipelineLayout->addWidget(networkDelayDisplay, row++, 0, 1, 2);

	// Receive (NDI)
	label = new QLabel("Receive", this);
	pipelineLayout->addWidget(label, row, 0);
	receiveTimeDisplay = new QLabel("-", this);
	pipelineLayout->addWidget(receiveTimeDisplay, row++, 1, Qt::AlignRight);

	// Arrow + Buffer delay
	bufferDelayDisplay = new QLabel("     |", this);
	bufferDelayDisplay->setAlignment(Qt::AlignCenter);
	pipelineLayout->addWidget(bufferDelayDisplay, row++, 0, 1, 2);

	// Present (NDI)
	label = new QLabel("Present", this);
	pipelineLayout->addWidget(label, row, 0);
	presentTimeDisplay = new QLabel("-", this);
	pipelineLayout->addWidget(presentTimeDisplay, row++, 1, Qt::AlignRight);

	// OBS Section Header
	label = new QLabel("── OBS ──", this);
	label->setStyleSheet("color: #888;");
	pipelineLayout->addWidget(label, row++, 0, 1, 2, Qt::AlignCenter);

	// Arrow + Render delay
	renderDelayDisplay = new QLabel("     |", this);
	renderDelayDisplay->setAlignment(Qt::AlignCenter);
	pipelineLayout->addWidget(renderDelayDisplay, row++, 0, 1, 2);

	// Render (OBS)
	label = new QLabel("Render", this);
	pipelineLayout->addWidget(label, row, 0);
	renderTimeDisplay = new QLabel("-", this);
	pipelineLayout->addWidget(renderTimeDisplay, row++, 1, Qt::AlignRight);

	mainLayout->addLayout(pipelineLayout);

	// Separator
	QFrame *line2 = new QFrame(this);
	line2->setFrameShape(QFrame::HLine);
	line2->setFrameShadow(QFrame::Sunken);
	mainLayout->addWidget(line2);

	// Total delay
	QHBoxLayout *totalLayout = new QHBoxLayout();
	label = new QLabel("Total:", this);
	label->setProperty("class", "text-large");
	totalLayout->addWidget(label);
	totalDelayDisplay = new QLabel("-", this);
	totalDelayDisplay->setProperty("class", "text-large");
	totalLayout->addWidget(totalDelayDisplay);
	totalLayout->addStretch();
	mainLayout->addLayout(totalLayout);

	mainLayout->addStretch();
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

void SyncTestDock::cb_render_timing(void *param, calldata_t *cd)
{
	auto *dock = (SyncTestDock *)param;

	int64_t rendered_ns;
	if (!calldata_get_int(cd, "rendered_ns", &rendered_ns))
		return;

	QMetaObject::invokeMethod(dock, [dock, rendered_ns]() { dock->on_render_timing(rendered_ns); });
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
	signal_handler_connect(sh, "render_timing", cb_render_timing, this);

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
	creationTimeDisplay->setText("-");
	networkDelayDisplay->setText("     |");
	receiveTimeDisplay->setText("-");
	bufferDelayDisplay->setText("     |");
	presentTimeDisplay->setText("-");
	renderDelayDisplay->setText("     |");
	renderTimeDisplay->setText("-");
	totalDelayDisplay->setText("-");

	last_creation_ns = 0;
	last_network_ns = 0;
	last_buffer_ns = 0;
	last_present_ns = 0;
	last_render_delay_ns = 0;
	last_presentation_obs_ns = 0;
	pending_frames.clear();

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
	// Pipeline timing values
	int64_t creation_ns = timing.ndi_timecode_ns;
	int64_t network_ns = timing.pipeline_latency_ns;
	int64_t receive_ns = creation_ns + network_ns;

	// Buffer time: ts_ahead_ns tells us how far ahead the frame is scheduled
	// Positive = frame is buffered, Negative = frame is late/no buffer
	int64_t buffer_ns = timing.ts_ahead_ns;
	int64_t present_ns = receive_ns + buffer_ns;

	// Update displays
	creationTimeDisplay->setText(format_time_ns(creation_ns));

	int64_t network_ms = network_ns / 1000000;
	networkDelayDisplay->setText(QStringLiteral("     | %1ms").arg(network_ms));

	receiveTimeDisplay->setText(format_time_ns(receive_ns));

	int64_t buffer_ms = buffer_ns / 1000000;
	bufferDelayDisplay->setText(QStringLiteral("     | %1ms").arg(buffer_ms));

	presentTimeDisplay->setText(format_time_ns(present_ns));

	// Push frame timing to FIFO queue for exact order matching
	PendingFrameTiming pft;
	pft.frame_number = timing.frame_number;
	pft.creation_ns = creation_ns;
	pft.present_ns = present_ns;
	pft.network_ns = network_ns;
	pft.buffer_ns = buffer_ns;
	pending_frames.push_back(pft);

	// Keep queue bounded (max 120 frames = ~2 seconds at 60fps)
	while (pending_frames.size() > 120) {
		pending_frames.pop_front();
	}

	// Cache for logging and render timing calculation
	last_creation_ns = creation_ns;
	last_network_ns = network_ns;
	last_buffer_ns = buffer_ns;
	last_present_ns = present_ns;
	last_presentation_obs_ns = timing.presentation_ns;

	// Log consolidated status every second
	log_consolidated_status(timing.obs_now_ns);
}

void SyncTestDock::on_render_timing(int64_t rendered_ns)
{
	// Pop matching frame from FIFO queue (exact 1:1 order match)
	// NDI frames flow through OBS in order, so first in = first out
	if (pending_frames.empty())
		return;

	const PendingFrameTiming pft = pending_frames.front();
	pending_frames.pop_front();

	// Frames are rendered on schedule, so render delay is 0
	// (The scheduling is done by OBS based on presentation_ns)
	last_render_delay_ns = 0;
	renderDelayDisplay->setText(QStringLiteral("     | 0ms"));

	// Render time = present time (rendered on schedule)
	renderTimeDisplay->setText(format_time_ns(pft.present_ns));

	// Total delay = network + buffer (render is 0)
	int64_t total_ns = pft.network_ns + pft.buffer_ns;
	int64_t total_ms = total_ns / 1000000;
	totalDelayDisplay->setText(QStringLiteral("%1 ms").arg(total_ms));
}

void SyncTestDock::log_consolidated_status(uint64_t now_ts)
{
	// Only log once per second using OBS monotonic clock
	if (last_log_ts == 0)
		last_log_ts = now_ts;

	if (now_ts < last_log_ts || now_ts - last_log_ts < 1000000000ULL)
		return;

	// Calculate average latency
	double avg_latency = sync_count_since_log > 0
		? latency_sum_since_log / sync_count_since_log
		: last_latency_ms;

	// Calculate drop rate
	int64_t total = total_frames_seen + total_frame_drops;
	double drop_rate = total > 0 ? (double)total_frame_drops * 100.0 / (double)total : 0.0;

	// Calculate times
	int64_t network_ms = last_network_ns / 1000000;
	int64_t buffer_ms = last_buffer_ns / 1000000;
	int64_t render_ms = last_render_delay_ns / 1000000;
	int64_t total_delay_ms = network_ms + buffer_ms + render_ms;

	// Format creation time as time-of-day
	int64_t creation_tod = (last_creation_ns / 1000000) % 86400000LL;
	int c_h = (int)(creation_tod / 3600000);
	int c_m = (int)((creation_tod % 3600000) / 60000);
	int c_s = (int)((creation_tod % 60000) / 1000);
	int c_ms = (int)(creation_tod % 1000);

	blog(LOG_INFO, "[distroav] SYNC: av=%.1fms drops=%" PRId64 "/%" PRId64 "(%.1f%%) "
		"creation=%02d:%02d:%02d.%03d network=%" PRId64 "ms buffer=%" PRId64 "ms render=%" PRId64 "ms total=%" PRId64 "ms",
		avg_latency,
		total_frame_drops, total,
		drop_rate,
		c_h, c_m, c_s, c_ms,
		network_ms, buffer_ms, render_ms, total_delay_ms);

	// Reset counters
	sync_count_since_log = 0;
	latency_sum_since_log = 0.0;
	last_log_ts = now_ts;
}

void SyncTestDock::connect_to_ndi_source()
{
	if (ndi_source_ref)
		return;

	obs_enum_sources([](void *param, obs_source_t *source) {
		auto *dock = (SyncTestDock *)param;

		const char *source_id = obs_source_get_id(source);
		if (source_id && strcmp(source_id, "ndi_source") == 0) {
			auto *sh = obs_source_get_signal_handler(source);
			signal_handler_connect(sh, "ndi_timing", cb_ndi_timing, dock);
			dock->ndi_source_ref = obs_source_get_weak_source(source);

			blog(LOG_INFO, "[distroav] SyncDock: Connected to NDI source '%s' for timing signals",
			     obs_source_get_name(source));
			return false;
		}
		return true;
	}, this);

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
