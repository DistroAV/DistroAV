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

#include <cstdint>
#include <cstring> // for memcmp - used by TestMulticastRoundTrip()
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <cctype>
#include <iomanip> // for std::setw, std::left
#include <map>
#include <algorithm> // for std::any_of
#include <chrono>    // for TestMulticastRoundTrip()'s poll deadline
#include <random>    // for TestMulticastRoundTrip()'s probe token
#include <utility>   // for std::pair - g_multicastMemberships' key
#include <mutex>     // for g_multicastMembershipsMutex

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#include <netfw.h>
#include <comutil.h>
#include <netlistmgr.h> // INetworkListManager - per-adapter NetworkCategory (Get-NetConnectionProfile)

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

#else // Linux and macOS

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h> // select()/fd_set - used by TestMulticastRoundTrip()
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>
#include <cerrno>
#include <fstream>

#if defined(__linux__)
#include <net/if_arp.h> // ARPHRD_* constants
#include <dirent.h>     // opendir/readdir - used to scan /proc for avahi-daemon
#elif defined(__APPLE__)
#include <net/if_dl.h>    // sockaddr_dl
#include <net/if_types.h> // IFT_* constants
#include <net/route.h>    // if_msghdr2, RTM_IFINFO2
#include <sys/sysctl.h>
#include <mach-o/dyld.h> // _NSGetExecutablePath
#include <climits>       // PATH_MAX
#include <cstdio>        // popen/pclose
#include <cstdlib>       // realpath
#endif

#endif // _WIN32

#include "ndi-network-report.h"
// kLinuxSyntheticWifiType now lives in ndi-network-report.h (shared with
// ndi-adapter-table-widget.cpp, which needs the same sentinel to label the
// Type column correctly).

#ifdef _WIN32

// ---------------------------------------------------------------------------
// NDI's documented port usage (docs.ndi.video/all/getting-started/white-paper/
// ndi-related-network-ports): UDP 5353 for mDNS discovery; TCP 5959 for the
// optional NDI Discovery Server; TCP 5960 for the "query this machine's
// sources by IP" lookup; and TCP (or, on NDI 5+ Reliable UDP, UDP too) 5961
// and up, one port per active stream. NDI's own guidance is that a handful
// of ports above 5960 is normally enough for a single machine, with some
// margin for safety - we treat 5960-6000 as "the range a firewall rule
// needs to cover" for that reason, rather than trying to track exactly how
// many streams are active. Only used on Windows: this is where we can
// actually enumerate firewall rules to check against it (see the
// platform-limitations note at the bottom of this file for why the same
// isn't done on Linux/macOS).
constexpr int kNdiMdnsPort = 5353;             // UDP
constexpr int kNdiDiscoveryServerPort = 5959;  // TCP
constexpr int kNdiStreamPortRangeStart = 5960; // TCP, and UDP for Reliable UDP
constexpr int kNdiStreamPortRangeEnd = 6000;   // TCP, and UDP for Reliable UDP

// Full path of the executable hosting this process (i.e. OBS Studio's
// obs64.exe, not this plugin's DLL - firewall rules are per-process, and
// the process is what OBS Studio launched). Returns an empty string on
// failure so callers can skip the comparison rather than matching garbage.
static std::wstring GetCurrentExecutablePathW()
{
	std::wstring path(MAX_PATH, L'\0');
	for (;;) {
		DWORD len = GetModuleFileNameW(nullptr, &path[0], (DWORD)path.size());
		if (len == 0)
			return std::wstring();
		if (len < path.size()) {
			path.resize(len);
			return path;
		}
		// Buffer was too small (e.g. a deeply nested install path); grow
		// and retry rather than silently returning a truncated path.
		path.resize(path.size() * 2);
	}
}

// Windows Firewall rule LocalPorts strings look like "5960-5969", "5353",
// "80,443,5960-5990", or "*" (any port). Returns whether `port` falls
// inside that spec.
static bool PortSpecCoversPort(const std::wstring &portsSpec, int port)
{
	if (portsSpec == L"*")
		return true;

	size_t pos = 0;
	while (pos <= portsSpec.size()) {
		size_t comma = portsSpec.find(L',', pos);
		std::wstring token =
			portsSpec.substr(pos, comma == std::wstring::npos ? std::wstring::npos : comma - pos);

		size_t first = token.find_first_not_of(L" \t");
		size_t last = token.find_last_not_of(L" \t");
		if (first != std::wstring::npos) {
			token = token.substr(first, last - first + 1);

			size_t dash = token.find(L'-');
			if (dash != std::wstring::npos) {
				int lo = _wtoi(token.substr(0, dash).c_str());
				int hi = _wtoi(token.substr(dash + 1).c_str());
				if (port >= lo && port <= hi)
					return true;
			} else if (_wtoi(token.c_str()) == port) {
				return true;
			}
		}

		if (comma == std::wstring::npos)
			break;
		pos = comma + 1;
	}
	return false;
}

// Does an enabled, inbound, allow rule with this protocol/LocalPorts cover
// any of the ports NDI needs? Only checks the protocol(s) the relevant NDI
// port actually uses (UDP for mDNS, TCP for discovery/stream ports - see
// the kNdi* constants above).
static bool RuleCoversNdiPorts(long protocol, const std::wstring &localPorts)
{
	const bool tcp = (protocol == NET_FW_IP_PROTOCOL_TCP || protocol == NET_FW_IP_PROTOCOL_ANY);
	const bool udp = (protocol == NET_FW_IP_PROTOCOL_UDP || protocol == NET_FW_IP_PROTOCOL_ANY);

	if (udp && PortSpecCoversPort(localPorts, kNdiMdnsPort))
		return true;

	if (tcp) {
		if (PortSpecCoversPort(localPorts, kNdiDiscoveryServerPort))
			return true;
		for (int port = kNdiStreamPortRangeStart; port <= kNdiStreamPortRangeEnd; ++port) {
			if (PortSpecCoversPort(localPorts, port))
				return true;
		}
	}
	return false;
}

// Per-profile-type Windows Firewall state, as evaluated by
// GetFirewallStateByProfileType() below - see that function's comment for
// why this is computed once per profile type rather than once per adapter.
struct PerProfileFirewallState {
	bool firewallEnabled = false;
	FirewallPortAccess mdnsPortOpen = FirewallPortAccess::NotApplicable;
	FirewallPortAccess ndiPortsOpen = FirewallPortAccess::NotApplicable;
	FirewallPortAccess filePrinterSharing = FirewallPortAccess::NotApplicable;
};

// Accumulates, for a single port (mDNS or NDI) and a single profile type,
// whether an enabled Allow/Block rule was seen covering that port in each
// direction - see PortAccessFromAccumulator() below for how this collapses
// into the final FirewallPortAccess. Local to GetFirewallStateByProfileType()
// - a rule set is scanned once per call, so this doesn't need to persist
// across calls the way the NLM category map does.
struct PortRuleAccumulator {
	bool inAllowed = false;
	bool outAllowed = false;
	bool inBlocked = false;
	bool outBlocked = false;
};

// An enabled Block rule always wins over an Allow rule covering the same
// port/protocol/profile/direction, matching how Windows Firewall itself
// resolves conflicting rules (block takes priority regardless of rule
// specificity, with the sole exception of authenticated-bypass rules this
// codebase doesn't attempt to model). Allowed requires BOTH directions to
// be open - a one-directional hole (e.g. outbound open, inbound not) isn't
// a usable NDI connection, so it reports Blocked just like a fully-closed
// port would. NotApplicable when the Windows Firewall master switch itself
// is off for this profile - with nothing being filtered, "Allowed"/
// "Blocked" wouldn't be a meaningful answer about rules at all.
static FirewallPortAccess PortAccessFromAccumulator(bool firewallEnabledForProfile, const PortRuleAccumulator &acc)
{
	if (!firewallEnabledForProfile)
		return FirewallPortAccess::NotApplicable;

	const bool inOpen = acc.inAllowed && !acc.inBlocked;
	const bool outOpen = acc.outAllowed && !acc.outBlocked;
	return (inOpen && outOpen) ? FirewallPortAccess::Allowed : FirewallPortAccess::Blocked;
}

// Same block-wins-over-allow precedence as PortAccessFromAccumulator()
// above, but only the inbound direction is consulted - used for File &
// Printer Sharing, whose built-in rule group has no outbound rules at all
// (see NdiAdapterInfo::filePrinterSharing), so requiring both directions
// to agree would make it permanently report Blocked.
static FirewallPortAccess InboundPortAccessFromAccumulator(bool firewallEnabledForProfile,
							   const PortRuleAccumulator &acc)
{
	if (!firewallEnabledForProfile)
		return FirewallPortAccess::NotApplicable;

	const bool inOpen = acc.inAllowed && !acc.inBlocked;
	return inOpen ? FirewallPortAccess::Allowed : FirewallPortAccess::Blocked;
}

