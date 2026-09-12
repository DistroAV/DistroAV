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

#include "ndi-adapter-table-widget.h"
#include "ndi-sender-table-widget.h"
#include "plugin-main.h"
#include <QTableView>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QMenu>
#include <QAction>
#include <QHBoxLayout>
#include <QBrush>
#include <QColor>

// ifTypeName() below classifies NdiAdapterInfo::ifType for display. The
// numeric values it switches on come from different headers per platform -
// on Windows they arrive transitively via ndi-network-report.h's
// <iphlpapi.h>/<windows.h> includes, but Linux/macOS need these pulled in
// directly since ndi-network-report.h only shims the *integer type*
// (ULONG), not these platform enums.
#if !defined(_WIN32)
#if defined(__linux__)
#include <net/if_arp.h> // ARPHRD_* constants
#elif defined(__APPLE__)
#include <net/if_types.h> // IFT_* constants
#endif
#endif

namespace {

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

// NOTE: this mirrors the classification in IfTypeToString() (ndi-network-report.cpp)
// rather than calling it, even though that duplicates the case list. That
// function returns a plain std::string; feeding a runtime string into tr()
// here would still work, but Qt's lupdate tool only extracts *literal*
// tr("...") arguments for translation, so runtime strings would silently
// never get translated. Keeping literal tr() calls in both places is the
// correct trade-off for a translatable UI at the cost of the duplication.
// If you change one, change the other.
QString ifTypeName(ULONG ifType)
{
#if defined(_WIN32)
	switch (ifType) {
	case IF_TYPE_ETHERNET_CSMACD:
		return QObject::tr("Ethernet");
	case IF_TYPE_IEEE80211:
		return QObject::tr("Wi-Fi");
	case IF_TYPE_SOFTWARE_LOOPBACK:
		return QObject::tr("Loopback");
	case IF_TYPE_PPP:
		return QObject::tr("PPP");
	case IF_TYPE_TUNNEL:
		return QObject::tr("Tunnel");
	case IF_TYPE_IEEE1394:
		return QObject::tr("FireWire");
	default:
		return QObject::tr("Other");
	}
#elif defined(__linux__)
	// Managed-mode Wi-Fi reports as ARPHRD_ETHER (1), same as wired
	// Ethernet, so it's caught via the synthetic sentinel that
	// ndi-network-report.cpp's GetIfTypeForName() stores instead - see
	// kLinuxSyntheticWifiType's definition in ndi-network-report.h.
	if (ifType == kLinuxSyntheticWifiType)
		return QObject::tr("Wi-Fi");
	switch (ifType) {
	case ARPHRD_ETHER:
		return QObject::tr("Ethernet");
	case ARPHRD_IEEE80211:
		return QObject::tr("Wi-Fi"); // monitor-mode interfaces
	case ARPHRD_LOOPBACK:
		return QObject::tr("Loopback");
	case ARPHRD_PPP:
		return QObject::tr("PPP");
	case ARPHRD_TUNNEL:
	case ARPHRD_TUNNEL6:
	case ARPHRD_SIT:
	case ARPHRD_IPGRE:
	case ARPHRD_NONE: // common for WireGuard, some VPNs
		return QObject::tr("Tunnel");
	case ARPHRD_IEEE1394:
		return QObject::tr("FireWire");
	default:
		return QObject::tr("Other");
	}
#elif defined(__APPLE__)
	switch (ifType) {
	case IFT_ETHER:
		// macOS reports Wi-Fi as IFT_ETHER too (802.11 emulates
		// Ethernet at this layer). Telling them apart needs
		// SystemConfiguration.framework's
		// SCNetworkInterfaceGetInterfaceType(), which this file
		// doesn't link against, so both show the combined label.
		return QObject::tr("Ethernet/Wi-Fi");
	case IFT_LOOP:
		return QObject::tr("Loopback");
	case IFT_PPP:
		return QObject::tr("PPP");
	case IFT_GIF:
	case IFT_STF:
		return QObject::tr("Tunnel");
	case IFT_IEEE1394:
		return QObject::tr("FireWire");
	default:
		return QObject::tr("Other");
	}
#endif
}

QString networkCategoryName(NdiNetworkCategory category)
{
	switch (category) {
	case NdiNetworkCategory::Public:
		return QObject::tr("Public");
	case NdiNetworkCategory::Private:
		return QObject::tr("Private");
	case NdiNetworkCategory::DomainAuthenticated:
		return QObject::tr("Domain");
	case NdiNetworkCategory::Unknown:
	default:
		return QObject::tr("Unknown");
	}
}

QString avahiStatusLabelText(AvahiStatus status)
{
	switch (status) {
	case AvahiStatus::Running:
		return QObject::tr("Avahi: Running");
	case AvahiStatus::NotRunning:
		return QObject::tr("Avahi: Not running");
	case AvahiStatus::NotApplicable:
	default:
		return QObject::tr("Avahi: N/A");
	}
}

// Display helper for the per-adapter FW column: "N/A" when the adapter's
// networkCategory is Unknown (no per-adapter firewall state could be
// joined - see NdiAdapterInfo::firewallEnabled), otherwise Yes/No.
QString yesNoOrNA(bool applicable, bool value)
{
	if (!applicable)
		return QObject::tr("N/A");
	return value ? QObject::tr("Yes") : QObject::tr("No");
}

// Tri-state sort value for the column above: N/A must sort distinctly from
// both Yes and No rather than colliding with either.
int yesNoOrNASortValue(bool applicable, bool value)
{
	if (!applicable)
		return -1;
	return value ? 1 : 0;
}

// NOTE: mirrors FirewallPortAccessToString() (ndi-network-report.cpp)
// rather than calling it, for the same lupdate/tr() reason documented on
// ifTypeName() above. Used for the per-adapter mDNS/Ports columns -
// NotApplicable already covers both "the profile's firewall master switch
// is off" and "networkCategory is Unknown", so unlike ColFirewallEnabled
// these columns don't need a separate applicability check.
QString firewallPortAccessName(FirewallPortAccess access)
{
	switch (access) {
	case FirewallPortAccess::Allowed:
		return QObject::tr("Allowed");
	case FirewallPortAccess::Blocked:
		return QObject::tr("Blocked");
	case FirewallPortAccess::NotApplicable:
	default:
		return QObject::tr("N/A");
	}
}

} // namespace

