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

#include "ndi-receiver-table-widget.h"
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
QString msFromNs(int64_t ns)
{
	if (ns == 0)
		return "0";
	double ms = ns / 1000000.0;
	return QString::number(ms, 'f', 2);
}

QString percentFromFraction(double fraction)
{
	return QString::number(fraction * 100.0, 'f', 2);
}
} // namespace

// Model
NdiReceiverTableModel::NdiReceiverTableModel(QObject *parent) : QAbstractTableModel(parent) {}

void NdiReceiverTableModel::setReceivers(std::vector<std::shared_ptr<ReceiverInfo>> receivers)
{
	beginResetModel();
	m_receivers = std::move(receivers);
	endResetModel();
}

const ReceiverInfo *NdiReceiverTableModel::receiverAt(int row) const
{
	if (row < 0 || row >= static_cast<int>(m_receivers.size()))
		return nullptr;
	return m_receivers[row].get();
}

int NdiReceiverTableModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;
	return static_cast<int>(m_receivers.size());
}

int NdiReceiverTableModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;
	return ColumnCount;
}

QVariant NdiReceiverTableModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();
	int row = index.row();
	int col = index.column();
	if (row < 0 || row >= static_cast<int>(m_receivers.size()))
		return QVariant();
	const ReceiverInfo *r = m_receivers[row].get();
	if (!r)
		return QVariant();

	if (role == Qt::DisplayRole) {
		auto report = r->getReportSnapshot();
		switch (col) {
		case ColObsSourceName:
			return QString::fromStdString(report.obs_source_name);
		case ColNDIName:
			return QString::fromStdString(r->get_ndi_name());
		case ColVideoFramesDropped:
			return QVariant((int)report.video_frames_dropped);
		case ColVideoQueueDepth:
			return QVariant((int)report.video_queue_size);
		case ColFormatDescription:
			return QString::fromStdString(report.format_description);
		case ColOsFPS:
			return QString::number(report.osFPS, 'f', 2);
		case ColTsFPS:
			return QString::number(report.tsFPS, 'f', 2);
		case ColDeficitFPS:
			return percentFromFraction(report.deficit_fps);
		case ColDeficitSPS:
			return percentFromFraction(report.deficit_sps);
		case ColJitterRatio:
			return QString::number(report.jitter_ratio, 'f', 2);
		case ColBudgetUsedPerFrameCapture:
			return percentFromFraction(report.budget_used_per_frame_capture);
		case ColMaxCapturePct:
			return percentFromFraction(report.max_capture_pct);
		case ColBudgetUsedPerFrameProcessing:
			return percentFromFraction(report.budget_used_per_frame_processing);
		case ColMaxProcessPct:
			return percentFromFraction(report.max_process_pct);
		case ColAvDriftMsPerHour:
			return msFromNs(report.av_drift_ns_per_hour);
		default:
			return QVariant();
		}
	}
	if (role == NdiReceiverSortRole) {
		auto report = r->getReportSnapshot();
		switch (col) {
		case ColObsSourceName:
			return QString::fromStdString(report.obs_source_name);
		case ColNDIName:
			return QString::fromStdString(r->get_ndi_name());
		case ColVideoFramesDropped:
			return QVariant((qulonglong)report.video_frames_dropped);
		case ColVideoQueueDepth:
			return QVariant((qulonglong)report.video_queue_size);
		case ColFormatDescription:
			return QString::fromStdString(report.format_description);
		case ColOsFPS:
			return QVariant(report.osFPS);
		case ColTsFPS:
			return QVariant(report.tsFPS);
		case ColDeficitFPS:
			return QVariant(report.deficit_fps);
		case ColDeficitSPS:
			return QVariant(report.deficit_sps);
		case ColJitterRatio:
			return QVariant(report.jitter_ratio);
		case ColBudgetUsedPerFrameCapture:
			return QVariant(report.budget_used_per_frame_capture);
		case ColMaxCapturePct:
			return QVariant(report.max_capture_pct);
		case ColBudgetUsedPerFrameProcessing:
			return QVariant(report.budget_used_per_frame_processing);
		case ColMaxProcessPct:
			return QVariant(report.max_process_pct);
		case ColAvDriftMsPerHour:
			return QVariant(report.av_drift_ns_per_hour / 1000000.0);
		default:
			return QVariant();
		}
	}
	if (role == Qt::TextAlignmentRole) {
		switch (index.column()) {
		case ColVideoFramesDropped:
		case ColVideoQueueDepth:
		case ColOsFPS:
		case ColTsFPS:
		case ColDeficitFPS:
		case ColDeficitSPS:
		case ColJitterRatio:
		case ColBudgetUsedPerFrameCapture:
		case ColMaxCapturePct:
		case ColBudgetUsedPerFrameProcessing:
		case ColMaxProcessPct:
		case ColAvDriftMsPerHour:
			return QVariant(Qt::AlignRight | Qt::AlignVCenter);
		default:
			return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
		}
	}
	return QVariant();
}