// Maps NdiNetworkCategory (this codebase's portable enum, sourced from NLM -
// see GetNetworkCategoriesByAdapterGuid()) to NET_FW_PROFILE_TYPE2 (Windows
// Firewall's own profile-type enum, from netfw.h) so an adapter's already-
// known category can be used to look itself up in the per-profile-type map
// GetFirewallStateByProfileType() returns. Returns false (and leaves
// profileType unset) for NdiNetworkCategory::Unknown - there's no firewall
// profile to map an unknown network to.
static bool TryMapCategoryToProfileType(NdiNetworkCategory category, NET_FW_PROFILE_TYPE2 &profileType)
{
	switch (category) {
	case NdiNetworkCategory::DomainAuthenticated:
		profileType = NET_FW_PROFILE2_DOMAIN;
		return true;
	case NdiNetworkCategory::Private:
		profileType = NET_FW_PROFILE2_PRIVATE;
		return true;
	case NdiNetworkCategory::Public:
		profileType = NET_FW_PROFILE2_PUBLIC;
		return true;
	case NdiNetworkCategory::Unknown:
	default:
		return false;
	}
}

// Enumerates the full Windows Firewall rule set once and evaluates it
// against each of the three profile types (Domain/Private/Public)
// individually, rather than against a single machine-wide "currently
// active" bitmask. Windows Firewall rules scope by profile
// (INetFwRule::Profiles), not by adapter, so this - one shared pass, joined
// per-adapter via TryMapCategoryToProfileType() and each adapter's own
// independently-sourced networkCategory - is both cheaper and structurally
// correct compared to re-enumerating rules once per adapter. This mirrors
// the same pattern GetNetworkCategoriesByAdapterGuid() already established
// for network categories.
//
// Deliberately does NOT gate on INetFwPolicy2::get_CurrentProfileTypes() -
// that API was observed to sometimes omit a profile that was actually
// active. Instead every profile type is evaluated unconditionally; a
// profile type with no adapter currently on it just means its entry in the
// returned map goes unused.
static std::map<NET_FW_PROFILE_TYPE2, PerProfileFirewallState> GetFirewallStateByProfileType()
{
	std::map<NET_FW_PROFILE_TYPE2, PerProfileFirewallState> result;

	const std::wstring exePath = GetCurrentExecutablePathW();

	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	bool uninit = SUCCEEDED(hr);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
		return result;

	INetFwPolicy2 *policy = nullptr;
	hr = CoCreateInstance(__uuidof(NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER, __uuidof(INetFwPolicy2),
			      (void **)&policy);
	if (FAILED(hr)) {
		if (uninit)
			CoUninitialize();
		return result;
	}

	const NET_FW_PROFILE_TYPE2 profileTypes[] = {NET_FW_PROFILE2_DOMAIN, NET_FW_PROFILE2_PRIVATE,
						     NET_FW_PROFILE2_PUBLIC};

	for (NET_FW_PROFILE_TYPE2 profileType : profileTypes) {
		PerProfileFirewallState state;

		VARIANT_BOOL profileFirewallEnabled = VARIANT_TRUE;
		if (SUCCEEDED(policy->get_FirewallEnabled(profileType, &profileFirewallEnabled)))
			state.firewallEnabled = (profileFirewallEnabled == VARIANT_TRUE);

		result[profileType] = state;
	}

	//----------------------------------------------------------
	// Enumerate rules once, evaluate against every profile type
	//----------------------------------------------------------

	INetFwRules *rules = nullptr;
	if (FAILED(policy->get_Rules(&rules))) {
		policy->Release();
		if (uninit)
			CoUninitialize();
		return result;
	}

	IUnknown *unk = nullptr;
	rules->get__NewEnum(&unk);

	IEnumVARIANT *e = nullptr;
	unk->QueryInterface(IID_IEnumVARIANT, (void **)&e);

	VARIANT var;
	VariantInit(&var);

	// Per-profile-type, per-port accumulators - see PortRuleAccumulator's
	// comment. Both Allow and Block rules, in both directions, are folded
	// in here as they're encountered; the final FirewallPortAccess per
	// profile is only derived once the whole rule set has been scanned
	// (PortAccessFromAccumulator(), after the loop below). fileSharingAcc
	// only ever has its inbound fields populated - see
	// InboundPortAccessFromAccumulator().
	std::map<NET_FW_PROFILE_TYPE2, PortRuleAccumulator> mdnsAcc, ndiAcc, fileSharingAcc;

	while (e->Next(1, &var, nullptr) == S_OK) {
		INetFwRule *rule = nullptr;

		if (FAILED(var.pdispVal->QueryInterface(__uuidof(INetFwRule), (void **)&rule))) {
			VariantClear(&var);
			continue;
		}

		VARIANT_BOOL enabled;
		rule->get_Enabled(&enabled);
		if (enabled != VARIANT_TRUE) {
			rule->Release();
			VariantClear(&var);
			continue;
		}

		// Unlike the old machine-wide check, both directions and both
		// actions are considered here (not just inbound/allow) - a rule
		// can legitimately open or block either direction, and an
		// enabled Block always overrides a matching Allow (see
		// PortAccessFromAccumulator()).
		NET_FW_RULE_DIRECTION direction;
		NET_FW_ACTION action;
		rule->get_Direction(&direction);
		rule->get_Action(&action);

		long ruleProfiles = 0;
		rule->get_Profiles(&ruleProfiles);

		//------------------------------------------
		// File & Printer Sharing: unlike the mDNS/NDI port checks below,
		// this is a generic Windows feature toggle, not process-specific -
		// the built-in rules in this group have no ApplicationName
		// restriction at all, so this deliberately does NOT gate on
		// programMatches. Only Direction=In is consulted, since the group
		// has no outbound rules (see NdiAdapterInfo::filePrinterSharing).
		//------------------------------------------

		if (direction == NET_FW_RULE_DIR_IN) {
			BSTR grouping = nullptr;
			if (SUCCEEDED(rule->get_Grouping(&grouping))) {
				if (grouping && wcscmp(grouping, L"@FirewallAPI.dll,-28502") == 0) {
					const bool isBlock = (action == NET_FW_ACTION_BLOCK);
					for (NET_FW_PROFILE_TYPE2 profileType : profileTypes) {
						if (ruleProfiles != NET_FW_PROFILE2_ALL &&
						    (ruleProfiles & profileType) == 0)
							continue;
						PortRuleAccumulator &acc = fileSharingAcc[profileType];
						acc.inAllowed |= !isBlock;
						acc.inBlocked |= isBlock;
					}
				}
				SysFreeString(grouping);
			}
		}

		//------------------------------------------
		// A rule only counts toward mDNS/NDI port state if it's scoped
		// explicitly to this process's own executable (i.e. the OBS Studio
		// binary hosting this plugin) by ApplicationName. Unlike D1's
		// original design, a rule with NO ApplicationName restriction
		// ("All Programs"/Any) does NOT count, even though it would in
		// practice also let this process's traffic through - real-world
		// testing turned up "All Programs, Any protocol" rules (e.g.
		// auto-generated by Hyper-V/WSL/Docker for a virtual switch that
		// happens to share the same network profile) that made the report
		// claim ports were open when nothing OBS-specific was actually
		// granting that access. Scoping strictly to this exe trades
		// away crediting those broader rules in exchange for the report
		// only ever reflecting rules that were actually written for this
		// plugin.
		//------------------------------------------

		bool programMatches = false;
		BSTR appName = nullptr;
		if (SUCCEEDED(rule->get_ApplicationName(&appName))) {
			if (appName && *appName != L'\0' && !exePath.empty()) {
				programMatches = (_wcsicmp(appName, exePath.c_str()) == 0);
			}
			SysFreeString(appName);
		}

		long portProtocol = 0;
		rule->get_Protocol(&portProtocol);
		const bool udp = (portProtocol == NET_FW_IP_PROTOCOL_UDP || portProtocol == NET_FW_IP_PROTOCOL_ANY);

		BSTR localPorts = nullptr;
		bool ruleCoversNdiPorts = false;
		bool ruleCoversMdnsPort = false;
		if (programMatches && portProtocol == NET_FW_IP_PROTOCOL_ANY) {
			// LocalPorts is only a meaningful, queryable property when a
			// rule's Protocol is explicitly TCP or UDP - Windows Firewall
			// doesn't track a port restriction for an Any-protocol rule at
			// all (its own GUI greys out port selection once "Any" is
			// picked), so get_LocalPorts() below returns nothing usable
			// for one. An Any-protocol rule has no port restriction by
			// construction, so it covers every port unconditionally
			// rather than needing LocalPorts consulted at all.
			ruleCoversNdiPorts = true;
			ruleCoversMdnsPort = true;
		} else if (programMatches && SUCCEEDED(rule->get_LocalPorts(&localPorts))) {
			if (localPorts) {
				ruleCoversNdiPorts = RuleCoversNdiPorts(portProtocol, localPorts);
				ruleCoversMdnsPort = udp && PortSpecCoversPort(localPorts, kNdiMdnsPort);
			}
			SysFreeString(localPorts);
		}

		if (ruleCoversNdiPorts || ruleCoversMdnsPort) {
			const bool isOut = (direction == NET_FW_RULE_DIR_OUT);
			const bool isBlock = (action == NET_FW_ACTION_BLOCK);

			for (NET_FW_PROFILE_TYPE2 profileType : profileTypes) {
				if (ruleProfiles != NET_FW_PROFILE2_ALL && (ruleProfiles & profileType) == 0)
					continue;

				if (ruleCoversNdiPorts) {
					PortRuleAccumulator &acc = ndiAcc[profileType];
					if (isOut) {
						acc.outAllowed |= !isBlock;
						acc.outBlocked |= isBlock;
					} else {
						acc.inAllowed |= !isBlock;
						acc.inBlocked |= isBlock;
					}
				}
				if (ruleCoversMdnsPort) {
					PortRuleAccumulator &acc = mdnsAcc[profileType];
					if (isOut) {
						acc.outAllowed |= !isBlock;
						acc.outBlocked |= isBlock;
					} else {
						acc.inAllowed |= !isBlock;
						acc.inBlocked |= isBlock;
					}
				}
			}
		}

		rule->Release();
		VariantClear(&var);
	}

	e->Release();
	unk->Release();
	rules->Release();
	policy->Release();

	for (NET_FW_PROFILE_TYPE2 profileType : profileTypes) {
		const bool fwEnabled = result[profileType].firewallEnabled;
		result[profileType].mdnsPortOpen = PortAccessFromAccumulator(fwEnabled, mdnsAcc[profileType]);
		result[profileType].ndiPortsOpen = PortAccessFromAccumulator(fwEnabled, ndiAcc[profileType]);
		result[profileType].filePrinterSharing =
			InboundPortAccessFromAccumulator(fwEnabled, fileSharingAcc[profileType]);
	}

	if (uninit)
		CoUninitialize();

	return result;
}

#else // Linux and macOS

// There is no single, unprivileged, cross-distro/version API equivalent to
// Windows' INetFwPolicy2 on Linux (iptables/nftables/firewalld/ufw all
// differ, and rule enumeration generally requires root) or on macOS (pf, via
// pfctl, also requires root). We deliberately do NOT shell out to
// iptables/pfctl here: for most non-root users it would silently fail or
// misreport, and parsing rule output is fragile across versions/distros.
// See the "platform limitations" note at the bottom of this file.

#if defined(__linux__)
// Best-effort, unprivileged check for whether avahi-daemon is currently
// running, by scanning /proc for a process whose comm name is exactly
// "avahi-daemon" - the same technique ps/pgrep use under the hood,
// without shelling out to either. This file deliberately avoids invoking
// external tools on Linux for the same reason documented on
// LinuxUfwReportsDisabled() above: init-system differences (systemd vs.
// sysvinit vs. OpenRC/runit) make a "systemctl is-active avahi-daemon"
// check unreliable across distros/containers, whereas /proc/<pid>/comm is
// a stable kernel interface everywhere Linux runs. /proc/<pid>/comm is
// truncated to 15 characters by the kernel, but "avahi-daemon" is exactly
// 12, so there's no truncation ambiguity to worry about here.
//
// This deliberately does NOT distinguish "confirmed not running" from
// "couldn't read /proc" - both report false. An unprivileged process
// being unable to read /proc's own PID listing doesn't happen on any
// mainstream distro; if it somehow does, treating that the same as "not
// running" is the safer direction for a diagnostic warning about missing
// mDNS support (matching the "narrow positive signal or don't guess"
// approach used throughout this file).
static bool IsAvahiDaemonRunningLinux()
{
	DIR *procDir = opendir("/proc");
	if (!procDir)
		return false;

	bool found = false;
	struct dirent *entry;
	while ((entry = readdir(procDir)) != nullptr) {
		const char *name = entry->d_name;

		// Only interested in numeric (PID) directory entries.
		bool allDigits = (name[0] != '\0');
		for (const char *c = name; *c; ++c) {
			if (!isdigit(static_cast<unsigned char>(*c))) {
				allDigits = false;
				break;
			}
		}
		if (!allDigits)
			continue;

		std::ifstream commFile(std::string("/proc/") + name + "/comm");
		if (!commFile.good())
			continue;

		std::string comm;
		std::getline(commFile, comm);
		while (!comm.empty() && (comm.back() == '\n' || comm.back() == '\r'))
			comm.pop_back();

		if (comm == "avahi-daemon") {
			found = true;
			break;
		}
	}
	closedir(procDir);
	return found;
}
#endif

#endif // _WIN32

static bool StringContainsAny(const std::string &hay, std::initializer_list<const char *> needles)
{
	for (auto n : needles)
		if (hay.find(n) != std::string::npos)
			return true;
	return false;
}

// Heuristic: guess whether an adapter is "virtual" (VPN, container, hypervisor, etc.)
static bool LooksVirtual(const std::string &desc, ULONG ifType)
{
#if defined(_WIN32)
	if (ifType == IF_TYPE_TUNNEL)
		return true;
	if (ifType == IF_TYPE_SOFTWARE_LOOPBACK)
		return true;
#elif defined(__linux__)
	if (ifType == ARPHRD_LOOPBACK)
		return true;
	if (ifType == ARPHRD_TUNNEL || ifType == ARPHRD_TUNNEL6 || ifType == ARPHRD_SIT || ifType == ARPHRD_IPGRE ||
	    ifType == ARPHRD_NONE) // ARPHRD_NONE: common for WireGuard, some VPNs
		return true;
#elif defined(__APPLE__)
	if (ifType == IFT_LOOP)
		return true;
	if (ifType == IFT_GIF || ifType == IFT_STF) // generic + 6to4 tunnel interfaces
		return true;
#endif

	std::string lower = desc;
	for (auto &c : lower)
		c = (char)tolower((unsigned char)c);

#if defined(_WIN32)
	return StringContainsAny(lower, {"virtual", "vmware", "hyper-v", "hyperv", "vpn", "tap-windows", "tailscale",
					 "wireguard", "loopback", "teredo", "vethernet", "npcap", "parsec"});
#else
	// On Linux/macOS `desc` is the raw interface name (see
	// EnumerateAdapters() below - there's no separate vendor description
	// string like Windows provides), so we match against the naming
	// conventions those platforms actually use for virtual/tunnel/
	// container adapters.
	return StringContainsAny(lower, {"virtual",   "vmware", "vbox",     "vnic", "vmnet",  "vpn",    "tailscale",
					 "wireguard", "wg",     "loopback", "veth", "docker", "bridge", "br-",
					 "podman",    "cni",    "flannel",  "utun", "tun",    "tap",    "ppp",
					 "gif",       "stf",    "awdl",     "llw",  "teredo"});
#endif
}

// Declared in ndi-network-report.h (non-static) so network-monitor.cpp's log
// dump can reuse this instead of keeping its own duplicate switch statement.
std::string IfTypeToString(ULONG ifType)
{
#if defined(_WIN32)
	switch (ifType) {
	case IF_TYPE_ETHERNET_CSMACD:
		return "Ethernet";
	case IF_TYPE_IEEE80211:
		return "Wi-Fi";
	case IF_TYPE_TUNNEL:
		return "Tunnel/VPN";
	case IF_TYPE_SOFTWARE_LOOPBACK:
		return "Loopback";
	case IF_TYPE_PPP:
		return "PPP";
	case IF_TYPE_IEEE1394:
		return "FireWire";
	default:
		return "Other (" + std::to_string(ifType) + ")";
	}
#elif defined(__linux__)
	if (ifType == kLinuxSyntheticWifiType)
		return "Wi-Fi";
	switch (ifType) {
	case ARPHRD_ETHER:
		return "Ethernet";
	case ARPHRD_IEEE80211:
		return "Wi-Fi"; // monitor-mode interfaces; managed-mode Wi-Fi is caught above
	case ARPHRD_LOOPBACK:
		return "Loopback";
	case ARPHRD_TUNNEL:
	case ARPHRD_TUNNEL6:
	case ARPHRD_SIT:
	case ARPHRD_IPGRE:
	case ARPHRD_NONE:
		return "Tunnel/VPN";
	case ARPHRD_PPP:
		return "PPP";
	case ARPHRD_IEEE1394:
		return "FireWire";
	default:
		return "Other (" + std::to_string(ifType) + ")";
	}
#elif defined(__APPLE__)
	switch (ifType) {
	case IFT_ETHER:
		// macOS reports Wi-Fi adapters as IFT_ETHER too (802.11
		// emulates Ethernet at this layer), so this lightweight check
		// can't tell wired and wireless apart the way Windows'
		// IF_TYPE_IEEE80211 does. Doing so reliably needs
		// SystemConfiguration.framework's SCNetworkInterfaceGetInterfaceType(),
		// which this file intentionally doesn't link against. See the
		// platform-limitations note at the bottom of this file.
		return "Ethernet/Wi-Fi";
	case IFT_LOOP:
		return "Loopback";
	case IFT_GIF:
	case IFT_STF:
		return "Tunnel/VPN";
	case IFT_PPP:
		return "PPP";
	case IFT_IEEE1394:
		return "FireWire";
	default:
		return "Other (" + std::to_string(ifType) + ")";
	}
#endif
}

std::string NetworkCategoryToString(NdiNetworkCategory category)
{
	switch (category) {
	case NdiNetworkCategory::Public:
		return "Public";
	case NdiNetworkCategory::Private:
		return "Private";
	case NdiNetworkCategory::DomainAuthenticated:
		return "Domain";
	case NdiNetworkCategory::Unknown:
	default:
		return "Unknown";
	}
}

std::string FirewallPortAccessToString(FirewallPortAccess access)
{
	switch (access) {
	case FirewallPortAccess::Allowed:
		return "Allowed";
	case FirewallPortAccess::Blocked:
		return "Blocked";
	case FirewallPortAccess::NotApplicable:
	default:
		return "N/A";
	}
}

std::string AvahiStatusToString(AvahiStatus status)
{
	switch (status) {
	case AvahiStatus::Running:
		return "Yes";
	case AvahiStatus::NotRunning:
		return "No";
	case AvahiStatus::NotApplicable:
	default:
		return "N/A";
	}
}

// Declared static (not in the header) since, unlike IfTypeToString()/
// NetworkCategoryToString() above, nothing outside this file currently
// needs to query Avahi status on its own -
// DiagnoseNdiNetwork() below is the only caller. IsAvahiDaemonRunningLinux()
// is only compiled in (and only in scope) when __linux__ is defined - see
// the #if defined(__linux__) block earlier in this file - so this stays
// behind the same guard rather than calling something that wouldn't exist
// in a non-Linux build.
#if defined(__linux__)
static AvahiStatus GetAvahiStatus()
{
	return IsAvahiDaemonRunningLinux() ? AvahiStatus::Running : AvahiStatus::NotRunning;
}
#else
static AvahiStatus GetAvahiStatus()
{
	return AvahiStatus::NotApplicable;
}
#endif

// Helper: truncate strings that are longer than maxLen, adding ellipsis
static std::string truncate_for_column(const std::string &s, size_t maxLen)
{
	if (s.size() <= maxLen)
		return s;
	if (maxLen <= 3)
		return s.substr(0, maxLen);
	return s.substr(0, maxLen - 3) + "...";
}

#ifdef _WIN32

// Uppercase "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}" form of a GUID, used
// as a map key below. Windows' own formatting (StringFromGUID2) and the
// AdapterName string GetAdaptersAddresses() hands us are both already in
// this form in practice, but we canonicalize both sides through the same
// parse/reformat round-trip rather than relying on that - a stray casing
// difference between the two APIs would otherwise cause every adapter to
// silently miss its category rather than failing loudly.
static std::string CanonicalGuidString(const GUID &guid)
{
	wchar_t buf[64] = {};
	StringFromGUID2(guid, buf, static_cast<int>(sizeof(buf) / sizeof(buf[0])));
	std::string s = utf8_from_wstring(buf);
	for (auto &c : s)
		c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
	return s;
}

// Queries the Network List Manager (NLM) for the NLM_NETWORK_CATEGORY of
// every currently-connected network, keyed by adapter GUID - the same
// data PowerShell's `Get-NetConnectionProfile` surfaces per-interface.
// Unlike INetFwPolicy2 (queried once for the whole process in
// GetFirewallStateByProfileType() above), NLM naturally enumerates *per
// adapter/connection*, so this is a single pass covering every adapter
// rather than something we'd want to re-query once per NIC.
//
// An adapter with no entry in the returned map (e.g. unplugged, or not
// yet associated with a network) should be treated as
// NdiNetworkCategory::Unknown by the caller - absence here is "can't
// confirm", not "off"/"Public", matching the "narrow positive signal or
// don't guess" approach the rest of this file's diagnostics use.
static std::map<std::string, NdiNetworkCategory> GetNetworkCategoriesByAdapterGuid()
{
	std::map<std::string, NdiNetworkCategory> result;

	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	// RPC_E_CHANGED_MODE just means some other code on this thread (e.g.
	// Qt, or GetFirewallStateByProfileType() itself if a future refactor
	// moves these calls onto the same thread) already initialized COM with
	// a different apartment model - COM is still usable, we just don't own
	// tearing it down.
	bool uninit = SUCCEEDED(hr);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
		return result;

	INetworkListManager *nlm = nullptr;
	if (FAILED(CoCreateInstance(CLSID_NetworkListManager, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&nlm))) || !nlm) {
		if (uninit)
			CoUninitialize();
		return result;
	}

	IEnumNetworkConnections *connEnum = nullptr;
	if (SUCCEEDED(nlm->GetNetworkConnections(&connEnum)) && connEnum) {
		INetworkConnection *conn = nullptr;
		ULONG fetched = 0;
		while (connEnum->Next(1, &conn, &fetched) == S_OK && fetched == 1) {
			GUID adapterId{};
			if (SUCCEEDED(conn->GetAdapterId(&adapterId))) {
				INetwork *network = nullptr;
				if (SUCCEEDED(conn->GetNetwork(&network)) && network) {
					NLM_NETWORK_CATEGORY cat;
					if (SUCCEEDED(network->GetCategory(&cat))) {
						NdiNetworkCategory portable = NdiNetworkCategory::Unknown;
						switch (cat) {
						case NLM_NETWORK_CATEGORY_PUBLIC:
							portable = NdiNetworkCategory::Public;
							break;
						case NLM_NETWORK_CATEGORY_PRIVATE:
							portable = NdiNetworkCategory::Private;
							break;
						case NLM_NETWORK_CATEGORY_DOMAIN_AUTHENTICATED:
							portable = NdiNetworkCategory::DomainAuthenticated;
							break;
						}
						result[CanonicalGuidString(adapterId)] = portable;
					}
					network->Release();
				}
			}
			conn->Release();
		}
		connEnum->Release();
	}

	nlm->Release();
	if (uninit)
		CoUninitialize();
	return result;
}

