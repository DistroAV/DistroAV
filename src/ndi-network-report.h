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
#include <cstdint>
#include <string>
#include <vector>
#include "change-notifier.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#else
// ---------------------------------------------------------------------------
// Minimal Windows-type compatibility shims.
//
// The fields below were populated straight from GetAdaptersAddresses()/
// GetIfEntry2() on Windows, using Win32 typedefs. These shims let this
// header - and every .cpp that includes it - compile unmodified on
// Linux/macOS; ndi-network-report.cpp has the platform-specific logic that
// fills the fields in on those platforms.
//
//   ULONG, ULONG64 - plain fixed-width unsigned ints, same widths as Win32's.
//   IF_LUID        - on Windows this is NET_LUID (via <ifdef.h>), an opaque,
//                     stable-for-the-adapter's-lifetime handle used to
//                     re-query counters via GetIfEntry2(). We don't have
//                     that concept on Linux/macOS, so we store the OS
//                     interface index (if_nametoindex()) in the same
//                     ".Value" field the real NET_LUID union exposes. Unlike
//                     a real LUID, an ifindex CAN be reused after an adapter
//                     is unplugged/removed - "good enough for a diagnostics
//                     dialog," not a guaranteed-unique identifier for the
//                     life of the process.
typedef uint32_t ULONG;
typedef uint64_t ULONG64;
struct IF_LUID {
	uint64_t Value = 0;
};
#endif

#if defined(__linux__)
// Linux reports Wi-Fi NICs operating in normal (managed) mode as
// ARPHRD_ETHER (1), the same code as wired Ethernet, so the raw ifType value
// alone can't distinguish them. ndi-network-report.cpp's EnumerateAdapters()/
// GetIfTypeForName() detect Wi-Fi separately (presence of a wireless PHY)
// and store this sentinel in NdiAdapterInfo::ifType instead. Not a real
// ARPHRD_* value. Declared here (rather than privately in
// ndi-network-report.cpp) so any other code that interprets ifType - e.g.
// the adapter table widget's column label - agrees with what's actually
// stored in it. constexpr at namespace scope has internal linkage, so each
// translation unit that includes this header gets its own copy with no ODR
// concerns.
constexpr ULONG kLinuxSyntheticWifiType = 0x80000001u;
#endif

// Helper: convert a UTF-16 std::wstring (Windows wchar_t) to UTF-8 std::string.
// Use this wherever a std::wstring (from Windows APIs) must be converted to
// a UTF-8 std::string to avoid narrow/wchar_t->char implicit conversions.
// Windows-only: Linux/macOS build interface names directly as UTF-8
// std::string (from getifaddrs()), so there's nothing to convert there.
#ifdef _WIN32
static inline std::string utf8_from_wstring(const std::wstring &w)
{
	if (w.empty())
		return std::string();

	// Determine required buffer size (in bytes) for UTF-8
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), NULL, 0, NULL, NULL);
	if (size_needed <= 0)
		return std::string();

	std::string out;
	out.resize(size_needed);
	WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &out[0], size_needed, NULL, NULL);
	return out;
}
#endif

// Windows Network Location Awareness (NLA) category assigned to whichever
// network this specific adapter is currently connected to - the same
// per-interface value PowerShell's `Get-NetConnectionProfile` reports as
// NetworkCategory. This is per-adapter - useful because a machine with
// several NICs can have one adapter categorized Private and another
// Public simultaneously. A direct point-to-point Ethernet link (no DHCP
// server, no gateway) is especially prone to landing on Public, which
// makes Windows Firewall silently drop unsolicited inbound mDNS queries
// on that adapter even though the adapter otherwise looks fine - a common,
// easy-to-miss, single-direction cause of "mDNS discovery works from A to
// B but not B to A". See GetNetworkCategoriesByAdapterGuid() in
// ndi-network-report.cpp for how this is populated on Windows.
//
// Linux/macOS have no equivalent OS-assigned per-network trust level
// (their firewalls, where present, aren't scoped by "which network is
// this" the way NLA is), so this stays Unknown on those platforms rather
// than guessing.
enum class NdiNetworkCategory {
	Unknown, // Not Windows, or no NLM entry found for this adapter's network
	Public,
	Private,
	DomainAuthenticated,
};

// Plain, non-translated label ("Unknown" / "Public" / "Private" / "Domain")
// - see IfTypeToString()'s comment below for why ndi-adapter-table-widget.cpp
// keeps its own tr()-based mapping instead of calling this for its
// on-screen label.
std::string NetworkCategoryToString(NdiNetworkCategory category);