QVariant NdiReceiverTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
		return QAbstractTableModel::headerData(section, orientation, role);
	switch (section) {
	case ColObsSourceName:
		return QStringLiteral("OBS Source Name");
	case ColNDIName:
		return QStringLiteral("NDI Name");
	case ColVideoFramesDropped:
		return QStringLiteral("Dropped");
	case ColVideoQueueDepth:
		return QStringLiteral("Queued");
	case ColFormatDescription:
		return QStringLiteral("Format");
	case ColOsFPS:
		return QStringLiteral("FPS");
	case ColTsFPS:
		return QStringLiteral("TS FPS");
	case ColDeficitFPS:
		return QStringLiteral("FPS Deficit %");
	case ColDeficitSPS:
		return QStringLiteral("SPS Deficit %");
	case ColJitterRatio:
		return QStringLiteral("Jitter Ratio");
	case ColBudgetUsedPerFrameCapture:
		return QStringLiteral("Capture %");
	case ColMaxCapturePct:
		return QStringLiteral("Max Capture %");
	case ColBudgetUsedPerFrameProcessing:
		return QStringLiteral("Process %");
	case ColMaxProcessPct:
		return QStringLiteral("Max Process %");
	case ColAvDriftMsPerHour:
		return QStringLiteral("Drift ms/hr");
	default:
		return QVariant();
	}
}

Qt::ItemFlags NdiReceiverTableModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

// Widget
NdiReceiverTableWidget::NdiReceiverTableWidget(QWidget *parent)
	: QWidget(parent),
	  m_tableView(new QTableView(this)),
	  m_model(new NdiReceiverTableModel(this)),
	  m_proxyModel(new QSortFilterProxyModel(this))
{
	m_proxyModel->setSourceModel(m_model);
	m_proxyModel->setSortRole(NdiReceiverSortRole);
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
	m_tableView->sortByColumn(NdiReceiverTableModel::ColObsSourceName, Qt::AscendingOrder);

	QHeaderView *header = m_tableView->horizontalHeader();
	header->setSectionsMovable(true);
	header->setSectionsClickable(true);
	header->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(header, &QHeaderView::customContextMenuRequested, this, &NdiReceiverTableWidget::showHeaderContextMenu);
	connect(header, &QHeaderView::sectionResized, this, &NdiReceiverTableWidget::layoutStateChanged);
	connect(header, &QHeaderView::sectionMoved, this, &NdiReceiverTableWidget::layoutStateChanged);
	connect(header, &QHeaderView::sortIndicatorChanged, this, &NdiReceiverTableWidget::layoutStateChanged);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	m_tableView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	layout->addWidget(m_tableView);
	layout->setStretch(0, 1);

	setLayout(layout);
	QPointer<NdiReceiverTableWidget> guard(this);
	m_changeNotifier = new ChangeNotifier([guard]() {
		// Guard against the notifier firing (via a queued connection) after this
		// widget (and its parent dialog) has already been destroyed.
		if (guard)
			guard->setReceivers(network_monitor->getAllReceiverInfo());
	});
	network_monitor->setReceiverChangeNotifier(m_changeNotifier);
}

NdiReceiverTableWidget::~NdiReceiverTableWidget()
{
	if (m_changeNotifier) {
		network_monitor->setReceiverChangeNotifier(nullptr);
		// Delete synchronously (rather than deleteLater) so that any events
		// already queued for this notifier (e.g. a pending changed() signal
		// posted from the monitor thread) are discarded by Qt immediately,
		// instead of being delivered after this widget starts being destroyed.
		delete m_changeNotifier;
		m_changeNotifier = nullptr;
	}
}

void NdiReceiverTableWidget::setReceivers(const std::vector<std::shared_ptr<ReceiverInfo>> &receivers)
{
	m_model->setReceivers(receivers);
	if (!m_columnsAutoSized) {
		m_tableView->resizeColumnsToContents();
		m_columnsAutoSized = true;
	}
}

void NdiReceiverTableWidget::showHeaderContextMenu(const QPoint &pos)
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

QByteArray NdiReceiverTableWidget::saveLayoutState() const
{
	return m_tableView->horizontalHeader()->saveState();
}

bool NdiReceiverTableWidget::restoreLayoutState(const QByteArray &state)
{
	return m_tableView->horizontalHeader()->restoreState(state);
}