// Enumerate IPv4 adapters using GetAdaptersAddresses.
static std::vector<NdiAdapterInfo> EnumerateAdapters()
{
	std::vector<NdiAdapterInfo> result;

	ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
	ULONG bufLen = 15000;
	std::vector<BYTE> buffer(bufLen);
	PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());

	ULONG ret = GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &bufLen);
	if (ret == ERROR_BUFFER_OVERFLOW) {
		buffer.resize(bufLen);
		pAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
		ret = GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &bufLen);
	}
	if (ret != NO_ERROR)
		return result;

	// One NLM pass covers every adapter (see
	// GetNetworkCategoriesByAdapterGuid()'s comment above), so this stays
	// outside the per-adapter loop below rather than re-querying NLM once
	// per NIC. Same idea for the per-profile-type firewall state below -
	// see GetFirewallStateByProfileType()'s comment.
	const auto categoriesByAdapterGuid = GetNetworkCategoriesByAdapterGuid();
	const auto firewallStateByProfile = GetFirewallStateByProfileType();

	for (PIP_ADAPTER_ADDRESSES cur = pAddresses; cur != nullptr; cur = cur->Next) {
		std::wstring wFriendly(cur->FriendlyName ? cur->FriendlyName : L"");
		std::wstring wDesc(cur->Description ? cur->Description : L"");
		std::string friendlyName = utf8_from_wstring(wFriendly);
		std::string description = utf8_from_wstring(wDesc);

		bool up = (cur->OperStatus == IfOperStatusUp);

		// AdapterName is the adapter's GUID as an ANSI string (e.g.
		// "{4D36E972-...}") - the same identity NLM's
		// INetworkConnection::GetAdapterId() reports, just in a
		// different type/format. Parsed once per adapter (not once per
		// unicast address below) since it's the same adapter either
		// way. An adapter absent from the map (e.g. not currently
		// associated with any network) resolves to Unknown, matching
		// the "don't guess" convention used elsewhere in this file.
		NdiNetworkCategory networkCategory = NdiNetworkCategory::Unknown;
		if (cur->AdapterName) {
			std::string narrowGuid(cur->AdapterName);
			std::wstring wideGuid(narrowGuid.begin(), narrowGuid.end());
			GUID adapterGuid{};
			if (SUCCEEDED(IIDFromString(wideGuid.c_str(), &adapterGuid))) {
				auto it = categoriesByAdapterGuid.find(CanonicalGuidString(adapterGuid));
				if (it != categoriesByAdapterGuid.end())
					networkCategory = it->second;
			}
		}

		// Joins this adapter's already-known networkCategory against the
		// per-profile-type firewall state computed once above - see
		// GetFirewallStateByProfileType()'s comment for why rules are
		// evaluated per profile type rather than per adapter. Stays at
		// its default (firewallEnabled false, ports/sharing NotApplicable)
		// when the category can't be mapped to a firewall profile (i.e.
		// Unknown).
		bool firewallEnabled = false;
		FirewallPortAccess mdnsPortOpen = FirewallPortAccess::NotApplicable;
		FirewallPortAccess ndiPortsOpen = FirewallPortAccess::NotApplicable;
		FirewallPortAccess filePrinterSharing = FirewallPortAccess::NotApplicable;
		NET_FW_PROFILE_TYPE2 profileType;
		if (TryMapCategoryToProfileType(networkCategory, profileType)) {
			auto pfIt = firewallStateByProfile.find(profileType);
			if (pfIt != firewallStateByProfile.end()) {
				firewallEnabled = pfIt->second.firewallEnabled;
				mdnsPortOpen = pfIt->second.mdnsPortOpen;
				ndiPortsOpen = pfIt->second.ndiPortsOpen;
				filePrinterSharing = pfIt->second.filePrinterSharing;
			}
		}

		for (PIP_ADAPTER_UNICAST_ADDRESS ua = cur->FirstUnicastAddress; ua != nullptr; ua = ua->Next) {
			if (ua->Address.lpSockaddr->sa_family != AF_INET)
				continue;

			NdiAdapterInfo info;
			info.friendlyName = friendlyName;
			info.description = description;
			info.isUp = up;
			info.ifType = cur->IfType;
			info.looksVirtual = LooksVirtual(description, cur->IfType);
			info.supportsMulticast = !(cur->Flags & IP_ADAPTER_NO_MULTICAST);
			info.luid = cur->Luid;
			info.networkCategory = networkCategory;
			info.firewallEnabled = firewallEnabled;
			info.mdnsPortOpen = mdnsPortOpen;
			info.ndiPortsOpen = ndiPortsOpen;
			info.filePrinterSharing = filePrinterSharing;
			info.lastSample = {0, 0, 0, 0, 0};
			char ipStr[INET_ADDRSTRLEN] = {};
			sockaddr_in *sa = reinterpret_cast<sockaddr_in *>(ua->Address.lpSockaddr);
			inet_ntop(AF_INET, &sa->sin_addr, ipStr, sizeof(ipStr));
			info.ipv4Address = ipStr;

			ULONG mask = (ua->OnLinkPrefixLength == 0) ? 0 : (0xFFFFFFFFu << (32 - ua->OnLinkPrefixLength));
			in_addr maskAddr;
			maskAddr.s_addr = htonl(mask);
			char maskStr[INET_ADDRSTRLEN] = {};
			inet_ntop(AF_INET, &maskAddr, maskStr, sizeof(maskStr));
			info.subnetMask = maskStr;

			result.push_back(info);
		}
	}
	return result;
}

