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

#include "plugin-main.h"
#include "network-monitor.h"
#include "config-notifier.h"
#include "ndi-network-report.h"
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace {
struct SenderColumnDef {
	const char *header;
	const char *unit;
};

struct ReceiverColumnDef {
	const char *header;
	const char *unit;
};

std::string formatFixed2(double value)
{
	std::ostringstream out;
	out << std::fixed << std::setprecision(2) << value;
	return out.str();
}

std::string formatRounded(double value)
{
	return std::to_string(std::lround(value));
}

std::string formatBps(double bps)
{
	std::ostringstream out;

	if (bps >= 1e9) {
		out << std::fixed << std::setprecision(2) << (bps / 1e9) << " Gbps";
	} else if (bps >= 1e6) {
		out << std::fixed << std::setprecision(2) << (bps / 1e6) << " Mbps";
	} else if (bps >= 1e3) {
		out << std::fixed << std::setprecision(2) << (bps / 1e3) << " Kbps";
	} else {
		out << std::fixed << std::setprecision(0) << bps << " bps";
	}

	return out.str();
}

template<size_t N>
std::array<std::size_t, N> computeReceiverWidths(const std::array<ReceiverColumnDef, N> &columns,
						 const NetworkMonitor::ReceiverInfoMap &receivers)
{
	std::array<std::size_t, N> widths = {};
	for (std::size_t i = 0; i < N; ++i) {
		widths[i] = std::max<std::size_t>(std::strlen(columns[i].header), std::strlen(columns[i].unit));
	}

	for (const auto &pair : receivers) {
		const auto &snapshot = pair.second->getReportSnapshot();
		const std::array<std::string, N> values = {{
			pair.second->get_ndi_name(),
			snapshot.obs_source_name,
			std::to_string(snapshot.video_frames_dropped),
			std::to_string(snapshot.video_queue_size),
			snapshot.format_description,
			formatFixed2(snapshot.osFPS),
			formatFixed2(snapshot.tsFPS),
			formatFixed2(snapshot.deficit_fps),
			formatFixed2(snapshot.jitter_ratio),
			formatFixed2(snapshot.budget_used_per_frame_capture),
			formatFixed2(snapshot.max_capture_pct),
			formatFixed2(snapshot.budget_used_per_frame_processing),
			formatFixed2(snapshot.max_process_pct),
			formatRounded(snapshot.osSPS),
			formatFixed2(snapshot.deficit_sps),
			format_av_drift_ms_per_hour(snapshot),
		}};
		for (std::size_t i = 0; i < N; ++i)
			widths[i] = std::max<std::size_t>(widths[i], values[i].size());
	}

	return widths;
}

template<size_t N>
std::array<std::size_t, N> computeSenderWidths(const std::array<SenderColumnDef, N> &columns,
					       const NetworkMonitor::SenderInfoMap &senders)
{
	std::array<std::size_t, N> widths = {};
	for (std::size_t i = 0; i < N; ++i) {
		widths[i] = std::max<std::size_t>(std::strlen(columns[i].header), std::strlen(columns[i].unit));
	}

	for (const auto &pair : senders) {
		const auto &snapshot = pair.second->getReportSnapshot();
		const std::array<std::string, N> values = {{
			pair.second->get_ndi_name(),
			snapshot.format_description,
			std::to_string(snapshot.receivers),
			snapshot.discoverable ? "Yes" : "No",
			formatFixed2(snapshot.osFPS),
			formatFixed2(snapshot.tsFPS),
			formatFixed2(snapshot.deficit_fps),
			formatFixed2(snapshot.deficit_sps),
			formatFixed2(snapshot.jitter_ratio),
			formatFixed2(snapshot.budget_used_per_frame_blocking),
			formatFixed2(snapshot.max_send_block_pct),
			formatFixed2(snapshot.budget_used_per_frame_processing),
			formatFixed2(snapshot.max_process_pct),
			std::to_string(snapshot.receiver_changes),
			std::to_string(snapshot.discoverable_changes),
		}};
		for (std::size_t i = 0; i < N; ++i)
			widths[i] = std::max<std::size_t>(widths[i], values[i].size());
	}

	return widths;
}

template<size_t N>
void appendReceiverSeparator(std::ostringstream &out, const std::array<std::size_t, N> &widths, char fill)
{
	out << '+';
	for (std::size_t i = 0; i < N; ++i) {
		out << std::string(widths[i] + 2, fill) << '+';
	}
	out << '\n';
}

template<size_t N>
void appendReceiverRow(std::ostringstream &out, const std::array<std::string, N> &values,
		       const std::array<std::size_t, N> &widths)
{
	out << '|';
	for (std::size_t i = 0; i < N; ++i) {
		out << ' ' << std::left << std::setw(static_cast<int>(widths[i])) << values[i] << ' ' << '|';
	}
	out << '\n';
}
} // namespace

