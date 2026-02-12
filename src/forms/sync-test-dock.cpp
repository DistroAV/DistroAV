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
#include <util/platform.h>
#include <inttypes.h>
#include <string>
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

	// NDI Source selector
	QHBoxLayout *sourceLayout = new QHBoxLayout();
	QLabel *sourceLabel = new QLabel(obs_module_text("NDIPlugin.SyncDock.Source"), this);
	sourceLayout->addWidget(sourceLabel);

	ndiSourceCombo = new QComboBox(this);
	ndiSourceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	ndiSourceCombo->setPlaceholderText(obs_module_text("NDIPlugin.SyncDock.NoSources"));
	sourceLayout->addWidget(ndiSourceCombo);
	mainLayout->addLayout(sourceLayout);

	connect(ndiSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &SyncTestDock::onSourceSelectionChanged);

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

	// A/V Index display
	label = new QLabel("Index:", this);
	metricsLayout->addWidget(label, 2, 0);

	indexDisplay = new QLabel("A(-) V(-)", this);
	metricsLayout->addWidget(indexDisplay, 2, 1);

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

	// Arrow + Release delay (receive → release)
	releaseDelayDisplay = new QLabel("     |", this);
	releaseDelayDisplay->setAlignment(Qt::AlignCenter);
	pipelineLayout->addWidget(releaseDelayDisplay, row++, 0, 1, 2);

	// Release (frame handed to OBS)
	label = new QLabel("Release", this);
	pipelineLayout->addWidget(label, row, 0);
	releaseTimeDisplay = new QLabel("-", this);
	pipelineLayout->addWidget(releaseTimeDisplay, row++, 1, Qt::AlignRight);

	// Arrow + Buffer wait delay (release → present)
	bufferWaitDisplay = new QLabel("     |", this);
	bufferWaitDisplay->setAlignment(Qt::AlignCenter);
	pipelineLayout->addWidget(bufferWaitDisplay, row++, 0, 1, 2);

	// OBS Section Header
	label = new QLabel("── OBS ──", this);
	label->setStyleSheet("color: #888;");
	pipelineLayout->addWidget(label, row++, 0, 1, 2, Qt::AlignCenter);

	// Present (scheduled render time)
	label = new QLabel("Present", this);
	pipelineLayout->addWidget(label, row, 0);
	presentTimeDisplay = new QLabel("-", this);
	pipelineLayout->addWidget(presentTimeDisplay, row++, 1, Qt::AlignRight);

	// Arrow + Render delay (present → render)
	renderDelayDisplay = new QLabel("     |", this);
	renderDelayDisplay->setAlignment(Qt::AlignCenter);
	pipelineLayout->addWidget(renderDelayDisplay, row++, 0, 1, 2);

	// Render (actual output time)
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

	// Subscribe to OBS global signals for source list refresh
	signal_handler_t *core_sh = obs_get_signal_handler();
	signal_handler_connect(core_sh, "source_create", cb_source_created, this);
	signal_handler_connect(core_sh, "source_destroy", cb_source_destroyed, this);
	signal_handler_connect(core_sh, "source_rename", cb_source_renamed, this);

	QTimer::singleShot(0, this, [this]() {
		start_output();
		populateNdiSourceList();
		// Connection happens via onSourceSelectionChanged when combo box gets first item
	});
}

SyncTestDock::~SyncTestDock()
{
	disconnect_from_ndi_source();

	// Disconnect from OBS global signals
	signal_handler_t *obs_sh = obs_get_signal_handler();
	if (obs_sh) {
		signal_handler_disconnect(obs_sh, "frame_output", cb_obs_frame_output, this);
		signal_handler_disconnect(obs_sh, "source_create", cb_source_created, this);
		signal_handler_disconnect(obs_sh, "source_destroy", cb_source_destroyed, this);
		signal_handler_disconnect(obs_sh, "source_rename", cb_source_renamed, this);
	}

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

	int64_t frame_ts;
	int64_t rendered_ns;
	if (!calldata_get_int(cd, "frame_ts", &frame_ts))
		return;
	if (!calldata_get_int(cd, "rendered_ns", &rendered_ns))
		return;

	QMetaObject::invokeMethod(dock, [dock, frame_ts, rendered_ns]() { dock->on_render_timing(frame_ts, rendered_ns); });
}