#else // Linux and macOS

#if defined(__linux__)

static bool IsWirelessLinux(const std::string &ifname)
{
	struct stat st;
	// Either path existing means the kernel considers this a wireless
	// PHY; the checks NetworkManager/`iw` use under the hood.
	return stat(("/sys/class/net/" + ifname + "/wireless").c_str(), &st) == 0 ||
	       stat(("/sys/class/net/" + ifname + "/phy80211").c_str(), &st) == 0;
}

static ULONG GetIfTypeForName(const std::string &ifname)
{
	std::ifstream f("/sys/class/net/" + ifname + "/type");
	int t = 0;
	if (f.good())
		f >> t;

	if (t == ARPHRD_ETHER && IsWirelessLinux(ifname))
		return kLinuxSyntheticWifiType;

	return static_cast<ULONG>(t);
}

// Windows' Description field (e.g. "Intel(R) Ethernet Connection...") isn't
// exposed by getifaddrs()/sysfs directly. The closest unprivileged
// approximation is the kernel driver module name bound to the device.
static std::string GetDriverNameLinux(const std::string &ifname)
{
	char buf[256] = {};
	std::string path = "/sys/class/net/" + ifname + "/device/driver";
	ssize_t len = readlink(path.c_str(), buf, sizeof(buf) - 1);
	if (len <= 0)
		return {};
	std::string full(buf, static_cast<size_t>(len));
	auto pos = full.find_last_of('/');
	return (pos == std::string::npos) ? full : full.substr(pos + 1);
}