NetworkMonitor::NetworkMonitor() : m_running(true), m_checkInterval(std::chrono::milliseconds(1000))
{
	// Launch the background monitor thread. It runs until m_running is
	// cleared in the destructor.
	m_thread = std::thread(&NetworkMonitor::monitorLoop, this);
	m_senders.clear();
	NDIlib_find_create_t find_desc = {0};
	find_desc.show_local_sources = true;
	find_desc.p_groups = NULL;
	m_ndi_find = nullptr;
	if (ndiLib)
		m_ndi_find = ndiLib->find_create_v2(&find_desc);
	m_network_report = DiagnoseNdiNetwork();
}

NetworkMonitor::~NetworkMonitor()
{
	// Signal the monitor thread to stop and wake it immediately, then
	// wait for it to exit cleanly.
	m_running.store(false);
	m_cv.notify_all();

	if (m_thread.joinable()) {
		m_thread.join();
	}
	if (ndiLib)
		ndiLib->find_destroy(m_ndi_find);
}

SenderInfo *NetworkMonitor::registerSender(NDIlib_send_instance_t sender, std::string tag)
{
	if (sender == nullptr) {
		return nullptr;
	}

	SenderInfo *sender_info = nullptr;

	{
		std::lock_guard<std::mutex> lock(m_mutex);

		auto it = m_senders.find(sender);
		if (it == m_senders.end()) {
			auto sp = std::make_shared<SenderInfo>(sender);
			sp->set_tag(tag);
			getCurrentSenderInfo(sender, sp.get());

			m_senders[sender] = sp;
			sender_info = sp.get();
		} else {
			// Sender already registered, return existing info.
			sender_info = it->second.get();
		}
	}

	// Wake the monitor thread so it can react to the change right away.
	m_cv.notify_all();
	return sender_info;
}

void NetworkMonitor::unregisterSender(NDIlib_send_instance_t sender)
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_senders.find(sender);
		if (it != m_senders.end()) {
			m_senders.erase(it);
		}
	}
	m_cv.notify_all();
}

SenderInfo *NetworkMonitor::getSenderInfo(NDIlib_send_instance_t sender) const
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (sender == nullptr) {
		return nullptr;
	}

	auto it = m_senders.find(sender);
	if (it != m_senders.end()) {
		return it->second.get();
	}

	return nullptr;
}

SenderInfo *NetworkMonitor::getSenderInfo(std::string tag) const
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (tag.empty()) {
		return nullptr;
	}

	for (const auto &pair : m_senders) {
		if (pair.second->get_tag() == tag) {
			return pair.second.get();
		}
	}

	return nullptr;
}

std::string NetworkMonitor::getSenderStatus(SenderInfo *senderInfo) const
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (!senderInfo) {
		return SenderInfo::get_missing_sender_status();
	}

	for (const auto &pair : m_senders) {
		if (pair.second.get() == senderInfo) {
			return pair.second->get_brief_sender_status();
		}
	}

	return SenderInfo::get_missing_sender_status();
}

std::string NetworkMonitor::getReceiverStatus(ReceiverInfo *receiverInfo) const
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (!receiverInfo) {
		return ReceiverInfo::get_missing_receiver_status();
	}

	for (const auto &pair : m_receivers) {
		if (pair.second.get() == receiverInfo) {
			return pair.second->get_brief_receiver_status();
		}
	}

	return ReceiverInfo::get_missing_receiver_status();
}

