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

#include "ndi-sender-report.h"
#include "plugin-main.h" // provides extern ndiLib and network_monitor
#include <algorithm>
#include <vector>
#include <string>

// Constructor
SenderInfo::SenderInfo(NDIlib_send_instance_t sender) : m_sender(sender), tag(""), ndi_name(""), running(), snapshot()
{
}

// Accessors
size_t SenderInfo::get_receivers() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return snapshot.receivers;
}

void SenderInfo::set_receivers(size_t r)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	snapshot.receivers = r;
}

bool SenderInfo::is_discoverable() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return snapshot.discoverable;
}

void SenderInfo::set_discoverable(bool d)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	snapshot.discoverable = d;
}

void SenderInfo::registerChangeNotifier(ChangeNotifier *notifier, const std::string &key)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!notifier || key.empty())
		return;
	changeNotifiers[key] = notifier;
}

void SenderInfo::unregisterChangeNotifier(ChangeNotifier *notifier, const std::string &key)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = changeNotifiers.find(key);
	if (it == changeNotifiers.end())
		return;
	if (notifier == nullptr || it->second == notifier) {
		changeNotifiers.erase(it);
	}
}

void SenderInfo::notify_change()
{
	std::vector<ChangeNotifier *> notifiers;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		notifiers.reserve(changeNotifiers.size());
		for (auto &p : changeNotifiers)
			notifiers.push_back(p.second);
	}
	for (auto *n : notifiers) {
		if (n)
			n->emitChanged();
	}
}

std::string SenderInfo::get_tag() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return tag;
}

void SenderInfo::set_tag(const std::string &t)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	tag = t;
}

void SenderInfo::set_ndi_name(const std::string &name)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	ndi_name = name;
}

std::string SenderInfo::get_ndi_name() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return ndi_name;
}

NDISenderStats SenderInfo::getReportSnapshot() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return snapshot;
}

std::string SenderInfo::get_brief_sender_status() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	std::string status = "NDI Sender: ";
	status += "Valid";
	status += ", Discoverable: " + std::string(snapshot.discoverable ? "Yes" : "No");
	status += ", Receivers: " + std::to_string(snapshot.receivers);
	return status;
}

std::string SenderInfo::get_missing_sender_status()
{
	return "NDI Sender: Invalid, Discoverable: No, Receivers: 0";
}

// Called when a video frame is submitted to the NDI send API.
void SenderInfo::send_video_frame(uint64_t t0, uint64_t t1, uint64_t t2, NDIlib_video_frame_v2_t &ndi_frame,
				  video_data *)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (running.video_frame_count == 0) {
		running.start_video_frame_os = t1;
		running.start_video_frame_timestamp = ndi_frame.timestamp * 100ULL;
		running.tot_frame_interval = 0;
		running.max_frame_interval = 0;
		running.fourcc = ndi_frame.FourCC;
		running.xres = ndi_frame.xres;
		running.yres = ndi_frame.yres;
		running.frame_rate_D = ndi_frame.frame_rate_D;
		running.frame_rate_N = ndi_frame.frame_rate_N;
		running.tot_video_send_block = 0;
		running.max_video_send_block = 0;
		running.tot_video_processing = 0;
		running.max_video_processing = 0;
	}

	if (running.last_video_frame_timestamp != 0) {
		uint64_t frame_interval = (ndi_frame.timestamp * 100ULL) - running.last_video_frame_timestamp;
		running.tot_frame_interval += frame_interval;
		running.max_frame_interval = std::max<uint64_t>(running.max_frame_interval, frame_interval);
	}

	running.last_video_frame_timestamp = ndi_frame.timestamp * 100ULL;
	running.target_fps = (double)ndi_frame.frame_rate_N / (double)ndi_frame.frame_rate_D;

	uint64_t block_interval = t2 - t1;
	running.tot_video_send_block += block_interval;
	running.max_video_send_block = std::max<uint64_t>(running.max_video_send_block, block_interval);

	uint64_t processing_interval = t1 - t0;
	running.tot_video_processing += processing_interval;
	running.max_video_processing = std::max<uint64_t>(running.max_video_processing, processing_interval);

	running.last_video_frame_os = t1;
	++running.video_frame_count;
}

// Called when audio frames are submitted.
void SenderInfo::send_audio_frame(uint64_t t0, uint64_t, uint64_t, NDIlib_audio_frame_v3_t &ndi_frame,
				  uint64_t timestamp)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (running.audio_sample_count == 0) {
		running.start_audio_sample_os = t0;
		running.start_audio_sample_timestamp = timestamp * 100ULL;
	}
	running.last_audio_sample_timestamp = timestamp * 100ULL;
	running.last_audio_sample_os = t0;
	running.target_sps = (double)ndi_frame.sample_rate;
	running.audio_sample_count += ndi_frame.no_samples;
}