// ---------------------------------------------------------------------------
// NdiAdapterTableModel
// ---------------------------------------------------------------------------

NdiAdapterTableModel::NdiAdapterTableModel(QObject *parent) : QAbstractTableModel(parent) {}

void NdiAdapterTableModel::setAdapters(std::vector<NdiAdapterInfo> adapters)
{
	beginResetModel();
	m_adapters = std::move(adapters);
	endResetModel();
}

const NdiAdapterInfo *NdiAdapterTableModel::adapterAt(int row) const
{
	if (row < 0 || row >= static_cast<int>(m_adapters.size()))
		return nullptr;
	return &m_adapters[row];
}

int NdiAdapterTableModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;
	return static_cast<int>(m_adapters.size());
}

int NdiAdapterTableModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;
	return ColumnCount;
}

QVariant NdiAdapterTableModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

	const int row = index.row();
	if (row < 0 || row >= static_cast<int>(m_adapters.size()))
		return QVariant();

	const NdiAdapterInfo &a = m_adapters[row];
	const bool wantsSortValue = (role == NdiAdapterSortRole);

	if (role == Qt::DisplayRole || wantsSortValue) {
		switch (index.column()) {
		case ColNDI:
			return wantsSortValue ? QVariant(a.ndiCandidate)
					      : QVariant(a.ndiCandidate ? tr("Yes") : tr("No"));

		case ColFriendlyName:
			return QString::fromStdString(a.friendlyName);
		case ColDescription:
			return QString::fromStdString(a.description);
		case ColIpv4Address:
			return QString::fromStdString(a.ipv4Address);
		case ColSubnetMask:
			return QString::fromStdString(a.subnetMask);
		case ColIsUp:
			return wantsSortValue ? QVariant(a.isUp) : QVariant(a.isUp ? tr("Yes") : tr("No"));
		case ColSupportsMulticast:
			return wantsSortValue ? QVariant(a.supportsMulticast)
					      : QVariant(a.supportsMulticast ? tr("Yes") : tr("No"));
		case ColJoin:
			return wantsSortValue ? QVariant(a.multicastJoinOk)
					      : QVariant(a.multicastJoinOk ? tr("Yes") : tr("No"));
		case ColRoundTrip:
			return wantsSortValue ? QVariant(a.multicastRoundTripOk)
					      : QVariant(a.multicastRoundTripOk ? tr("Yes") : tr("No"));
		case ColLooksVirtual:
			return wantsSortValue ? QVariant(a.looksVirtual)
					      : QVariant(a.looksVirtual ? tr("Yes") : tr("No"));
		case ColNetworkCategory:
			return wantsSortValue ? QVariant(static_cast<int>(a.networkCategory))
					      : QVariant(networkCategoryName(a.networkCategory));
		case ColFirewallEnabled: {
			const bool applicable = (a.networkCategory != NdiNetworkCategory::Unknown);
			return wantsSortValue ? QVariant(yesNoOrNASortValue(applicable, a.firewallEnabled))
					      : QVariant(yesNoOrNA(applicable, a.firewallEnabled));
		}
		case ColMdnsPortOpen:
			return wantsSortValue ? QVariant(static_cast<int>(a.mdnsPortOpen))
					      : QVariant(firewallPortAccessName(a.mdnsPortOpen));
		case ColNdiPortsOpen:
			return wantsSortValue ? QVariant(static_cast<int>(a.ndiPortsOpen))
					      : QVariant(firewallPortAccessName(a.ndiPortsOpen));
		case ColFilePrinterSharing:
			return wantsSortValue ? QVariant(static_cast<int>(a.filePrinterSharing))
					      : QVariant(firewallPortAccessName(a.filePrinterSharing));
		case ColIfType:
			return wantsSortValue
				       ? QVariant(static_cast<uint>(a.ifType))
				       : QVariant(QStringLiteral("%1 (%2)").arg(a.ifType).arg(ifTypeName(a.ifType)));
		case ColInBps:
			return wantsSortValue ? QVariant(a.inBps) : QVariant(formatBps(a.inBps));
		case ColOutBps:
			return wantsSortValue ? QVariant(a.outBps) : QVariant(formatBps(a.outBps));
		case ColReceiveSpeed:
			return wantsSortValue ? QVariant(a.receiveSpeed) : QVariant(formatBps(a.receiveSpeed));
		case ColTransmitSpeed:
			return wantsSortValue ? QVariant(a.transmitSpeed) : QVariant(formatBps(a.transmitSpeed));
		default:
			return QVariant();
		}
	}

	if (role == Qt::TextAlignmentRole) {
		switch (index.column()) {
		case ColNDI:
		case ColIsUp:
		case ColSupportsMulticast:
		case ColJoin:
		case ColRoundTrip:
		case ColLooksVirtual:
		case ColFirewallEnabled:
		case ColMdnsPortOpen:
		case ColNdiPortsOpen:
		case ColFilePrinterSharing:
			// Center Yes/No/N/A style columns
			return QVariant(Qt::AlignHCenter | Qt::AlignVCenter);
		case ColNetworkCategory:
			return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
		case ColIfType:
		case ColInBps:
		case ColOutBps:
		case ColReceiveSpeed:
		case ColTransmitSpeed:
			return QVariant(Qt::AlignRight | Qt::AlignVCenter);
		default:
			return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
		}
	}

	// Highlight entire row background blue when adapter is an NDI candidate.
	if (role == Qt::BackgroundRole) {
		if (a.ndiCandidate) {
			// Light blue suitable as background
			return QVariant(QBrush(QColor(0, 120, 215, 64))); // semi-transparent blue
		}
		return QVariant();
	}

	return QVariant();
}