std::string getFormattedReceiverReport(const NetworkMonitor::ReceiverInfoMap &m_receivers)
{
	static const std::array<ReceiverColumnDef, 16> kColumns = {{
		{"NDI Name", ""},
		{"OBS Source Name", ""},
		{"Dropped", "frames"},
		{"Queued", "frames"},
		{"Format", ""},
		{"FPS", "fps"},
		{"TS FPS", "fps"},
		{"FPS Deficit %", "%"},
		{"Jitter Ratio", "ratio"},
		{"Capture %", "%"},
		{"Max Capture %", "ratio"},
		{"Process %", "%"},
		{"Max Process %", "ratio"},
		{"SPS", "sps"},
		{"SPS Deficit %", "%"},
		{"Drift ms/hr", "ms/hr"},
	}};

	std::ostringstream out;
	const auto widths = computeReceiverWidths(kColumns, m_receivers);

	std::array<std::string, kColumns.size()> headers = {};
	for (std::size_t i = 0; i < kColumns.size(); ++i) {
		headers[i] = kColumns[i].header;
	}

	out << "NDI Receiver Report\n";
	appendReceiverSeparator(out, widths, '-');
	appendReceiverRow(out, headers, widths);
	appendReceiverSeparator(out, widths, '=');

	// m_receivers is keyed by obs_source_t*, so iterating it directly has
	// no meaningful order - sort by NDI name for a stable, readable report.
	std::vector<std::shared_ptr<ReceiverInfo>> sortedReceivers;
	sortedReceivers.reserve(m_receivers.size());
	for (const auto &pair : m_receivers) {
		if (pair.second)
			sortedReceivers.push_back(pair.second);
	}
	std::sort(sortedReceivers.begin(), sortedReceivers.end(),
		  [](const auto &a, const auto &b) { return a->get_ndi_name() < b->get_ndi_name(); });

	for (const auto &receiver : sortedReceivers) {
		const auto snapshot = receiver->getReportSnapshot();
		const std::array<std::string, 16> values = {{
			receiver->get_ndi_name(),
			snapshot.obs_source_name,
			std::to_string(snapshot.video_frames_dropped),
			std::to_string(snapshot.video_queue_size),
			snapshot.format_description,
			formatFixed2(snapshot.osFPS),
			formatFixed2(snapshot.tsFPS),
			formatFixed2(snapshot.deficit_fps),
			formatFixed2(snapshot.jitter_ratio),
			formatFixed2(snapshot.budget_used_per_frame_capture),
			formatFixed2(snapshot.max_capture_pct),
			formatFixed2(snapshot.budget_used_per_frame_processing),
			formatFixed2(snapshot.max_process_pct),
			formatRounded(snapshot.osSPS),
			formatFixed2(snapshot.deficit_sps),
			format_av_drift_ms_per_hour(snapshot),
		}};
		appendReceiverRow(out, values, widths);
	}
	appendReceiverSeparator(out, widths, '-');
	return out.str();
}

std::string getFormattedSenderReport(const NetworkMonitor::SenderInfoMap &m_senders)
{
	static const std::array<SenderColumnDef, 15> kColumns = {{
		{"NDI name", ""},
		{"Format", ""},
		{"Recvrs", ""},
		{"Disc?", ""},
		{"FPS", "fps"},
		{"TS FPS", "fps"},
		{"FPS Deficit %", "%"},
		{"SPS Deficit %", "%"},
		{"Jitter Ratio", "ratio"},
		{"Send Block %", "%"},
		{"Max Send Block %", "ratio"},
		{"Processing %", "%"},
		{"Max Processing %", "ratio"},
		{"Recvr Changes", ""},
		{"Disc Changes", ""},
	}};

	std::ostringstream out;
	const auto widths = computeSenderWidths(kColumns, m_senders);

	std::array<std::string, kColumns.size()> headers = {};
	for (std::size_t i = 0; i < kColumns.size(); ++i) {
		headers[i] = kColumns[i].header;
	}

	out << "NDI Sender Report\n";
	appendReceiverSeparator(out, widths, '-');
	appendReceiverRow(out, headers, widths);
	appendReceiverSeparator(out, widths, '=');

	// m_senders is keyed by NDIlib_send_instance_t, so iterating it directly
	// has no meaningful order - sort by NDI name for a stable, readable report.
	std::vector<std::shared_ptr<SenderInfo>> sortedSenders;
	sortedSenders.reserve(m_senders.size());
	for (const auto &pair : m_senders) {
		if (pair.second)
			sortedSenders.push_back(pair.second);
	}
	std::sort(sortedSenders.begin(), sortedSenders.end(),
		  [](const auto &a, const auto &b) { return a->get_ndi_name() < b->get_ndi_name(); });

	for (const auto &sender : sortedSenders) {
		const auto snapshot = sender->getReportSnapshot();
		const std::array<std::string, 15> values = {{
			sender->get_ndi_name(),
			snapshot.format_description,
			std::to_string(snapshot.receivers),
			snapshot.discoverable ? "Yes" : "No",
			formatFixed2(snapshot.osFPS),
			formatFixed2(snapshot.tsFPS),
			formatFixed2(snapshot.deficit_fps),
			formatFixed2(snapshot.deficit_sps),
			formatFixed2(snapshot.jitter_ratio),
			formatFixed2(snapshot.budget_used_per_frame_blocking),
			formatFixed2(snapshot.max_send_block_pct),
			formatFixed2(snapshot.budget_used_per_frame_processing),
			formatFixed2(snapshot.max_process_pct),
			std::to_string(snapshot.receiver_changes),
			std::to_string(snapshot.discoverable_changes),
		}};
		appendReceiverRow(out, values, widths);
	}
	appendReceiverSeparator(out, widths, '-');
	return out.str();
}