#endif // __linux__

// Enumerate IPv4 adapters using getifaddrs(). One getifaddrs() call is
// reused for both the AF_INET pass (adapter/address info) and, on macOS,
// building a name -> IFT_* hardware-type lookup from the AF_LINK entries
// the same call also returns (Linux doesn't return AF_LINK entries here;
// GetIfTypeForName() reads /sys/class/net/<name>/type instead).
static std::vector<NdiAdapterInfo> EnumerateAdapters()
{
	std::vector<NdiAdapterInfo> result;

	struct ifaddrs *ifaddr = nullptr;
	if (getifaddrs(&ifaddr) != 0)
		return result;

#if defined(__APPLE__)
	std::map<std::string, ULONG> linkTypes;
	for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_LINK && ifa->ifa_name) {
			auto *sdl = reinterpret_cast<struct sockaddr_dl *>(ifa->ifa_addr);
			linkTypes[ifa->ifa_name] = static_cast<ULONG>(sdl->sdl_type);
		}
	}
#endif

	for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
		if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET || !ifa->ifa_name)
			continue;

		std::string ifname = ifa->ifa_name;

		NdiAdapterInfo info;
		info.friendlyName = ifname;
#if defined(__linux__)
		{
			std::string drv = GetDriverNameLinux(ifname);
			info.description = drv.empty() ? ifname : (ifname + " (" + drv + ")");
			info.ifType = GetIfTypeForName(ifname);
		}
#elif defined(__APPLE__)
		// No lightweight equivalent to a driver/vendor string without
		// IOKit; fall back to the interface name.
		info.description = ifname;
		{
			auto it = linkTypes.find(ifname);
			info.ifType = (it != linkTypes.end()) ? it->second : 0;
		}
#endif
		info.isUp = (ifa->ifa_flags & IFF_UP) && (ifa->ifa_flags & IFF_RUNNING);
		info.supportsMulticast = (ifa->ifa_flags & IFF_MULTICAST) != 0;
		info.looksVirtual = LooksVirtual(ifname, info.ifType);
		info.luid.Value = if_nametoindex(ifname.c_str());
		info.lastSample = {0, 0, 0, 0, 0};

		char ipStr[INET_ADDRSTRLEN] = {};
		auto *sa = reinterpret_cast<sockaddr_in *>(ifa->ifa_addr);
		inet_ntop(AF_INET, &sa->sin_addr, ipStr, sizeof(ipStr));
		info.ipv4Address = ipStr;

		if (ifa->ifa_netmask) {
			char maskStr[INET_ADDRSTRLEN] = {};
			auto *sm = reinterpret_cast<sockaddr_in *>(ifa->ifa_netmask);
			inet_ntop(AF_INET, &sm->sin_addr, maskStr, sizeof(maskStr));
			info.subnetMask = maskStr;
		}

		result.push_back(info);
	}

	freeifaddrs(ifaddr);
	return result;
}

#endif // _WIN32

// Sends a small random-token UDP probe to 224.0.0.251:5353 through this
// specific adapter (via IP_MULTICAST_IF) and confirms it actually comes
// back to `s` via multicast loopback within the poll window. See
// NdiAdapterInfo::multicastRoundTripOk in the header for why this exists
// as a check distinct from the IP_ADD_MEMBERSHIP-only join test: sendto()
// and recvfrom() are real network-stack operations that traverse whatever
// sits below the socket API (e.g. a VPN client's WFP-level "block LAN
// traffic" filter), unlike bind()/setsockopt(IP_ADD_MEMBERSHIP), which
// only reflect what the OS is willing to accept locally.
//
// `s` must already be bound to 0.0.0.0:5353 and have successfully joined
// 224.0.0.251 with imr_interface set to adapterIp - i.e. this is meant to
// be called from inside TestMdns()'s per-adapter loop, immediately after
// a successful join and before dropping membership, while the socket is
// still actively listening on that group.
#ifdef _WIN32
static bool TestMulticastRoundTrip(SOCKET s, const std::string &adapterIp)
#else
static bool TestMulticastRoundTrip(int s, const std::string &adapterIp)
#endif
{
	in_addr ifaceAddr{};
	if (inet_pton(AF_INET, adapterIp.c_str(), &ifaceAddr) != 1)
		return false;

	// Steers outbound multicast traffic through this specific adapter -
	// the send-side counterpart to the imr_interface used for the join.
	// Without this, the OS picks whichever route it considers "best" for
	// 224.0.0.251, which on a multi-homed host is not necessarily the
	// adapter under test.
	if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&ifaceAddr, sizeof(ifaceAddr)) != 0)
		return false;

	// The whole test depends on multicast loopback being enabled - it's
	// on by default on both Windows and POSIX, but set it explicitly
	// rather than relying on that default holding true everywhere.