// Whether a given port (mDNS 5353, or NDI's discovery/stream ports) is
// actually reachable through the Windows Firewall for a given
// profile/adapter - see GetFirewallStateByProfileType() in
// ndi-network-report.cpp for how this is computed. Only rules explicitly
// scoped to this process's own executable (i.e. the OBS Studio binary
// hosting this plugin) are consulted - a rule with no program restriction
// ("All Programs"/Any) is ignored, since it says nothing about whether
// *this* process specifically is let through. A rule's Action (Allow vs
// Block) and Direction (In vs Out) are both consulted, and an enabled
// Block rule always wins over an Allow rule covering the same
// port/protocol/profile/direction - matching how Windows Firewall itself
// prioritizes block over allow. Allowed requires BOTH directions to be
// open; if they disagree (one open, one not), or neither is open, the
// result is Blocked - a one-directional hole doesn't make a usable NDI
// connection. NotApplicable covers "networkCategory is Unknown" (non-
// Windows, or no NLM entry), and also "the Windows Firewall master switch
// itself is off for this profile" - in that case nothing is actually being
// filtered, so Allowed/Blocked wouldn't be a meaningful answer about rules.
enum class FirewallPortAccess { NotApplicable, Allowed, Blocked };

// Plain, non-translated label ("N/A" / "Allowed" / "Blocked") - see
// IfTypeToString()'s comment below for why ndi-adapter-table-widget.cpp
// keeps its own tr()-based mapping instead of calling this for its
// on-screen label.
std::string FirewallPortAccessToString(FirewallPortAccess access);

// Whether the Avahi mDNS/DNS-SD daemon (avahi-daemon) is running on this
// machine. On Linux, avahi-daemon is normally what actually answers mDNS
// queries on port 5353 - the OS itself doesn't bundle an mDNS responder
// the way Windows and macOS do, so if it's not running, NDI discovery can
// fail (or only work one-directionally) even when every firewall/adapter
// check above looks fine. This is a distinct signal from
// NdiNetworkReport::mdnsPortInUse below: that only reports whether
// *something* is bound to 5353, not whether it's Avahi specifically or a
// short-lived per-process responder that goes away when its host process
// exits.
//
// NotApplicable on Windows (has its own always-running mDNS responder as
// part of the OS - see mdnsPortInUse for the closest cross-platform check
// there) and on macOS (mDNSResponder is a launchd-managed system service,
// not Avahi, and isn't meaningfully "off" in normal operation - see
// GetAvahiStatus() in ndi-network-report.cpp).
enum class AvahiStatus { NotApplicable, Running, NotRunning };

// Plain, non-translated label ("N/A" / "Yes" / "No").
std::string AvahiStatusToString(AvahiStatus status);

struct NicSample {
	ULONG64 inOctets;
	ULONG64 outOctets;
	ULONG64 timestampMs;
	ULONG64 transmitSpeed;
	ULONG64 receiveSpeed;
};

struct NdiAdapterInfo {
	std::string friendlyName;
	std::string description;
	std::string ipv4Address;
	std::string subnetMask;
	IF_LUID luid;
	NicSample lastSample = {0, 0, 0, 0, 0};
	double inBps = 0.0;
	double outBps = 0.0;
	double receiveSpeed = 0.0;
	double transmitSpeed = 0.0;
	bool isUp = false;
	bool supportsMulticast =
		false; // Static capability (IP_ADAPTER_NO_MULTICAST) - NOT a live test; see multicastJoinOk below
	bool looksVirtual = false; // VPN / Hyper-V / VMware / loopback / Teredo etc.
	ULONG ifType = 0;
	bool ndiCandidate;
	NdiNetworkCategory networkCategory = NdiNetworkCategory::Unknown;

	// Per-adapter Windows Firewall diagnostics, joined from this adapter's
	// networkCategory above against the per-profile-type rule evaluation
	// done once in GetFirewallStateByProfileType() (ndi-network-report.cpp).
	// Windows Firewall rules are scoped by network profile (Domain/Private/
	// Public), not by adapter, so a single machine with adapters in
	// different profiles can have one adapter's traffic wide open and
	// another's silently blocked - these fields let the UI show that per
	// adapter instead of one misleading machine-wide answer.
	// firewallEnabled stays at its default (false), and mdnsPortOpen/
	// ndiPortsOpen/filePrinterSharing stay at their default
	// (NotApplicable), when networkCategory is Unknown (non-Windows, or no
	// NLM entry for this adapter's network) - "can't confirm", not
	// "off"/"closed".
	bool firewallEnabled = false;
	FirewallPortAccess mdnsPortOpen = FirewallPortAccess::NotApplicable;
	FirewallPortAccess ndiPortsOpen = FirewallPortAccess::NotApplicable;