void dumpSenderReportToLog(const NetworkMonitor::SenderInfoMap &m_senders)
{
	const char reportHeader[] = "NDI Sender Report";
	obs_log(LOG_INFO, "%s ----------------------------------------", reportHeader);
	for (const auto &pair : m_senders) {
		const auto &sender = pair.second;
		if (!sender)
			continue;
		std::string ndi_name = sender->get_ndi_name();
		const auto snapshot = sender->getReportSnapshot();
		obs_log(LOG_INFO, "%s '%s' Format: %s", reportHeader, ndi_name.c_str(),
			snapshot.format_description.c_str());
		obs_log(LOG_INFO, "%s '%s' Receivers: %zu", reportHeader, ndi_name.c_str(), snapshot.receivers);
		obs_log(LOG_INFO, "%s '%s' Disc?: %s", reportHeader, ndi_name.c_str(),
			snapshot.discoverable ? "Yes" : "No");
		obs_log(LOG_INFO, "%s '%s' FPS: %.2f fps", reportHeader, ndi_name.c_str(), snapshot.osFPS);
		obs_log(LOG_INFO, "%s '%s' TS FPS: %.2f fps", reportHeader, ndi_name.c_str(), snapshot.tsFPS);
		obs_log(LOG_INFO, "%s '%s' FPS Deficit %%: %.2f", reportHeader, ndi_name.c_str(), snapshot.deficit_fps);
		obs_log(LOG_INFO, "%s '%s' SPS Deficit %%: %.2f", reportHeader, ndi_name.c_str(), snapshot.deficit_sps);
		obs_log(LOG_INFO, "%s '%s' Jitter Ratio: %.2f", reportHeader, ndi_name.c_str(), snapshot.jitter_ratio);
		obs_log(LOG_INFO, "%s '%s' Send Block %%: %.2f", reportHeader, ndi_name.c_str(),
			snapshot.budget_used_per_frame_blocking);
		obs_log(LOG_INFO, "%s '%s' Max Send Block %%: %.2f", reportHeader, ndi_name.c_str(),
			snapshot.max_send_block_pct);
		obs_log(LOG_INFO, "%s '%s' Processing %%: %.2f", reportHeader, ndi_name.c_str(),
			snapshot.budget_used_per_frame_processing);
		obs_log(LOG_INFO, "%s '%s' Max Processing %%: %.2f", reportHeader, ndi_name.c_str(),
			snapshot.max_process_pct);
		obs_log(LOG_INFO, "%s '%s' Recvr Changes: %zu", reportHeader, ndi_name.c_str(),
			snapshot.receiver_changes);
		obs_log(LOG_INFO, "%s '%s' Disc Changes: %zu", reportHeader, ndi_name.c_str(),
			snapshot.discoverable_changes);
	}
}

