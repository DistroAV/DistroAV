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

#include "ndi-sender-table-widget.h"
#include "plugin-main.h"
#include <QTableView>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QFontMetrics>
#include <QPointer>

namespace {
// Display a 0.0-1.0 fraction as a percentage string, e.g. 0.0432 -> "4.32".
QString percentFromFraction(double fraction)
{
	return QString::number(fraction * 100.0, 'f', 2);
}
} // namespace

// Model
NdiSenderTableModel::NdiSenderTableModel(QObject *parent) : QAbstractTableModel(parent) {}

void NdiSenderTableModel::setSenders(std::vector<std::shared_ptr<SenderInfo>> senders)
{
	beginResetModel();
	m_senders = std::move(senders);
	endResetModel();
}

const SenderInfo *NdiSenderTableModel::senderAt(int row) const
{
	if (row < 0 || row >= static_cast<int>(m_senders.size()))
		return nullptr;
	return m_senders[row].get();
}

int NdiSenderTableModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;
	return static_cast<int>(m_senders.size());
}

int NdiSenderTableModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;
	return ColumnCount;
}

QVariant NdiSenderTableModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();
	int row = index.row();
	int col = index.column();
	if (row < 0 || row >= static_cast<int>(m_senders.size()))
		return QVariant();
	const SenderInfo *s = m_senders[row].get();
	if (!s)
		return QVariant();

	if (role == Qt::DisplayRole) {
		auto report = s->getReportSnapshot(); // not ideally thread-safe but acceptable for viewer
		switch (col) {
		case ColNDIName:
			return QString::fromStdString(s->get_ndi_name());
		case ColFormat:
			return QString::fromStdString(report.format_description);
		case ColReceivers:
			return QVariant((qulonglong)report.receivers);
		case ColDiscoverable:
			return report.discoverable ? QString("Yes") : QString("No");
		case ColFps:
			return QString::number(report.osFPS, 'f', 2);
		case ColTsFps:
			return QString::number(report.tsFPS, 'f', 2);
		case ColFpsDeficit:
			return percentFromFraction(report.deficit_fps);
		case ColSpsDeficit:
			return percentFromFraction(report.deficit_sps);
		case ColJitterRatio:
			return QString::number(report.jitter_ratio, 'f', 2);
		case ColBudgetUsedBlocking:
			return percentFromFraction(report.budget_used_per_frame_blocking);
		case ColMaxSendBlockPct:
			return percentFromFraction(report.max_send_block_pct);
		case ColBudgetUsedProcessing:
			return percentFromFraction(report.budget_used_per_frame_processing);
		case ColMaxProcessPct:
			return percentFromFraction(report.max_process_pct);
		case ColReceiverChanges:
			return QVariant((qulonglong)report.receiver_changes);
		case ColDiscoverableChanges:
			return QVariant((qulonglong)report.discoverable_changes);
		default:
			return QVariant();
		}
	}
	// Raw, typed values for sorting (QSortFilterProxyModel is configured with
	// setSortRole(NdiSenderSortRole)). Without this, sorting falls back to the
	// formatted display strings and produces lexical ordering (e.g. "10"
	// sorting before "9") instead of numeric ordering.
	if (role == NdiSenderSortRole) {
		auto report = s->getReportSnapshot();
		switch (col) {
		case ColNDIName:
			return QString::fromStdString(s->get_ndi_name());
		case ColFormat:
			return QString::fromStdString(report.format_description);
		case ColReceivers:
			return QVariant((qulonglong)report.receivers);
		case ColDiscoverable:
			return QVariant(report.discoverable);
		case ColFps:
			return QVariant(report.osFPS);
		case ColTsFps:
			return QVariant(report.tsFPS);
		case ColFpsDeficit:
			return QVariant(report.deficit_fps);
		case ColSpsDeficit:
			return QVariant(report.deficit_sps);
		case ColJitterRatio:
			return QVariant(report.jitter_ratio);
		case ColBudgetUsedBlocking:
			return QVariant(report.budget_used_per_frame_blocking);
		case ColMaxSendBlockPct:
			return QVariant(report.max_send_block_pct);
		case ColBudgetUsedProcessing:
			return QVariant(report.budget_used_per_frame_processing);
		case ColMaxProcessPct:
			return QVariant(report.max_process_pct);
		case ColReceiverChanges:
			return QVariant((qulonglong)report.receiver_changes);
		case ColDiscoverableChanges:
			return QVariant((qulonglong)report.discoverable_changes);
		default:
			return QVariant();
		}
	}
	if (role == Qt::TextAlignmentRole) {
		switch (index.column()) {
		case ColNDIName:
		case ColFormat:
		case ColDiscoverable:
			return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
		default:
			return QVariant(Qt::AlignRight | Qt::AlignVCenter);
		}
	}
	return QVariant();
}