#ifdef _WIN32
	BOOL loopOn = TRUE;
#else
	int loopOn = 1;
#endif
	setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, (const char *)&loopOn, sizeof(loopOn));

	// A random token lets us positively identify our own probe amid
	// whatever genuine mDNS traffic this socket also receives - it's
	// bound to the real port 5353 and joined the real mDNS group (see
	// TestMdns() above), so other devices' real mDNS chatter can and
	// will show up here too.
	std::random_device rd;
	std::mt19937_64 rng(rd());
	std::ostringstream tokenStream;
	tokenStream << "NDINETMON-RTPROBE-" << std::hex << rng();
	const std::string payload = tokenStream.str();

	sockaddr_in dest{};
	dest.sin_family = AF_INET;
	dest.sin_port = htons(5353);
	inet_pton(AF_INET, "224.0.0.251", &dest.sin_addr);

	if (sendto(s, payload.data(), (int)payload.size(), 0, (sockaddr *)&dest, sizeof(dest)) < 0)
		return false;

	// Poll for up to ~500ms total. Anything that isn't our own token
	// (real mDNS traffic sharing this port) is discarded and we keep
	// waiting until the deadline.
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
	char buf[1500];

	while (true) {
		const auto now = std::chrono::steady_clock::now();
		if (now >= deadline)
			break;
		const auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();

		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(s, &readfds);
		timeval tv{};
		tv.tv_sec = static_cast<long>(remainingMs / 1000);
		tv.tv_usec = static_cast<long>((remainingMs % 1000) * 1000);

#ifdef _WIN32
		int selectResult = select(0, &readfds, nullptr, nullptr, &tv);
#else
		int selectResult = select(s + 1, &readfds, nullptr, nullptr, &tv);
#endif
		if (selectResult <= 0)
			break; // timed out, or select() itself failed - either way, give up

		sockaddr_in from{};
#ifdef _WIN32
		int fromLen = sizeof(from);
#else
		socklen_t fromLen = sizeof(from);
#endif
		int received = recvfrom(s, buf, sizeof(buf) - 1, 0, (sockaddr *)&from, &fromLen);
		if (received <= 0)
			continue;

		if (payload.size() == static_cast<size_t>(received) && memcmp(buf, payload.data(), received) == 0) {
			// our own probe came back - the round trip succeeded
			return true;
		}

		// else: not our packet - loop and keep waiting until the deadline
	}

	return false;
}

// Holds one (adapter, IPv4 address) pair's multicast group membership -
// see the big comment on TestMdns() below for why this is joined once and
// held indefinitely rather than joined/dropped every tick. Keyed by
// (luid.Value, ipv4Address) rather than just the LUID, matching
// EnumerateAdapters()'s granularity of one NdiAdapterInfo per unicast
// address - a multi-homed adapter with more than one IPv4 address gets
// tracked (and joined) independently per address.
//
// TestMdns()/DiagnoseNdiNetwork() normally only ever run on
// NetworkMonitor's single background thread (monitorLoop()), but
// NetworkMonitor's constructor also makes one synchronous
// DiagnoseNdiNetwork() call of its own right as that background thread is
// starting up (see network-monitor.cpp), so there's a brief real window
// where both could touch this map concurrently - hence the mutex, rather
// than assuming single-threaded access.
#ifdef _WIN32
struct PersistedMulticastMembership {
	SOCKET sock = INVALID_SOCKET;
};
#else
struct PersistedMulticastMembership {
	int sock = -1;
};
#endif

static std::mutex g_multicastMembershipsMutex;
static std::map<std::pair<uint64_t, std::string>, PersistedMulticastMembership> g_multicastMemberships;

// Returns a socket that has already joined 224.0.0.251 through adapter
// `a`'s interface - creating one and joining it, if this (adapter, IP)
// pair hasn't been seen before, and holding onto it in
// g_multicastMemberships for every subsequent call rather than dropping
// membership afterward. Returns INVALID_SOCKET/-1 on failure (caller
// treats that as "join failed").
#ifdef _WIN32
static SOCKET EnsureMulticastMembership(const NdiAdapterInfo &a)
#else
static int EnsureMulticastMembership(const NdiAdapterInfo &a)
#endif
{
	const auto key = std::make_pair(a.luid.Value, a.ipv4Address);

	std::lock_guard<std::mutex> lock(g_multicastMembershipsMutex);

	auto it = g_multicastMemberships.find(key);
	if (it != g_multicastMemberships.end())
		return it->second.sock;

#ifdef _WIN32
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return INVALID_SOCKET;

	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) {
		WSACleanup();
		return INVALID_SOCKET;
	}
	BOOL reuse = TRUE;
#else
	int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0)
		return -1;
	int reuse = 1;
#endif
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
#if !defined(_WIN32) && defined(SO_REUSEPORT)
	setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (const char *)&reuse, sizeof(reuse));
#endif

	// Multiple sockets (one per held membership, plus TestMdns()'s own
	// short-lived bind-test socket) all bind the same local port
	// 0.0.0.0:5353 concurrently - SO_REUSEADDR above is what makes that
	// legal, the same sharing mechanism that already lets this plugin
	// coexist with Bonjour/NDI's own runtime on the same port.
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(5353);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(s, (sockaddr *)&addr, sizeof(addr)) != 0) {
#ifdef _WIN32
		closesocket(s);
		WSACleanup();
		return INVALID_SOCKET;
#else
		close(s);
		return -1;
#endif
	}

	ip_mreq mreq{};
	inet_pton(AF_INET, "224.0.0.251", &mreq.imr_multiaddr);
	inet_pton(AF_INET, a.ipv4Address.c_str(), &mreq.imr_interface);
	if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&mreq, sizeof(mreq)) != 0) {
#ifdef _WIN32
		closesocket(s);
		WSACleanup();
		return INVALID_SOCKET;
#else
		close(s);
		return -1;
#endif
	}

	PersistedMulticastMembership membership;
	membership.sock = s;
	g_multicastMemberships[key] = membership;
	return s;
}

// Drops and closes membership for every (adapter, IP) pair that isn't in
// `currentAdapters` at all - i.e. the adapter was actually unplugged/
// disabled, not just currently down or otherwise non-qualifying. An
// adapter that's merely down but still enumerated keeps its held
// membership, so it doesn't need to rejoin if the link comes back.
static void DropVanishedMulticastMemberships(const std::vector<NdiAdapterInfo> &currentAdapters)
{
	std::lock_guard<std::mutex> lock(g_multicastMembershipsMutex);

	for (auto it = g_multicastMemberships.begin(); it != g_multicastMemberships.end();) {
		const auto &key = it->first;
		const bool stillPresent =
			std::any_of(currentAdapters.begin(), currentAdapters.end(), [&key](const NdiAdapterInfo &a) {
				return a.luid.Value == key.first && a.ipv4Address == key.second;
			});

		if (stillPresent) {
			++it;
			continue;
		}

#ifdef _WIN32
		closesocket(it->second.sock);
		WSACleanup(); // pairs with the WSAStartup() in EnsureMulticastMembership()
#else
		close(it->second.sock);
#endif
		it = g_multicastMemberships.erase(it);
	}
}