void SyncTestDock::cb_obs_frame_output(void *param, calldata_t *cd)
{
	auto *dock = (SyncTestDock *)param;

	int64_t render_wall_clock_ns;
	int64_t source_frame_ts;
	if (!calldata_get_int(cd, "output_wall_clock_ns", &render_wall_clock_ns))
		return;
	if (!calldata_get_int(cd, "source_frame_ts", &source_frame_ts))
		return;

	QMetaObject::invokeMethod(dock, [dock, render_wall_clock_ns, source_frame_ts]() {
		dock->on_obs_frame_output(render_wall_clock_ns, source_frame_ts);
	});
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
	// Disabled: render_timing uses current frame timestamp, not delayed timestamp
	// signal_handler_connect(sh, "render_timing", cb_render_timing, this);

	// Connect to OBS global frame_output signal for true render timing
	// This receives source_frame_ts from frame identity tracking, which
	// properly tracks through GPU delay filter
	signal_handler_t *obs_sh = obs_get_signal_handler();
	signal_handler_connect(obs_sh, "frame_output", cb_obs_frame_output, this);

	bool success = obs_output_start(o);

	if (!success)
		blog(LOG_WARNING, "[distroav] sync-test-output failed to start");

	sync_test = o;
}

void SyncTestDock::on_reset()
{
	total_frame_drops = 0;
	total_frames_seen = 0;
	frameDropDisplay->setText(QStringLiteral("0 (0.0%)"));
}

void SyncTestDock::on_video_marker_found(struct video_marker_found_s data)
{
	last_video_ix = data.qr_data.index;
	total_frames_seen++;

	if (total_frame_drops == 0 && total_frames_seen > 0)
		frameDropDisplay->setText(QStringLiteral("0 (0.0%)"));

	// Update index display
	indexDisplay->setText(QStringLiteral("A(%1) V(%2)").arg(last_audio_ix).arg(last_video_ix));
}

void SyncTestDock::on_audio_marker_found(struct audio_marker_found_s data)
{
	last_audio_ix = data.index;

	// Update index display
	indexDisplay->setText(QStringLiteral("A(%1) V(%2)").arg(last_audio_ix).arg(last_video_ix));
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

	// Release time: wall clock time when frame was handed to OBS
	int64_t release_ns = timing.release_wall_clock_ns;
	int64_t release_delay_ns = release_ns - receive_ns;

	// Present time: when OBS is scheduled to render this frame (wall clock)
	// presentation_ns is in OBS monotonic time, convert to wall clock
	int64_t present_wall_clock_ns = timing.presentation_ns + timing.clock_offset_ns;

	// Update NDI-side timestamps immediately (always visible, even when not in program)
	creationTimeDisplay->setText(format_time_ns(creation_ns));
	receiveTimeDisplay->setText(format_time_ns(receive_ns));
	releaseTimeDisplay->setText(format_time_ns(release_ns));
	presentTimeDisplay->setText(format_time_ns(present_wall_clock_ns));

	// Throttle ms delay displays to once per second (timestamps still update every frame)
	uint64_t now_ns = os_gettime_ns();
	if (now_ns - last_ms_display_update_ns >= 1000000000ULL) {
		last_ms_display_update_ns = now_ns;

		// Update delay displays
		int64_t network_ms = network_ns / 1000000;
		networkDelayDisplay->setText(QStringLiteral("     | %1ms").arg(network_ms));

		int64_t release_ms = release_delay_ns / 1000000;
		releaseDelayDisplay->setText(QStringLiteral("     | %1ms").arg(release_ms));

		// Buffer wait estimate (release → present) - actual value refined in on_obs_frame_output
		int64_t buffer_wait_ns = present_wall_clock_ns - release_ns;
		int64_t buffer_wait_ms = buffer_wait_ns / 1000000;
		bufferWaitDisplay->setText(QStringLiteral("     | %1ms (buffer)").arg(buffer_wait_ms));
	}

	// Push frame timing to FIFO queue for exact order matching
	PendingFrameTiming pft;
	pft.frame_number = timing.frame_number;
	pft.creation_ns = creation_ns;
	pft.presentation_obs_ns = timing.presentation_ns;
	pft.network_ns = network_ns;
	pft.release_ns = release_ns;
	pft.release_delay_ns = release_delay_ns;
	pft.present_wall_clock_ns = present_wall_clock_ns;
	pft.clock_offset_ns = timing.clock_offset_ns;
	pending_frames.push_back(pft);

	// Keep queue bounded (max 120 frames = ~2 seconds at 60fps)
	while (pending_frames.size() > 120) {
		pending_frames.pop_front();
	}

	// Cache for logging and render timing calculation
	last_creation_ns = creation_ns;
	last_receive_ns = receive_ns;
	last_network_ns = network_ns;
	last_release_ns = release_ns;
	last_release_delay_ns = release_delay_ns;
	last_presentation_obs_ns = timing.presentation_ns;

	// Log consolidated status every second
	log_consolidated_status(timing.obs_now_ns);
}