QVariant NdiAdapterTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
		return QAbstractTableModel::headerData(section, orientation, role);

	switch (section) {
	case ColNDI:
		return tr("  NDI  ");
	case ColFriendlyName:
		return tr("Friendly Name");
	case ColDescription:
		return tr("Description");
	case ColIpv4Address:
		return tr("IPv4 Address");
	case ColSubnetMask:
		return tr("Subnet Mask");
	case ColIsUp:
		return tr("Up");
	case ColSupportsMulticast:
		return tr("Multi");
	case ColJoin:
		return tr("Join");
	case ColRoundTrip:
		return tr("Rtrip");
	case ColLooksVirtual:
		return tr("Virt");
	case ColNetworkCategory:
		return tr("Cat");
	case ColFirewallEnabled:
		return tr("FW");
	case ColMdnsPortOpen:
		return tr("mDNS");
	case ColNdiPortsOpen:
		return tr("Ports");
	case ColFilePrinterSharing:
		return tr("F&P Sharing");
	case ColIfType:
		return tr("Type");
	case ColInBps:
		return tr("In");
	case ColOutBps:
		return tr("Out");
	case ColReceiveSpeed:
		return tr("Rx Speed");
	case ColTransmitSpeed:
		return tr("Tx Speed");
	default:
		return QVariant();
	}
}