void dumpAdapterReportToLog(const NdiNetworkReport &m_network_report)
{
	const char reportHeader[] = "NDI Adapter Report";
	obs_log(LOG_INFO, "%s ----------------------------------------", reportHeader);
	obs_log(LOG_INFO, "%s mDNS bind OK: %s", reportHeader, m_network_report.mdnsSocketBindOk ? "Yes" : "No");
	obs_log(LOG_INFO, "%s mDNS port not in use: %s", reportHeader, m_network_report.mdnsPortInUse ? "Yes" : "No");

	for (const auto &adapter : m_network_report.adapters) {
		std::string adapterName = adapter.friendlyName.empty() ? adapter.description : adapter.friendlyName;
		obs_log(LOG_INFO, "%s '%s' NDI: %s", reportHeader, adapterName.c_str(),
			adapter.ndiCandidate ? "Yes" : "No");
		obs_log(LOG_INFO, "%s '%s' Description: %s", reportHeader, adapterName.c_str(),
			adapter.description.c_str());
		obs_log(LOG_INFO, "%s '%s' IPv4 Address: %s", reportHeader, adapterName.c_str(),
			adapter.ipv4Address.c_str());
		obs_log(LOG_INFO, "%s '%s' Subnet Mask: %s", reportHeader, adapterName.c_str(),
			adapter.subnetMask.c_str());
		obs_log(LOG_INFO, "%s '%s' Up: %s", reportHeader, adapterName.c_str(), adapter.isUp ? "Yes" : "No");
		obs_log(LOG_INFO, "%s '%s' Multi: %s", reportHeader, adapterName.c_str(),
			adapter.supportsMulticast ? "Yes" : "No");
		obs_log(LOG_INFO, "%s '%s' Virt: %s", reportHeader, adapterName.c_str(),
			adapter.looksVirtual ? "Yes" : "No");
		obs_log(LOG_INFO, "%s '%s' Firewall Enabled: %s", reportHeader, adapterName.c_str(),
			adapter.firewallEnabled ? "Yes" : "No");
		obs_log(LOG_INFO, "%s '%s' mDNS Port Open: %s", reportHeader, adapterName.c_str(),
			FirewallPortAccessToString(adapter.mdnsPortOpen).c_str());
		obs_log(LOG_INFO, "%s '%s' NDI Ports Open: %s", reportHeader, adapterName.c_str(),
			FirewallPortAccessToString(adapter.ndiPortsOpen).c_str());
		obs_log(LOG_INFO, "%s '%s' F&P Sharing: %s", reportHeader, adapterName.c_str(),
			FirewallPortAccessToString(adapter.filePrinterSharing).c_str());
		obs_log(LOG_INFO, "%s '%s' Type: %lu (%s)", reportHeader, adapterName.c_str(),
			static_cast<unsigned long>(adapter.ifType), IfTypeToString(adapter.ifType).c_str());
		obs_log(LOG_INFO, "%s '%s' In: %s", reportHeader, adapterName.c_str(),
			formatBps(adapter.inBps).c_str());
		obs_log(LOG_INFO, "%s '%s' Out: %s", reportHeader, adapterName.c_str(),
			formatBps(adapter.outBps).c_str());
		obs_log(LOG_INFO, "%s '%s' Rx Speed: %s", reportHeader, adapterName.c_str(),
			formatBps(adapter.receiveSpeed).c_str());
		obs_log(LOG_INFO, "%s '%s' Tx Speed: %s", reportHeader, adapterName.c_str(),
			formatBps(adapter.transmitSpeed).c_str());
	}
}

void dumpReceiverReportToLog(const NetworkMonitor::ReceiverInfoMap &m_receivers)
{
	const char reportHeader[] = "NDI Receiver Report";
	obs_log(LOG_INFO, "%s ----------------------------------------", reportHeader);
	for (const auto &pair : m_receivers) {
		const auto &receiver = pair.second;
		if (!receiver)
			continue;

		std::string ndi_name = receiver->get_ndi_name();
		const auto snapshot = receiver->getReportSnapshot();
		obs_log(LOG_INFO, "%s '%s' OBS Source Name: %s", reportHeader, ndi_name.c_str(),
			snapshot.obs_source_name.c_str());
		obs_log(LOG_INFO, "%s '%s' Dropped frames: %zu", reportHeader, ndi_name.c_str(),
			snapshot.video_frames_dropped);
		obs_log(LOG_INFO, "%s '%s' Queued frames: %zu", reportHeader, ndi_name.c_str(),
			snapshot.video_queue_size);
		obs_log(LOG_INFO, "%s '%s' Format: %s", reportHeader, ndi_name.c_str(),
			snapshot.format_description.c_str());
		obs_log(LOG_INFO, "%s '%s' FPS: %.2f fps", reportHeader, ndi_name.c_str(), snapshot.osFPS);
		obs_log(LOG_INFO, "%s '%s' TS FPS: %.2f fps", reportHeader, ndi_name.c_str(), snapshot.tsFPS);
		obs_log(LOG_INFO, "%s '%s' FPS Deficit %%: %.2f", reportHeader, ndi_name.c_str(), snapshot.deficit_fps);
		obs_log(LOG_INFO, "%s '%s' Jitter Ratio: %.2f", reportHeader, ndi_name.c_str(), snapshot.jitter_ratio);
		obs_log(LOG_INFO, "%s '%s' Capture %%: %.2f", reportHeader, ndi_name.c_str(),
			snapshot.budget_used_per_frame_capture);
		obs_log(LOG_INFO, "%s '%s' Max Capture %%: %.2f", reportHeader, ndi_name.c_str(),
			snapshot.max_capture_pct);
		obs_log(LOG_INFO, "%s '%s' Process %%: %.2f", reportHeader, ndi_name.c_str(),
			snapshot.budget_used_per_frame_processing);
		obs_log(LOG_INFO, "%s '%s' Max Process %%: %.2f", reportHeader, ndi_name.c_str(),
			snapshot.max_process_pct);
		obs_log(LOG_INFO, "%s '%s' SPS: %ld", reportHeader, ndi_name.c_str(), std::lround(snapshot.osSPS));
		obs_log(LOG_INFO, "%s '%s' SPS Deficit %%: %.2f", reportHeader, ndi_name.c_str(), snapshot.deficit_sps);
		obs_log(LOG_INFO, "%s '%s' Drift ms/hr: %s", reportHeader, ndi_name.c_str(),
			format_av_drift_ms_per_hour(snapshot).c_str());
	}
}