void SyncTestDock::on_render_timing(int64_t frame_ts, int64_t rendered_ns)
{
	// NOTE: This function is DISABLED (see start_output line 316)
	// It's kept for potential future use but uses frame_ts which doesn't
	// track through GPU delay filters correctly.
	// Use on_obs_frame_output with source frame identity tracking instead.
	UNUSED_PARAMETER(frame_ts);
	UNUSED_PARAMETER(rendered_ns);
}

void SyncTestDock::on_obs_frame_output(int64_t render_wall_clock_ns, int64_t source_frame_ts)
{
	// This callback receives the TRUE render completion time from OBS core
	// render_wall_clock_ns: wall-clock time when frame was actually output
	// source_frame_ts: original frame timestamp from the source (tracked through pipeline)
	//
	// Frame identity tagging allows us to correlate this output timestamp with
	// the ACTUAL source frame that was rendered, even after going through
	// filters like Render Delay that reorder/delay frames.

	// Find matching frame in FIFO queue by source frame timestamp
	if (pending_frames.empty())
		return;

	// Find closest match (allow tolerance for timestamp precision)
	// With GPU delay filter disabled on render_timing, frames should
	// accumulate in queue and match correctly with delayed source_frame_ts
	const int64_t tolerance_ns = 100000000LL; // 100ms tolerance (3 frames at 30fps)
	PendingFrameTiming pft;
	auto best_it = pending_frames.end();
	int64_t best_diff = 1000000000000LL; // Large initial value

	for (auto it = pending_frames.begin(); it != pending_frames.end(); ++it) {
		int64_t diff = it->presentation_obs_ns - source_frame_ts;
		if (diff < 0) diff = -diff;
		if (diff < best_diff && diff <= tolerance_ns) {
			best_diff = diff;
			best_it = it;
		}
	}

	if (best_it == pending_frames.end()) {
		// No match found within tolerance
		if (!pending_frames.empty()) {
			int64_t first_ts = pending_frames.front().presentation_obs_ns;
			blog(LOG_DEBUG, "[distroav] OBS_RENDER_MATCH_FAIL: source_frame_ts=%" PRId64 " first_pending=%" PRId64 " diff=%" PRId64 " queue_size=%zu",
				source_frame_ts, first_ts, first_ts - source_frame_ts, pending_frames.size());
		}
		return;
	}

	pft = *best_it;
	pending_frames.erase(best_it);

	// Convert OBS monotonic render time to wall-clock using stored offset
	int64_t rendered_wall_clock_ns = render_wall_clock_ns + pft.clock_offset_ns;

	// Cache render wall-clock for consolidated logging
	last_render_wall_clock_ns = rendered_wall_clock_ns;

	// Buffer wait = time OBS holds frame (release → present)
	int64_t buffer_wait_ns = pft.present_wall_clock_ns - pft.release_ns;
	last_buffer_wait_ns = buffer_wait_ns;
	last_present_ns = pft.present_wall_clock_ns;

	// Render delay = GPU processing time (present → render)
	int64_t render_delay_ns = rendered_wall_clock_ns - pft.present_wall_clock_ns;
	last_render_delay_ns = render_delay_ns;

	// Display actual wall-clock render time (timestamp - updates every frame)
	renderTimeDisplay->setText(format_time_ns(rendered_wall_clock_ns));

	// Update timestamps from the MATCHED frame so they're consistent (timestamps - update every frame)
	creationTimeDisplay->setText(format_time_ns(pft.creation_ns));
	receiveTimeDisplay->setText(format_time_ns(pft.creation_ns + pft.network_ns));
	releaseTimeDisplay->setText(format_time_ns(pft.release_ns));
	presentTimeDisplay->setText(format_time_ns(pft.present_wall_clock_ns));

	// Throttle ms delay displays to once per second (timestamps still update every frame)
	uint64_t now_ns = os_gettime_ns();
	if (now_ns - last_ms_display_update_ns >= 1000000000ULL) {
		last_ms_display_update_ns = now_ns;

		// Update buffer wait display (release → present)
		int64_t buffer_wait_ms = buffer_wait_ns / 1000000;
		bufferWaitDisplay->setText(QStringLiteral("     | %1ms (buffer)").arg(buffer_wait_ms));

		// Update render delay display (present → render)
		int64_t render_delay_ms = render_delay_ns / 1000000;
		renderDelayDisplay->setText(QStringLiteral("     | %1ms (GPU)").arg(render_delay_ms));

		// Update network and release delays too (may have changed)
		int64_t network_ms = pft.network_ns / 1000000;
		networkDelayDisplay->setText(QStringLiteral("     | %1ms").arg(network_ms));

		int64_t release_ms = pft.release_delay_ns / 1000000;
		releaseDelayDisplay->setText(QStringLiteral("     | %1ms").arg(release_ms));

		// Total delay = network + release + buffer wait + render
		int64_t total_ns = pft.network_ns + pft.release_delay_ns + buffer_wait_ns + render_delay_ns;
		int64_t total_ms = total_ns / 1000000;
		totalDelayDisplay->setText(QStringLiteral("%1 ms").arg(total_ms));
	}
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
	int64_t release_ms = last_release_delay_ns / 1000000;
	int64_t buffer_ms = last_buffer_wait_ns / 1000000;
	int64_t render_ms = last_render_delay_ns / 1000000;
	int64_t total_delay_ms = network_ms + release_ms + buffer_ms + render_ms;

	// Format creation time as HH:MM:SS.mmm
	auto format_tod = [](int64_t ns) -> std::string {
		int64_t tod_ms = (ns / 1000000) % 86400000LL;
		if (tod_ms < 0) tod_ms += 86400000LL;
		int h = (int)(tod_ms / 3600000);
		int m = (int)((tod_ms % 3600000) / 60000);
		int s = (int)((tod_ms % 60000) / 1000);
		int ms = (int)(tod_ms % 1000);
		char buf[16];
		snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d", h, m, s, ms);
		return buf;
	};

	std::string creation_str = format_tod(last_creation_ns);
	std::string receive_str = format_tod(last_receive_ns);
	std::string release_str = format_tod(last_release_ns);
	std::string present_str = format_tod(last_present_ns);
	std::string render_str = format_tod(last_render_wall_clock_ns);

	blog(LOG_INFO, "[distroav] SYNC: av=%.1fms drops=%" PRId64 "(%.1f%%) "
		"creation=%s +%" PRId64 "ms(net) receive=%s +%" PRId64 "ms(rel) release=%s +%" PRId64 "ms(buf) present=%s +%" PRId64 "ms(gpu) rendered=%s total=%" PRId64 "ms",
		avg_latency,
		total_frame_drops, drop_rate,
		creation_str.c_str(), network_ms,
		receive_str.c_str(), release_ms,
		release_str.c_str(), buffer_ms,
		present_str.c_str(), render_ms,
		render_str.c_str(), total_delay_ms);

	// Reset counters
	sync_count_since_log = 0;
	latency_sum_since_log = 0.0;
	last_log_ts = now_ts;
}