// Compute stats and update snapshot. Implementation lives here (needs ndiLib/network_monitor).
void SenderInfo::calculate_stats()
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		snapshot = running;            // Copy running stats to snapshot for reporting
		running.video_frame_count = 0; // Reset running frame count after snapshot
		running.audio_sample_count = 0;
	}

	snapshot.osFPS = 0.0;
	snapshot.tsFPS = 0.0;
	if (snapshot.video_frame_count > 0) {
		double elapsedSeconds = (double)(snapshot.last_video_frame_os - snapshot.start_video_frame_os) / 1e9;
		if (elapsedSeconds > 0.0) {
			double dfps = (double)snapshot.video_frame_count / elapsedSeconds;
			snapshot.osFPS = dfps;
		}

		elapsedSeconds =
			(double)(snapshot.last_video_frame_timestamp - snapshot.start_video_frame_timestamp) / 1e9;
		if (elapsedSeconds > 0.0) {
			double dfps = (double)snapshot.video_frame_count / elapsedSeconds;
			snapshot.tsFPS = dfps;
		}
		snapshot.avg_frame_interval =
			snapshot.video_frame_count > 0 ? snapshot.tot_frame_interval / snapshot.video_frame_count : 0;
		snapshot.avg_video_send_block =
			snapshot.video_frame_count > 0 ? snapshot.tot_video_send_block / snapshot.video_frame_count : 0;
		snapshot.avg_video_processing =
			snapshot.video_frame_count > 0 ? snapshot.tot_video_processing / snapshot.video_frame_count : 0;
	}

	if (snapshot.audio_sample_count > 0) {
		double elapsedSeconds = (double)(snapshot.last_audio_sample_os - snapshot.start_audio_sample_os) / 1e9;
		if (elapsedSeconds > 0.0) {
			double dsps = (double)snapshot.audio_sample_count / elapsedSeconds;
			snapshot.osSPS = dsps;
		}
		elapsedSeconds =
			(double)(snapshot.last_audio_sample_timestamp - snapshot.start_audio_sample_timestamp) / 1e9;
		if (elapsedSeconds > 0.0) {
			double dsps = (double)snapshot.audio_sample_count / elapsedSeconds;
			snapshot.tsSPS = dsps;
		}
	}

	snapshot.deficit_fps = 0.0;
	snapshot.budget_used_per_frame_blocking = 0.0;
	snapshot.budget_used_per_frame_processing = 0.0;
	snapshot.max_send_block_pct = 0.0;
	snapshot.max_process_pct = 0.0;
	if (snapshot.target_fps > 0) {
		snapshot.deficit_fps = ((snapshot.target_fps - snapshot.osFPS) / snapshot.target_fps);
		snapshot.budget_used_per_frame_blocking = snapshot.avg_video_send_block / (1e9 / snapshot.target_fps);
		snapshot.budget_used_per_frame_processing = snapshot.avg_video_processing / (1e9 / snapshot.target_fps);

		// Worst-case send-block/processing time as a fraction of the target frame period
		// (not divided by the average - so a max that equals one full frame period at
		// the target fps always reads as 100%, regardless of how small the average is).
		snapshot.max_send_block_pct = (double)snapshot.max_video_send_block / (1e9 / snapshot.target_fps);
		snapshot.max_process_pct = (double)snapshot.max_video_processing / (1e9 / snapshot.target_fps);
	}

	snapshot.deficit_sps = 0.0;
	if (snapshot.target_sps > 0)
		snapshot.deficit_sps = ((snapshot.target_sps - snapshot.osSPS) / snapshot.target_sps);

	snapshot.jitter_ratio = 0.0;
	if (snapshot.avg_frame_interval > 0)
		snapshot.jitter_ratio =
			(double)snapshot.max_frame_interval /
			(double)snapshot.avg_frame_interval; // TODO: reset max_frame_interval periodically?

	snapshot.format_description = network_monitor->getFormatDescription(
		snapshot.xres, snapshot.yres, snapshot.frame_rate_D, snapshot.frame_rate_N, snapshot.fourcc);

	// Query ndiLib for connection count and network monitor for discoverability.
	if (ndiLib) {
		size_t conn = (size_t)ndiLib->send_get_no_connections(m_sender, 0);
		if (conn != snapshot.receivers) {
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				running.receiver_changes++;
				running.receivers = conn;
			}
			set_receivers(conn);
		}

		if (network_monitor) {
			std::vector<std::string> ndi_source_list = network_monitor->getNDISourceList();
			const NDIlib_source_t *source = ndiLib->send_get_source_name(m_sender);
			std::string sourceName = source ? source->p_ndi_name : "";
			bool discoverable = std::find(ndi_source_list.begin(), ndi_source_list.end(), sourceName) !=
					    ndi_source_list.end();
			if (discoverable != snapshot.discoverable) {
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					running.discoverable_changes++;
					running.discoverable = discoverable;
				}
				set_discoverable(discoverable);
			}
		}
	}
}