	// Whether Windows' built-in "File and Printer Sharing" rule group
	// (SMB/NetBIOS plus the ICMPv4 Echo Request rule it also bundles) is
	// actively letting inbound traffic through for this adapter's own
	// profile. Unlike mdnsPortOpen/ndiPortsOpen above, this deliberately
	// does NOT require the matching rule to be scoped to this process's
	// own executable - Windows' built-in rules in this group have no
	// ApplicationName restriction at all (it's a generic OS feature, not
	// something NDI/OBS-specific), so requiring an exe match would always
	// exclude them. Only the inbound direction is consulted - the rule
	// group has no outbound rules, since "sharing" is about accepting
	// inbound requests, not something this machine needs outbound
	// permission for. See GetFirewallStateByProfileType() in
	// ndi-network-report.cpp.
	FirewallPortAccess filePrinterSharing = FirewallPortAccess::NotApplicable;

	// Whether THIS adapter could actually join the 224.0.0.251 mDNS
	// multicast group in the most recent DiagnoseNdiNetwork() pass - a
	// live functional test via IP_ADD_MEMBERSHIP with imr_interface set
	// to this adapter's own IPv4 address, not to be confused with
	// supportsMulticast above (a static capability flag - "can this NIC
	// do multicast at all" - that says nothing about whether IGMP/join
	// actually succeeds right now, e.g. due to IGMPLevel being disabled
	// system-wide, an upstream switch's IGMP snooping without a querier,
	// or the adapter simply not being on a network yet).
	//
	// This lives per-adapter rather than as a single NdiNetworkReport-wide
	// bool because IGMP group membership is inherently a per-interface
	// operation - a host with more than one active NIC can join
	// successfully on one and fail on another, and it's specifically the
	// adapter carrying NDI traffic that matters, not "did joining succeed
	// somewhere."
	bool multicastJoinOk = false;

	// Whether a real UDP packet sent to 224.0.0.251:5353 through THIS
	// adapter (via IP_MULTICAST_IF) was actually received back by this
	// process (via multicast loopback) within the test window. This is a
	// materially different, and stronger, check than multicastJoinOk
	// above: IP_ADD_MEMBERSHIP is a purely local syscall that the OS can
	// accept even while something operating *below* the socket API - most
	// notably a VPN client's WFP callout driver enforcing a "block LAN
	// traffic" / disabled split-tunnel setting - silently discards every
	// real packet in flight. That combination (multicastJoinOk true,
	// multicastRoundTripOk false) is the fingerprint of exactly that
	// condition: every local check looks fine, but no actual multicast
	// traffic is getting through on this adapter. See
	// TestMulticastRoundTrip() in ndi-network-report.cpp.
	//
	// Unlike networkCategory/avahiStatus above, this isn't an OS-specific
	// signal - it's the same plain send/recv/select socket sequence on
	// Windows, Linux, and macOS, so it's meaningful (and directly
	// comparable) on all three platforms.
	bool multicastRoundTripOk = false;

	ChangeNotifier *changeNotifier = nullptr;
};

struct NdiNetworkReport {
	std::vector<NdiAdapterInfo> adapters;
	bool mdnsSocketBindOk = false; // Could we open a UDP socket on 5353?
	bool mdnsPortInUse = false;    // Something (Bonjour/NDI Access Mgr) already owns 5353

	AvahiStatus avahiStatus = AvahiStatus::NotApplicable; // Linux only - see AvahiStatus above
};

bool GetSample(IF_LUID luid, NicSample &out);
void UpdateNicbps(std::vector<NdiAdapterInfo> &nics);
NdiNetworkReport DiagnoseNdiNetwork();
std::string FormatNdiNetworkReport(const NdiNetworkReport &report);

// Classifies a raw NdiAdapterInfo::ifType value into a short human-readable
// label ("Ethernet", "Wi-Fi", "Loopback", "Tunnel/VPN", "Other (N)", etc).
// Exposed here (rather than kept private to ndi-network-report.cpp) so any
// other translation unit that needs a plain, non-translated label - e.g.
// network-monitor.cpp's log dump - can reuse the one canonical mapping
// instead of maintaining its own copy of the per-platform switch statement.
//
// NOTE: ndi-adapter-table-widget.cpp intentionally does NOT call this for
// its on-screen column - it keeps its own tr()-based switch, because Qt's
// lupdate only extracts literal tr("...") arguments for translation, and
// funneling a runtime std::string through tr() would silently defeat that.
std::string IfTypeToString(ULONG ifType);