std::string NetworkMonitor::getFormattedReceiverReport() const
{
	return ::getFormattedReceiverReport(m_receivers);
}

std::string NetworkMonitor::getFormattedSenderReport() const
{
	return ::getFormattedSenderReport(m_senders);
}
ReceiverInfo *NetworkMonitor::registerReceiver(obs_source_t *ndi_source)
{
	if (ndi_source == nullptr) {
		return nullptr;
	}

	ReceiverInfo *receiver_info = nullptr;

	{
		std::lock_guard<std::mutex> lock(m_mutex);

		auto it = m_receivers.find(ndi_source);
		if (it == m_receivers.end()) {
			auto sp = std::make_shared<ReceiverInfo>(ndi_source);
			getCurrentReceiverInfo(ndi_source, sp.get());

			m_receivers[ndi_source] = sp;
			receiver_info = sp.get();
		} else {
			// Receiver already registered, return existing info.
			receiver_info = it->second.get();
		}
	}

	// Wake the monitor thread so it can react to the change right away.
	m_cv.notify_all();
	return receiver_info;
}

void NetworkMonitor::setReceiver(obs_source_t *ndi_source, NDIlib_recv_instance_t ndi_receiver)
{
	if (ndi_source == nullptr) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_receivers.find(ndi_source);
		if (it != m_receivers.end()) {
			it->second->set_receiver(ndi_receiver);
		}
	}
}

void NetworkMonitor::unregisterReceiver(obs_source_t *ndi_source)
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_receivers.find(ndi_source);
		if (it != m_receivers.end()) {
			m_receivers.erase(it);
		}
	}
	m_cv.notify_all();
}

void NetworkMonitor::setCheckInterval(std::chrono::milliseconds interval)
{
	m_checkInterval.store(interval);
}

void NetworkMonitor::getCurrentSenderInfo(NDIlib_send_instance_t sender, SenderInfo *senderInfo)
{
	if (senderInfo == nullptr) {
		return;
	}

	if (sender) {
		senderInfo->calculate_stats();
	}
}

void NetworkMonitor::getCurrentReceiverInfo(obs_source_t *ndi_source, ReceiverInfo *receiverInfo)
{
	if (receiverInfo == nullptr) {
		return;
	}

	if (ndi_source) {
		receiverInfo->calculate_stats();
	}
}
void NetworkMonitor::monitorLoop()
{
	while (m_running.load()) {

		updateNDISourceList();

		updateNetworkReport(); // modifies m_network_report.adapters
		UpdateNicbps(m_network_report.adapters);

		notifyNetworkChange(); // Notify the UI that the network report has changed, so it can update the table view.

		StatusMap oldStatusMap, newStatusMap;
		oldStatusMap.clear();
		newStatusMap.clear();

		// Capture the existing status for each sender and update the current status. Only look at the brief status string,
		// which is a summary of the sender's state seen in properties.
		// This is done while holding the lock to ensure no changes occur to the sender list during this operation.
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			for (const auto &sender : m_senders) {
				oldStatusMap[sender.first] = sender.second->get_brief_sender_status();
				getCurrentSenderInfo(sender.first, sender.second.get());
				newStatusMap[sender.first] = sender.second->get_brief_sender_status();
			}
		}

		// Notify any registered ChangeNotifier objects for senders that have changed status.
		onSenderChanged(oldStatusMap, newStatusMap);

		notifySenderChange(); // Notify the UI that the sender list has changed, so it can update the table view.

		oldStatusMap.clear();
		newStatusMap.clear();

		// Capture the existing status for each receiver and update the current status. Only look at the brief status string,
		// which is a summary of the receiver's state seen in properties.
		// This is done while holding the lock to ensure no changes occur to the receiver list during this operation.
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			for (const auto &receiver : m_receivers) {
				oldStatusMap[receiver.first] = receiver.second->get_brief_receiver_status();
				getCurrentReceiverInfo(receiver.first, receiver.second.get());
				newStatusMap[receiver.first] = receiver.second->get_brief_receiver_status();
			}
		}

		// Notify any registered ChangeNotifier objects for receivers that have changed status.
		onReceiverChanged(oldStatusMap, newStatusMap);

		notifyReceiverChange(); // Notify the UI that the receiver list has changed so it can update the table widget.

		// Sleep for the configured interval, but wake early if the
		// sender list changes or shutdown is requested.
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cv.wait_for(lock, m_checkInterval.load(), [this]() { return !m_running.load(); });
	}
}