void SyncTestDock::populateNdiSourceList()
{
	// Block signals to avoid triggering onSourceSelectionChanged during population
	ndiSourceCombo->blockSignals(true);

	// Remember current selection
	QString currentSelection = selectedSourceName;
	int newSelectionIndex = -1;

	ndiSourceCombo->clear();

	// Collect all NDI sources
	struct EnumData {
		QStringList names;
	} enumData;

	obs_enum_sources([](void *param, obs_source_t *source) {
		auto *data = (EnumData *)param;
		const char *source_id = obs_source_get_id(source);
		if (source_id && strcmp(source_id, "ndi_source") == 0) {
			const char *name = obs_source_get_name(source);
			if (name)
				data->names.append(QString::fromUtf8(name));
		}
		return true;
	}, &enumData);

	// Sort alphabetically
	enumData.names.sort(Qt::CaseInsensitive);

	// Add to combo box
	for (int i = 0; i < enumData.names.size(); i++) {
		ndiSourceCombo->addItem(enumData.names[i]);
		if (enumData.names[i] == currentSelection)
			newSelectionIndex = i;
	}

	ndiSourceCombo->blockSignals(false);

	// Restore selection or select first if previous selection is gone
	if (ndiSourceCombo->count() > 0) {
		if (newSelectionIndex >= 0) {
			ndiSourceCombo->setCurrentIndex(newSelectionIndex);
		} else {
			// Previous selection gone, select first item
			ndiSourceCombo->setCurrentIndex(0);
			onSourceSelectionChanged(0);
		}
	} else {
		// No sources available
		selectedSourceName.clear();
		disconnect_from_ndi_source();
	}
}