// Binds a fresh, short-lived UDP socket to port 5353 on every call, purely
// to answer "can we bind this port right now" (mdnsSocketBindOk /
// mdnsPortInUse) - a local bind() call that doesn't touch the
// adapter/driver, so it runs every tick.
//
// Multicast group membership is joined ONCE per (adapter, IP) pair and
// held open indefinitely (see EnsureMulticastMembership()/
// g_multicastMemberships above) rather than joined and dropped every
// tick: cycling IP_ADD_MEMBERSHIP/IP_DROP_MEMBERSHIP on a live NIC once a
// second, forever, sends a real IGMP join/leave onto the wire and
// triggers an OID_802_3_MULTICAST_LIST update to the NIC driver every
// time - on some chipsets this causes genuine periodic drops in the
// adapter's actual throughput, visible in Windows' own Task Manager graph
// (independent of anything this plugin itself measures or displays). The
// round-trip probe below runs every tick regardless.
static void TestMdns(NdiNetworkReport &report)
{
#ifdef _WIN32
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return;

	SOCKET bindTestSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (bindTestSocket == INVALID_SOCKET) {
		WSACleanup();
		return;
	}
#else
	int bindTestSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (bindTestSocket < 0)
		return;
#endif

#ifdef _WIN32
	BOOL reuse = TRUE;
#else
	int reuse = 1;
#endif
	setsockopt(bindTestSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
#if !defined(_WIN32) && defined(SO_REUSEPORT)
	// Some mDNS responders (Avahi, mDNSResponder) bind with SO_REUSEPORT
	// rather than SO_REUSEADDR; set both so this bind-test matches how a
	// real responder would actually behave.
	setsockopt(bindTestSocket, SOL_SOCKET, SO_REUSEPORT, (const char *)&reuse, sizeof(reuse));
#endif

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(5353);
	addr.sin_addr.s_addr = INADDR_ANY;

	int bindResult = bind(bindTestSocket, (sockaddr *)&addr, sizeof(addr));
	if (bindResult == 0) {
		report.mdnsSocketBindOk = true;
	} else {
#ifdef _WIN32
		int err = WSAGetLastError();
		// WSAEADDRINUSE (10048) generally means another mDNS responder (Bonjour,
		// Windows mDNS, NDI Access Manager) already owns the port - that's fine.
		report.mdnsPortInUse = (err == WSAEADDRINUSE);
#else
		// EADDRINUSE generally means another mDNS responder (Avahi,
		// mDNSResponder, NDI Access Manager) already owns the port -
		// that's fine.
		report.mdnsPortInUse = (errno == EADDRINUSE);
#endif
	}

#ifdef _WIN32
	closesocket(bindTestSocket);
	WSACleanup();
#else
	close(bindTestSocket);
#endif

	// Round-trip test every currently-qualifying adapter against its held
	// (or newly-created) membership socket - see EnsureMulticastMembership().
	for (auto &a : report.adapters) {
		if (!a.isUp || a.looksVirtual || !a.supportsMulticast)
			continue;

		auto s = EnsureMulticastMembership(a);
#ifdef _WIN32
		if (s == INVALID_SOCKET)
			continue;
#else
		if (s < 0)
			continue;
#endif
		a.multicastJoinOk = true;
		a.multicastRoundTripOk = TestMulticastRoundTrip(s, a.ipv4Address);
	}

	DropVanishedMulticastMemberships(report.adapters);
}

#if defined(_WIN32)

bool GetSample(IF_LUID luid, NicSample &out)
{
	MIB_IF_ROW2 row = {};
	row.InterfaceLuid = luid;
	if (GetIfEntry2(&row) != NO_ERROR)
		return false;

	out.inOctets = row.InOctets;
	out.outOctets = row.OutOctets;
	out.receiveSpeed = row.ReceiveLinkSpeed;
	out.transmitSpeed = row.TransmitLinkSpeed;
	out.timestampMs = GetTickCount64();
	return true;
}

#elif defined(__linux__)

bool GetSample(IF_LUID luid, NicSample &out)
{
	char ifname[IF_NAMESIZE] = {};
	if (!if_indextoname(static_cast<unsigned int>(luid.Value), ifname))
		return false;

	auto readU64 = [&](const char *file) -> uint64_t {
		std::ifstream f(std::string("/sys/class/net/") + ifname + "/" + file);
		uint64_t v = 0;
		if (f.good())
			f >> v;
		return v;
	};

	out.inOctets = readU64("statistics/rx_bytes");
	out.outOctets = readU64("statistics/tx_bytes");

	// sysfs only exposes one link speed (Mbps), not separate rx/tx
	// figures, and it's frequently unavailable: -1 (or a read failure) is
	// common for virtual interfaces, many Wi-Fi drivers, and any
	// interface that's administratively down. We normalize "unknown" to
	// 0 bps rather than propagate the -1 sentinel.
	std::ifstream speedFile(std::string("/sys/class/net/") + ifname + "/speed");
	long speedMbps = -1;
	if (speedFile.good())
		speedFile >> speedMbps;
	ULONG64 speedBps = (speedMbps > 0) ? static_cast<ULONG64>(speedMbps) * 1000000ULL : 0ULL;
	out.receiveSpeed = speedBps;
	out.transmitSpeed = speedBps;

	out.timestampMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
							std::chrono::steady_clock::now().time_since_epoch())
							.count());
	return true;
}

#elif defined(__APPLE__)

bool GetSample(IF_LUID luid, NicSample &out)
{
	unsigned int ifindex = static_cast<unsigned int>(luid.Value);
	char ifname[IF_NAMESIZE] = {};
	if (!if_indextoname(ifindex, ifname))
		return false;

	// NET_RT_IFLIST2/if_msghdr2 gives 64-bit byte counters and the link
	// baudrate (already in bps) in one call - the same data source
	// `netstat -ib` uses. Passing the ifindex in mib[5] asks the kernel
	// to filter to just that interface.
	int mib[6] = {CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST2, static_cast<int>(ifindex)};
	size_t needed = 0;
	if (sysctl(mib, 6, nullptr, &needed, nullptr, 0) < 0)
		return false;

	std::vector<char> buf(needed);
	if (sysctl(mib, 6, buf.data(), &needed, nullptr, 0) < 0)
		return false;

	bool found = false;
	for (char *p = buf.data(); p < buf.data() + needed;) {
		auto *ifm = reinterpret_cast<if_msghdr2 *>(p);
		if (ifm->ifm_type == RTM_IFINFO2 && static_cast<unsigned int>(ifm->ifm_index) == ifindex) {
			out.inOctets = ifm->ifm_data.ifi_ibytes;
			out.outOctets = ifm->ifm_data.ifi_obytes;
			out.receiveSpeed = static_cast<ULONG64>(ifm->ifm_data.ifi_baudrate);
			out.transmitSpeed = static_cast<ULONG64>(ifm->ifm_data.ifi_baudrate);
			found = true;
			break;
		}
		if (ifm->ifm_msglen == 0)
			break; // guard against a malformed/short buffer
		p += ifm->ifm_msglen;
	}

	if (!found)
		return false;

	out.timestampMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
							std::chrono::steady_clock::now().time_since_epoch())
							.count());
	return true;
}

#endif

void UpdateNicbps(std::vector<NdiAdapterInfo> &nics)
{

	for (auto &nic : nics) {
		// if (!nic.ndiCandidate)  // TODO: Look in config to see if it is set as a prefered NIC.
		//	continue;

		NicSample prev, curr;

		prev = nic.lastSample;

		if (GetSample(nic.luid, curr)) {
			if (prev.timestampMs != 0ULL) {

				double elapsedSec = (curr.timestampMs - prev.timestampMs) / 1000.0;
				if (elapsedSec <= 0)
					continue;

				nic.inBps = ((curr.inOctets - prev.inOctets) * 8.0) / elapsedSec;
				nic.outBps = ((curr.outOctets - prev.outOctets) * 8.0) / elapsedSec;
				nic.receiveSpeed = curr.receiveSpeed;
				nic.transmitSpeed = curr.transmitSpeed;
			}
			nic.lastSample = curr;
		}
	}
}

NdiNetworkReport DiagnoseNdiNetwork()
{
	NdiNetworkReport report;
	report.adapters = EnumerateAdapters();
	TestMdns(report); // populates each adapter's NdiAdapterInfo::multicastJoinOk
	report.avahiStatus = GetAvahiStatus();

	for (auto &a : report.adapters) {
		a.ndiCandidate = (a.isUp && !a.looksVirtual && a.supportsMulticast && !a.ipv4Address.empty());
	}
	return report;
}

QString formatBps(double bps)
{
	// Fixed-width formatting: 6 chars for the numeric portion (e.g. "###.##")
	// and 5 chars for the unit (including leading space), total length = 11.
	if (bps >= 1e9)
		return QStringLiteral("%1 Gbps").arg(bps / 1e9, 6, 'f', 2);
	if (bps >= 1e6)
		return QStringLiteral("%1 Mbps").arg(bps / 1e6, 6, 'f', 2);
	if (bps >= 1e3)
		return QStringLiteral("%1 Kbps").arg(bps / 1e3, 6, 'f', 2);
	// For small values show integer bps with no decimals, still width 6.
	return QStringLiteral("%1 bps ").arg(bps, 6, 'f', 0);
}