Qt::ItemFlags NdiAdapterTableModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;
	// Selectable/enabled only - no ItemIsEditable, so cells are read-only.
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

// ---------------------------------------------------------------------------
// NdiAdapterTableWidget
// ---------------------------------------------------------------------------

NdiAdapterTableWidget::NdiAdapterTableWidget(QWidget *parent)
	: QWidget(parent),
	  m_tableView(new QTableView(this)),
	  m_model(new NdiAdapterTableModel(this)),
	  m_proxyModel(new QSortFilterProxyModel(this))
{
	auto *layout = new QVBoxLayout(this);

	// Get the NdiNetworkMonitor singleton instance and get the current network report.
	NdiNetworkReport report = network_monitor->getNetworkReport();

	// Row of read-only checkboxes showing diagnostic flags.
	auto makeFlag = [this](const QString &text, bool checked) {
		auto *cb = new QCheckBox(text, this);
		cb->setChecked(checked);
		cb->setEnabled(false);
		cb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
		return cb;
	};

	QGroupBox *mdnsGroup = new QGroupBox(tr("mDNS  / multicast discovery"), this);
	auto *mdnsLayout = new QHBoxLayout(mdnsGroup);
	mdnsLayout->setContentsMargins(6, 14, 6, 4);
	mdnsLayout->setSpacing(6);
	m_mdnsBindCheckBox = makeFlag("mDNS bind OK", report.mdnsSocketBindOk);
	m_mdnsPortCheckBox = makeFlag("mDNS port not in use", !report.mdnsPortInUse);
	m_avahiStatusLabel = new QLabel(this);
	m_avahiStatusLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	m_avahiStatusLabel->setText(avahiStatusLabelText(report.avahiStatus));
	mdnsLayout->addWidget(m_mdnsBindCheckBox, 1, Qt::AlignCenter);
	mdnsLayout->addWidget(m_mdnsPortCheckBox, 1, Qt::AlignCenter);
	mdnsLayout->addWidget(m_avahiStatusLabel, 1, Qt::AlignCenter);
	mdnsGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	QHBoxLayout *flagsLayout = new QHBoxLayout();
	flagsLayout->setContentsMargins(0, 0, 0, 0);
	flagsLayout->setSpacing(8);
	flagsLayout->addWidget(mdnsGroup, 1);
	layout->addLayout(flagsLayout);
	m_proxyModel->setSourceModel(m_model);
	m_proxyModel->setSortRole(NdiAdapterSortRole);
	m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);

	m_tableView->setModel(m_proxyModel);
	//m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
	//m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
	// Disable selection entirely so cells cannot be highlighted or interacted with.
	m_tableView->setSelectionMode(QAbstractItemView::NoSelection);
	// Prevent the view from getting focus (avoid keyboard navigation highlighting)
	m_tableView->setFocusPolicy(Qt::NoFocus);

	m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers); // read-only
	m_tableView->setAlternatingRowColors(true);
	m_tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_tableView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_tableView->verticalHeader()->setVisible(false);

	// Sorting: click a header section to sort asc/desc by that column.
	m_tableView->setSortingEnabled(true);
	m_tableView->sortByColumn(NdiAdapterTableModel::ColNDI, Qt::DescendingOrder);

	QHeaderView *header = m_tableView->horizontalHeader();
	header->setSectionsMovable(true);   // drag-to-reorder columns
	header->setSectionsClickable(true); // required for sort-on-click
	header->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(header, &QHeaderView::customContextMenuRequested, this, &NdiAdapterTableWidget::showHeaderContextMenu);
	connect(header, &QHeaderView::sectionResized, this, &NdiAdapterTableWidget::layoutStateChanged);
	connect(header, &QHeaderView::sectionMoved, this, &NdiAdapterTableWidget::layoutStateChanged);
	connect(header, &QHeaderView::sortIndicatorChanged, this, &NdiAdapterTableWidget::layoutStateChanged);

	layout->setContentsMargins(0, 0, 0, 0);
	// Let the table expand to take available space.
	m_tableView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	layout->addWidget(m_tableView);
	// Give the table stretch so it keeps space when the dialog lays out widgets.
	layout->setStretch(0, 1);

	setLayout(layout);
	m_changeNotifier = new ChangeNotifier([this]() {
		setAdapters(network_monitor->getAllAdapterInfo());
		refreshDiagnosticFlags();
	});
	network_monitor->setNetworkChangeNotifier(m_changeNotifier);
}