void SyncTestDock::onSourceSelectionChanged(int index)
{
	if (index < 0) {
		selectedSourceName.clear();
		disconnect_from_ndi_source();
		return;
	}

	QString newSourceName = ndiSourceCombo->itemText(index);
	if (newSourceName == selectedSourceName && ndi_source_ref)
		return;  // Already connected to this source

	selectedSourceName = newSourceName;

	// Clear pending frames queue when switching sources
	pending_frames.clear();

	// Disconnect from current source
	disconnect_from_ndi_source();

	// Connect to newly selected source
	connect_to_ndi_source();
}

void SyncTestDock::connect_to_ndi_source()
{
	if (ndi_source_ref)
		return;

	if (selectedSourceName.isEmpty()) {
		blog(LOG_DEBUG, "[distroav] SyncDock: No source selected");
		return;
	}

	obs_enum_sources([](void *param, obs_source_t *source) {
		auto *dock = (SyncTestDock *)param;

		const char *source_id = obs_source_get_id(source);
		if (source_id && strcmp(source_id, "ndi_source") == 0) {
			const char *name = obs_source_get_name(source);
			if (name && dock->selectedSourceName == QString::fromUtf8(name)) {
				auto *sh = obs_source_get_signal_handler(source);
				signal_handler_connect(sh, "ndi_timing", cb_ndi_timing, dock);
				dock->ndi_source_ref = obs_source_get_weak_source(source);

				blog(LOG_INFO, "[distroav] SyncDock: Connected to NDI source '%s' for timing signals", name);
				return false;
			}
		}
		return true;
	}, this);

	if (!ndi_source_ref) {
		blog(LOG_WARNING, "[distroav] SyncDock: Could not find NDI source '%s'",
			selectedSourceName.toUtf8().constData());
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

void SyncTestDock::cb_source_created(void *param, calldata_t *cd)
{
	auto *dock = (SyncTestDock *)param;
	obs_source_t *source = nullptr;
	if (!calldata_get_ptr(cd, "source", &source) || !source)
		return;

	const char *source_id = obs_source_get_id(source);
	if (source_id && strcmp(source_id, "ndi_source") == 0) {
		QMetaObject::invokeMethod(dock, [dock]() {
			dock->populateNdiSourceList();
		});
	}
}

void SyncTestDock::cb_source_destroyed(void *param, calldata_t *cd)
{
	auto *dock = (SyncTestDock *)param;
	obs_source_t *source = nullptr;
	if (!calldata_get_ptr(cd, "source", &source) || !source)
		return;

	const char *source_id = obs_source_get_id(source);
	if (source_id && strcmp(source_id, "ndi_source") == 0) {
		QMetaObject::invokeMethod(dock, [dock]() {
			dock->populateNdiSourceList();
		});
	}
}

void SyncTestDock::cb_source_renamed(void *param, calldata_t *cd)
{
	auto *dock = (SyncTestDock *)param;
	obs_source_t *source = nullptr;
	if (!calldata_get_ptr(cd, "source", &source) || !source)
		return;

	const char *source_id = obs_source_get_id(source);
	if (source_id && strcmp(source_id, "ndi_source") == 0) {
		// Get old and new names to update selectedSourceName if needed
		const char *new_name = nullptr;
		calldata_get_string(cd, "new_name", &new_name);

		QMetaObject::invokeMethod(dock, [dock, new_name = QString::fromUtf8(new_name ? new_name : "")]() {
			// If the renamed source was our selected source, update our selection name
			// The populateNdiSourceList will handle updating the combo box
			if (!new_name.isEmpty() && dock->ndi_source_ref) {
				OBSSourceAutoRelease src = obs_weak_source_get_source(dock->ndi_source_ref);
				if (src) {
					const char *current_name = obs_source_get_name(src);
					if (current_name && new_name == QString::fromUtf8(current_name)) {
						dock->selectedSourceName = new_name;
					}
				}
			}
			dock->populateNdiSourceList();
		});
	}
}