void NetworkMonitor::onSenderChanged(StatusMap before, StatusMap now)
{
	bool changed = false;

	for (const auto &pair : before) {
		const NDIlib_send_instance_t &key = (NDIlib_send_instance_t)pair.first;
		bool should_notify = false;

		// Decide whether this key changed according to the maps
		auto it_now = now.find(key);
		if (it_now == now.end()) {
			should_notify = true;
		} else if (it_now->second != pair.second) {
			should_notify = true;
		}

		if (!should_notify)
			continue;

		// Safely grab the shared_ptr while holding the lock, then call notify_change without the lock
		std::shared_ptr<SenderInfo> sptr;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			auto it = m_senders.find(key);
			if (it != m_senders.end())
				sptr = it->second;
		}

		if (sptr) {
			sptr->notify_change();
			changed = true;
		}
	}
}

void NetworkMonitor::onReceiverChanged(StatusMap before, StatusMap now)
{
	bool changed = false;

	for (const auto &pair : before) {
		obs_source_t *key = (obs_source_t *)pair.first;
		bool should_notify = false;

		// Decide whether this key changed according to the maps
		auto it_now = now.find((void *)key);
		if (it_now == now.end()) {
			should_notify = true;
		} else if (it_now->second != pair.second) {
			should_notify = true;
		}

		if (!should_notify)
			continue;

		// Safely grab the shared_ptr while holding the lock, then call notify_change without the lock
		std::shared_ptr<ReceiverInfo> sptr;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			auto it = m_receivers.find(key);
			if (it != m_receivers.end())
				sptr = it->second;
		}

		if (sptr) {
			sptr->notify_change();
			changed = true;
		}
	}
}
void NetworkMonitor::updateNetworkReport()
{
	NdiNetworkReport new_report = DiagnoseNdiNetwork();
	for (auto &adapter : new_report.adapters) {
		// Find the corresponding adapter in the existing report
		auto it = std::find_if(m_network_report.adapters.begin(), m_network_report.adapters.end(),
				       [&adapter](const NdiAdapterInfo &existing_adapter) {
					       return existing_adapter.luid.Value == adapter.luid.Value;
				       });
		if (it != m_network_report.adapters.end()) {
			// Update the existing adapter's information with the new data
			it->inBps = adapter.inBps;
			it->outBps = adapter.outBps;
			it->isUp = adapter.isUp;
			it->supportsMulticast = adapter.supportsMulticast;
			it->looksVirtual = adapter.looksVirtual;
			it->ifType = adapter.ifType;
			it->ndiCandidate = adapter.ndiCandidate;
			it->networkCategory = adapter.networkCategory;
			it->multicastJoinOk = adapter.multicastJoinOk;
			it->multicastRoundTripOk = adapter.multicastRoundTripOk;
			it->firewallEnabled = adapter.firewallEnabled;
			it->mdnsPortOpen = adapter.mdnsPortOpen;
			it->ndiPortsOpen = adapter.ndiPortsOpen;
			it->filePrinterSharing = adapter.filePrinterSharing;
		} else {
			// If the adapter is new, add it to the existing report
			m_network_report.adapters.push_back(adapter);
		}
	}
	// remove any adapters that are no longer present in the new report
	m_network_report.adapters.erase(
		std::remove_if(m_network_report.adapters.begin(), m_network_report.adapters.end(),
			       [&new_report](const NdiAdapterInfo &existing_adapter) {
				       return std::none_of(new_report.adapters.begin(), new_report.adapters.end(),
							   [&existing_adapter](const NdiAdapterInfo &adapter) {
								   return existing_adapter.luid.Value ==
									  adapter.luid.Value;
							   });
			       }),
		m_network_report.adapters.end());
}