void NdiAdapterTableWidget::refreshDiagnosticFlags()
{
	NdiNetworkReport report = network_monitor->getNetworkReport();
	if (m_mdnsBindCheckBox)
		m_mdnsBindCheckBox->setChecked(report.mdnsSocketBindOk);
	if (m_mdnsPortCheckBox)
		m_mdnsPortCheckBox->setChecked(!report.mdnsPortInUse);
	if (m_avahiStatusLabel)
		m_avahiStatusLabel->setText(avahiStatusLabelText(report.avahiStatus));
}
NdiAdapterTableWidget::~NdiAdapterTableWidget()
{
	if (m_changeNotifier) {
		network_monitor->setNetworkChangeNotifier(nullptr);
		delete m_changeNotifier;
		m_changeNotifier = nullptr;
	}
}
void NdiAdapterTableWidget::setAdapters(const std::vector<NdiAdapterInfo> &adapters)
{
	refreshDiagnosticFlags();
	m_model->setAdapters(adapters);
	// Auto-size columns only once, on first population - otherwise every
	// periodic data refresh would stomp on any column width the user dragged.
	if (!m_columnsAutoSized) {
		m_tableView->resizeColumnsToContents();

		// Ensure In/Out Bps columns can contain the fixed-width 11-character
		// field (6 for number + 5 for unit). Compute pixel width from font
		// metrics and apply it after resizing to contents so it is not
		// overwritten. All columns remain freely user-resizable (Interactive,
		// the QHeaderView default) - this just sets a sensible initial width.
		QFontMetrics fm(m_tableView->font());
		int charWidth = fm.horizontalAdvance(QLatin1Char('0'));
		int bpsPixels = charWidth * 11 + 16; // add padding for cell margins
		m_tableView->setColumnWidth(NdiAdapterTableModel::ColInBps, bpsPixels);
		m_tableView->setColumnWidth(NdiAdapterTableModel::ColOutBps, bpsPixels);
		m_tableView->setColumnWidth(NdiAdapterTableModel::ColReceiveSpeed, bpsPixels);
		m_tableView->setColumnWidth(NdiAdapterTableModel::ColTransmitSpeed, bpsPixels);

		m_columnsAutoSized = true;
	}
}

void NdiAdapterTableWidget::showHeaderContextMenu(const QPoint &pos)
{
	QHeaderView *header = m_tableView->horizontalHeader();

	QMenu menu(this);
	for (int logicalIndex = 0; logicalIndex < m_model->columnCount(); ++logicalIndex) {
		const QString label = m_model->headerData(logicalIndex, Qt::Horizontal).toString();
		QAction *action = menu.addAction(label);
		action->setCheckable(true);
		action->setChecked(!header->isSectionHidden(logicalIndex));

		connect(action, &QAction::toggled, this, [this, header, action, logicalIndex](bool visible) {
			if (!visible) {
				// Keep at least one column visible at all times.
				int visibleCount = 0;
				for (int i = 0; i < m_model->columnCount(); ++i) {
					if (i != logicalIndex && !header->isSectionHidden(i))
						++visibleCount;
				}
				if (visibleCount == 0) {
					QSignalBlocker blocker(action);
					action->setChecked(true);
					return;
				}
			}
			header->setSectionHidden(logicalIndex, !visible);
			emit layoutStateChanged();
		});
	}

	menu.exec(header->mapToGlobal(pos));
}

QByteArray NdiAdapterTableWidget::saveLayoutState() const
{
	return m_tableView->horizontalHeader()->saveState();
}

bool NdiAdapterTableWidget::restoreLayoutState(const QByteArray &state)
{
	return m_tableView->horizontalHeader()->restoreState(state);
}