QVariant NdiSenderTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
		return QAbstractTableModel::headerData(section, orientation, role);
	switch (section) {
	case ColNDIName:
		return QStringLiteral("NDI name");
	case ColFormat:
		return QStringLiteral("Format");
	case ColReceivers:
		return QStringLiteral("Recvrs");
	case ColDiscoverable:
		return QStringLiteral("Disc?");
	case ColFps:
		return QStringLiteral("FPS");
	case ColTsFps:
		return QStringLiteral("TS FPS");
	case ColFpsDeficit:
		return QStringLiteral("FPS Deficit %");
	case ColSpsDeficit:
		return QStringLiteral("SPS Deficit %");
	case ColJitterRatio:
		return QStringLiteral("Jitter Ratio");
	case ColBudgetUsedBlocking:
		return QStringLiteral("Send Block %");
	case ColMaxSendBlockPct:
		return QStringLiteral("Max Send Block %");
	case ColBudgetUsedProcessing:
		return QStringLiteral("Processing %");
	case ColMaxProcessPct:
		return QStringLiteral("Max Processing %");
	case ColReceiverChanges:
		return QStringLiteral("Recvr Changes");
	case ColDiscoverableChanges:
		return QStringLiteral("Disc Changes");
	default:
		return QVariant();
	}
}

Qt::ItemFlags NdiSenderTableModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

// Widget
NdiSenderTableWidget::NdiSenderTableWidget(QWidget *parent)
	: QWidget(parent),
	  m_tableView(new QTableView(this)),
	  m_model(new NdiSenderTableModel(this)),
	  m_proxyModel(new QSortFilterProxyModel(this))
{
	m_proxyModel->setSourceModel(m_model);
	m_proxyModel->setSortRole(NdiSenderSortRole);
	m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);

	m_tableView->setModel(m_proxyModel);
	m_tableView->setSelectionMode(QAbstractItemView::NoSelection);
	m_tableView->setFocusPolicy(Qt::NoFocus);
	m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_tableView->setAlternatingRowColors(true);
	m_tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_tableView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_tableView->verticalHeader()->setVisible(false);
	m_tableView->setSortingEnabled(true);
	m_tableView->sortByColumn(NdiSenderTableModel::ColNDIName, Qt::AscendingOrder);

	QHeaderView *header = m_tableView->horizontalHeader();
	header->setSectionsMovable(true);
	header->setSectionsClickable(true);
	header->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(header, &QHeaderView::customContextMenuRequested, this, &NdiSenderTableWidget::showHeaderContextMenu);
	connect(header, &QHeaderView::sectionResized, this, &NdiSenderTableWidget::layoutStateChanged);
	connect(header, &QHeaderView::sectionMoved, this, &NdiSenderTableWidget::layoutStateChanged);
	connect(header, &QHeaderView::sortIndicatorChanged, this, &NdiSenderTableWidget::layoutStateChanged);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	m_tableView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	layout->addWidget(m_tableView);
	layout->setStretch(0, 1);

	setLayout(layout);
	QPointer<NdiSenderTableWidget> guard(this);
	m_changeNotifier = new ChangeNotifier([guard]() {
		// Guard against the notifier firing (via a queued connection) after this
		// widget (and its parent dialog) has already been destroyed.
		if (guard)
			guard->setSenders(network_monitor->getAllSenderInfo());
	});

	network_monitor->setSenderChangeNotifier(m_changeNotifier);
}

NdiSenderTableWidget::~NdiSenderTableWidget()
{
	if (m_changeNotifier) {
		network_monitor->setSenderChangeNotifier(nullptr);
		// Delete synchronously (rather than deleteLater) so that any events
		// already queued for this notifier (e.g. a pending changed() signal
		// posted from the monitor thread) are discarded by Qt immediately,
		// instead of being delivered after this widget starts being destroyed.
		delete m_changeNotifier;
		m_changeNotifier = nullptr;
	}
}

void NdiSenderTableWidget::setSenders(const std::vector<std::shared_ptr<SenderInfo>> &senders)
{
	m_model->setSenders(senders);
	if (!m_columnsAutoSized) {
		m_tableView->resizeColumnsToContents();
		m_columnsAutoSized = true;
	}
}

void NdiSenderTableWidget::showHeaderContextMenu(const QPoint &pos)
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

QByteArray NdiSenderTableWidget::saveLayoutState() const
{
	return m_tableView->horizontalHeader()->saveState();
}

bool NdiSenderTableWidget::restoreLayoutState(const QByteArray &state)
{
	return m_tableView->horizontalHeader()->restoreState(state);
}