void NetworkMonitor::updateNDISourceList()
{
	// Safety check to avoid crash if the Lib is not loaded.
	if (!ndiLib) {
		return;
	}

	uint32_t n_sources = 0;
	uint32_t last_n_sources = 0;
	const NDIlib_source_t *sources = NULL;
	do {
		std::lock_guard<std::mutex> lock(m_mutex);

		if (ndiLib && m_ndi_find)
			ndiLib->find_wait_for_sources(m_ndi_find, 1000);
		last_n_sources = n_sources;

		n_sources = 0;
		if (ndiLib)
			sources = ndiLib->find_get_current_sources(m_ndi_find, &n_sources);
	} while (n_sources > last_n_sources);

	std::vector<std::string> newList;
	newList.reserve(n_sources);

	for (uint32_t i = 0; i < n_sources; ++i) {
		std::string name = std::string(sources[i].p_ndi_name ? sources[i].p_ndi_name : "");
		newList.push_back(name);
	}

	std::vector<std::string> oldList;
	{
		std::lock_guard<std::mutex> lock(m_list_mutex);
		oldList = m_ndi_source_list;
	}

	// Sort the newly discovered list and compare element-wise with the stored list
	bool listChanged = false;

	std::sort(newList.begin(), newList.end());

	if (newList.size() != oldList.size()) {
		listChanged = true;
	} else {
		for (size_t i = 0; i < newList.size(); ++i) {
			if (newList[i] != oldList[i]) {
				listChanged = true;
				break;
			}
		}
	}

	if (listChanged) {
		std::lock_guard<std::mutex> lock(m_list_mutex);
		m_ndi_source_list = newList;
		emit ConfigNotifier::instance() -> configChanged();
	}
}

std::string NetworkMonitor::get_fourcc_string(NDIlib_FourCC_video_type_e fourcc)
{
	if (fourcc == NDIlib_FourCC_video_type_max)
		return "None";
	uint32_t v = static_cast<uint32_t>(fourcc);
	char ch[5];
	ch[0] = static_cast<char>(v & 0xFF);
	ch[1] = static_cast<char>((v >> 8) & 0xFF);
	ch[2] = static_cast<char>((v >> 16) & 0xFF);
	ch[3] = static_cast<char>((v >> 24) & 0xFF);
	ch[4] = '\0';
	std::string fourccStr(ch);
	for (char &c : fourccStr) {
		unsigned char uc = static_cast<unsigned char>(c);
		if (uc < 32 || uc > 126)
			c = '.';
	}

	return fourccStr;
}

std::string NetworkMonitor::framerate_to_string(int frame_rate_N, int frame_rate_D)
{
	if (frame_rate_D > 0) {
		double fps = static_cast<double>(frame_rate_N) / static_cast<double>(frame_rate_D);
		double rounded = std::round(fps * 100.0) / 100.0;
		std::ostringstream ss;
		ss.setf(std::ios::fixed);
		ss << std::setprecision(2) << rounded;
		std::string s = ss.str();
		if (s.find('.') != std::string::npos) {
			while (!s.empty() && s.back() == '0')
				s.pop_back();
			if (!s.empty() && s.back() == '.')
				s.pop_back();
		}
		return s;
	} else {
		return "None";
	}
}

std::string NetworkMonitor::getFormatDescription(int xres, int yres, int frame_rate_D, int frame_rate_N,
						 NDIlib_FourCC_video_type_e fourcc)
{
	if (xres <= 0 || yres <= 0 || frame_rate_D <= 0 || frame_rate_N <= 0) {
		return "None";
	}
	std::string desc = std::to_string(xres) + "x" + std::to_string(yres);
	desc += "@" + framerate_to_string(frame_rate_N, frame_rate_D);
	desc += "[" + get_fourcc_string(fourcc) + "]";
	return desc;
}

void NetworkMonitor::dumpNetworkReportToLog() const
{
	dumpSenderReportToLog(m_senders);
	dumpReceiverReportToLog(m_receivers);
	dumpAdapterReportToLog(m_network_report);
}