std::string FormatNdiNetworkReport(const NdiNetworkReport &report)
{
	std::ostringstream out;

	// Column widths tuned for monospace display
	const int COL_REC = 3;
	const int COL_NAME = 28;
	const int COL_TYPE = 12;
	const int COL_IPV4 = 15;
	const int COL_MASK = 15;
	const int COL_STATUS = 6;
	const int COL_NETCAT = 8;
	const int COL_MCAST = 3;
	const int COL_JOIN = 6;
	const int COL_RTRIP = 6;
	const int COL_VIRT = 3;
	const int COL_FW = 3;
	const int COL_MDNSOPEN = 7; // fits "Allowed"/"Blocked"
	const int COL_PORTS = 7;    // fits "Allowed"/"Blocked"
	const int COL_FPSHARE = 11; // fits header "F&P Sharing" (data is only ever "Allowed"/"Blocked"/"N/A")
	const int COL_DESC = 20;
	const int COL_INBPS = 11;
	const int COL_OUTBPS = 11;
	const int COL_RSPEED = 11;
	const int COL_TSPEED = 11;
	// Header
	out << std::left << std::setw(COL_REC) << "NDI" << " | " << std::left << std::setw(COL_NAME) << "Name" << " | "
	    << std::left << std::setw(COL_TYPE) << "Type" << " | " << std::left << std::setw(COL_IPV4) << "IPv4"
	    << " | " << std::left << std::setw(COL_MASK) << "Subnet" << " | " << std::left << std::setw(COL_STATUS)
	    << "Status" << " | " << std::left << std::setw(COL_NETCAT) << "Category" << " | " << std::left
	    << std::setw(COL_MCAST) << "Mul" << " | " << std::left << std::setw(COL_JOIN) << "Joined" << " | "
	    << std::left << std::setw(COL_RTRIP) << "RTrip" << " | " << std::left << std::setw(COL_VIRT) << "Vir"
	    << " | " << std::left << std::setw(COL_FW) << "FW" << " | " << std::left << std::setw(COL_MDNSOPEN)
	    << "mDNS" << " | " << std::left << std::setw(COL_PORTS) << "Ports" << " | " << std::left
	    << std::setw(COL_FPSHARE) << "F&P Sharing" << " | " << std::left << std::setw(COL_DESC) << "Description"
	    << " | " << std::left << std::setw(COL_INBPS) << "In Mbps"
	    << " | " << std::left << std::setw(COL_OUTBPS) << "Out Mbps"
	    << " | " << std::left << std::setw(COL_RSPEED) << "Rx Speed"
	    << " | " << std::left << std::setw(COL_TSPEED) << "Tx Speed" << '\n';

	// Separator line
	out << std::string(COL_REC, '-') << "-+-" << std::string(COL_NAME, '-') << "-+-" << std::string(COL_TYPE, '-')
	    << "-+-" << std::string(COL_IPV4, '-') << "-+-" << std::string(COL_MASK, '-') << "-+-"
	    << std::string(COL_STATUS, '-') << "-+-" << std::string(COL_NETCAT, '-') << "-+-"
	    << std::string(COL_MCAST, '-') << "-+-" << std::string(COL_JOIN, '-') << "-+-"
	    << std::string(COL_RTRIP, '-') << "-+-" << std::string(COL_VIRT, '-') << "-+-" << std::string(COL_FW, '-')
	    << "-+-" << std::string(COL_MDNSOPEN, '-') << "-+-" << std::string(COL_PORTS, '-') << "-+-"
	    << std::string(COL_FPSHARE, '-') << "-+-" << std::string(COL_DESC, '-') << "-+-"
	    << std::string(COL_INBPS, '-') << "-+-" << std::string(COL_OUTBPS, '-') << "-+-"
	    << std::string(COL_RSPEED, '-') << "-+-" << std::string(COL_TSPEED, '-') << '\n';
	for (const auto &a : report.adapters) {
		// Format values and truncate as necessary to keep columns aligned
		std::string rec = a.ndiCandidate ? "  *" : "   ";
		std::string name = truncate_for_column(a.friendlyName, COL_NAME);
		std::string type = truncate_for_column(IfTypeToString(a.ifType), COL_TYPE);
		std::string ipv4 = truncate_for_column(a.ipv4Address.empty() ? "(none)" : a.ipv4Address, COL_IPV4);
		std::string mask = truncate_for_column(a.subnetMask.empty() ? "(none)" : a.subnetMask, COL_MASK);
		std::string status = a.isUp ? "Up" : "Down";
		std::string netcat = truncate_for_column(NetworkCategoryToString(a.networkCategory), COL_NETCAT);
		std::string mcast = a.supportsMulticast ? "Yes" : "No"; // static capability, not a live test
		std::string join = a.isUp && a.supportsMulticast ? (a.multicastJoinOk ? "Yes" : "No") : "n/a";
		// Only meaningful when the join itself succeeded - that's the
		// only case TestMulticastRoundTrip() actually runs (see
		// TestMdns()), so "n/a" here means "never attempted", not
		// "attempted and failed".
		std::string rtrip = a.multicastJoinOk ? (a.multicastRoundTripOk ? "Yes" : "No") : "n/a";
		std::string virt = a.looksVirtual ? "Yes" : "No";
		// n/a for firewallEnabled when networkCategory is Unknown
		// (non-Windows, or no NLM entry) - see
		// NdiAdapterInfo::firewallEnabled. mdnsPortOpen/ndiPortsOpen
		// already default to FirewallPortAccess::NotApplicable in that
		// same case, so FirewallPortAccessToString() alone covers it.
		const bool fwApplicable = (a.networkCategory != NdiNetworkCategory::Unknown);
		std::string fw = fwApplicable ? (a.firewallEnabled ? "Yes" : "No") : "n/a";
		std::string mdnsOpen = FirewallPortAccessToString(a.mdnsPortOpen);
		std::string portsOpen = FirewallPortAccessToString(a.ndiPortsOpen);
		std::string fpShare = FirewallPortAccessToString(a.filePrinterSharing);
		std::string desc = truncate_for_column(a.description, COL_DESC);
		std::string inBps = truncate_for_column(formatBps(a.inBps).toUtf8().constData(), COL_INBPS);
		std::string outBps = truncate_for_column(formatBps(a.outBps).toUtf8().constData(), COL_OUTBPS);
		std::string rspeed = truncate_for_column(formatBps(a.receiveSpeed).toUtf8().constData(), COL_RSPEED);
		std::string tspeed = truncate_for_column(formatBps(a.transmitSpeed).toUtf8().constData(), COL_TSPEED);
		out << std::left << std::setw(COL_REC) << rec << " | " << std::left << std::setw(COL_NAME) << name
		    << " | " << std::left << std::setw(COL_TYPE) << type << " | " << std::left << std::setw(COL_IPV4)
		    << ipv4 << " | " << std::left << std::setw(COL_MASK) << mask << " | " << std::left
		    << std::setw(COL_STATUS) << status << " | " << std::left << std::setw(COL_NETCAT) << netcat << " | "
		    << std::left << std::setw(COL_MCAST) << mcast << " | " << std::left << std::setw(COL_JOIN) << join
		    << " | " << std::left << std::setw(COL_RTRIP) << rtrip << " | " << std::left << std::setw(COL_VIRT)
		    << virt << " | " << std::left << std::setw(COL_FW) << fw << " | " << std::left
		    << std::setw(COL_MDNSOPEN) << mdnsOpen << " | " << std::left << std::setw(COL_PORTS) << portsOpen
		    << " | " << std::left << std::setw(COL_FPSHARE) << fpShare << " | " << std::left
		    << std::setw(COL_DESC) << desc << " | " << std::left << std::setw(COL_INBPS) << inBps << " | "
		    << std::left << std::setw(COL_OUTBPS) << outBps << " | " << std::left << std::setw(COL_RSPEED)
		    << rspeed << " | " << std::left << std::setw(COL_TSPEED) << tspeed << '\n';
	}

	out << "\nmDNS / multicast discovery\n";
	out << "--------------------------\n";
	out << "mDNS Port (5353)                  : " << (report.mdnsPortInUse ? "In Use" : "Available") << '\n';
	out << "mDNS Socket Bind Test             : " << (report.mdnsSocketBindOk ? "Succeeded" : "Failed") << '\n';
	out << "Avahi daemon running (Linux)      : " << AvahiStatusToString(report.avahiStatus) << '\n';
	return out.str();
}

// ---------------------------------------------------------------------------
// PLATFORM LIMITATIONS - see the chat response for the full list. The short
// version: Windows Firewall introspection has no cross-platform equivalent
// (stubbed to a single unprivileged ICMP check on Linux, unimplemented on
// macOS); Wi-Fi vs. Ethernet can't be distinguished on macOS without linking
// SystemConfiguration.framework; IF_LUID's persistence guarantee is
// approximated with an OS interface index that can be reused after unplug;
// and the vendor "Description" string Windows exposes has no direct
// Linux/macOS equivalent (approximated with the driver name on Linux only).
//
// The ULONG/ULONG64/IF_LUID compatibility shims this file relies on for
// NdiAdapterInfo/NicSample now live in ndi-network-report.h itself (guarded
// by #ifndef _WIN32), so every other translation unit that includes the
// header picks them up too - nothing project-wide is left dangling on this
// file alone.
