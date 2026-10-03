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

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <algorithm>
#include <memory>
#include <Processing.NDI.Lib.h>
#include <chrono>
#include <map>
#include <obs-frontend-api.h>
#include "change-notifier.h"
#include "ndi-network-report.h"
#include "ndi-sender-report.h"
#include "ndi-receiver-report.h"

// Maintains a thread-safe list of "sender" pointers (opaque void*) and runs
// a background thread that continuously monitors how many senders are
// currently registered. Callers register/unregister senders from any
// thread; the monitor thread periodically checks the current count.
class NetworkMonitor {
public:
	NetworkMonitor();
	~NetworkMonitor();

	typedef std::map<NDIlib_send_instance_t, std::shared_ptr<SenderInfo>> SenderInfoMap;
	typedef std::map<obs_source_t *, std::shared_ptr<ReceiverInfo>> ReceiverInfoMap;
	typedef std::map<void *, std::string> StatusMap;

	// Non-copyable, non-movable: owns a live thread and synchronization
	// primitives tied to `this`.
	NetworkMonitor(const NetworkMonitor &) = delete;
	NetworkMonitor &operator=(const NetworkMonitor &) = delete;
	NetworkMonitor(NetworkMonitor &&) = delete;
	NetworkMonitor &operator=(NetworkMonitor &&) = delete;

	// Adds `sender` to the list of registered senders. Duplicate pointers
	// are ignored (a sender already registered will not be added twice).
	// Access it using `getSenderInfo(sender)` or 'getSenderInfo(tag)' to get the current cached info for that sender.
	SenderInfo *registerSender(NDIlib_send_instance_t sender, std::string tag = "");

	// Removes `sender` from the list of registered senders, if present.
	void unregisterSender(NDIlib_send_instance_t sender);

	// Returns the cached sender info. This is updated periodically by the monitor thread.
	SenderInfo *getSenderInfo(NDIlib_send_instance_t sender) const;
	SenderInfo *getSenderInfo(std::string tag) const;

	std::vector<std::shared_ptr<SenderInfo>> getAllSenderInfo() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		std::vector<std::shared_ptr<SenderInfo>> sender_info_list;
		for (const auto &pair : m_senders) {
			sender_info_list.push_back(pair.second);
		}
		return sender_info_list;
	}

	// Adds the source as a potential receiver of ndi
	ReceiverInfo *registerReceiver(obs_source_t *ndi_source);
	void setReceiver(obs_source_t *ndi_source, NDIlib_recv_instance_t ndi_receiver);

	// Removes `receiver` from the list of registered receiver.
	void unregisterReceiver(obs_source_t *ndi_source);

	std::vector<std::shared_ptr<ReceiverInfo>> getAllReceiverInfo() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		std::vector<std::shared_ptr<ReceiverInfo>> receiver_info_list;
		for (const auto &pair : m_receivers) {
			receiver_info_list.push_back(pair.second);
		}
		return receiver_info_list;
	}
	std::vector<NdiAdapterInfo> getAllAdapterInfo() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_network_report.adapters;
	}

	std::string getFormattedAdapterReport() const { return FormatNdiNetworkReport(m_network_report); }

	void setNetworkChangeNotifier(ChangeNotifier *notifier)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_network_change_notifier = notifier;
	}

	void notifyNetworkChange()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_network_change_notifier) {
			m_network_change_notifier->emitChanged();
		}
	}

	void setSenderChangeNotifier(ChangeNotifier *notifier)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_sender_change_notifier = notifier;
	}
	void setReceiverChangeNotifier(ChangeNotifier *notifier)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_receiver_change_notifier = notifier;
	}
	void notifySenderChange()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_sender_change_notifier) {
			m_sender_change_notifier->emitChanged();
		}
	}
	void notifyReceiverChange()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_receiver_change_notifier) {
			m_receiver_change_notifier->emitChanged();
		}
	}
	// Returns a string representation of the sender's status. This is updated periodically by the monitor thread.
	std::string getSenderStatus(SenderInfo *sender) const;

	// Returns a string representation of the receiver's status. This is updated periodically by the monitor thread.
	std::string getReceiverStatus(ReceiverInfo *sender) const;

	std::string getFormattedNetworkReport() const { return FormatNdiNetworkReport(m_network_report); }
	std::string getFormattedReceiverReport() const;
	std::string getFormattedSenderReport() const;

	NdiNetworkReport getNetworkReport() const { return m_network_report; }

	// Interval between checks performed by the monitor thread.
	void setCheckInterval(std::chrono::milliseconds interval);
	std::vector<std::string> getNDISourceList() { return m_ndi_source_list; };
	std::string get_fourcc_string(NDIlib_FourCC_video_type_e fourcc);
	std::string framerate_to_string(int frame_rate_N, int frame_rate_D);
	std::string getFormatDescription(int xres, int yres, int frame_rate_D, int frame_rate_N,
					 NDIlib_FourCC_video_type_e fourcc);
	void dumpNetworkReportToLog() const;

private:
	void monitorLoop();
	void onSenderChanged(StatusMap before, StatusMap now);   // hook for the periodic check
	void onReceiverChanged(StatusMap before, StatusMap now); // hook for the periodic check
	void getCurrentSenderInfo(NDIlib_send_instance_t sender, SenderInfo *senderInfo);
	void getCurrentReceiverInfo(obs_source_t *ndi_source, ReceiverInfo *receiverInfo);
	void updateNDISourceList();
	void updateNetworkReport();
	std::string FormattedReceiverReport(const ReceiverInfoMap &m_receivers) const;
	mutable std::mutex m_list_mutex;
	std::vector<std::string> m_ndi_source_list;
	std::condition_variable m_cv;
	std::thread m_thread;
	std::atomic<bool> m_running;
	std::atomic<std::chrono::milliseconds> m_checkInterval;
	mutable std::mutex m_mutex;
	SenderInfoMap m_senders;     // map of sender to status info
	ReceiverInfoMap m_receivers; // map of receiver to status info
	NDIlib_find_instance_t m_ndi_find;
	NdiNetworkReport m_network_report;
	ChangeNotifier *m_network_change_notifier = nullptr;
	ChangeNotifier *m_sender_change_notifier = nullptr;
	ChangeNotifier *m_receiver_change_notifier = nullptr;
};

std::string getFormattedReceiverReport(const NetworkMonitor::ReceiverInfoMap &m_receivers);
std::string getFormattedSenderReport(const NetworkMonitor::SenderInfoMap &m_senders);
void dumpAdapterReportToLog(const NdiNetworkReport &m_network_report);
void dumpReceiverReportToLog(const NetworkMonitor::ReceiverInfoMap &m_receivers);
void dumpSenderReportToLog(const NetworkMonitor::SenderInfoMap &m_senders);
