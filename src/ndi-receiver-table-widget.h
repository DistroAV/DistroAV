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

#include <QWidget>
#include <QAbstractTableModel>
#include <QByteArray>
#include <QPoint>
#include <vector>
#include <memory>

#include "ndi-receiver-report.h"

class QTableView;
class QSortFilterProxyModel;

static constexpr int NdiReceiverSortRole = Qt::UserRole + 3;

class NdiReceiverTableModel : public QAbstractTableModel {
	Q_OBJECT
public:
	enum Column {
		ColObsSourceName = 0,
		ColNDIName,
		ColVideoFramesDropped,
		ColVideoQueueDepth,
		ColFormatDescription,
		ColOsFPS,
		ColTsFPS,
		ColDeficitFPS,
		ColDeficitSPS,
		ColJitterRatio,
		ColBudgetUsedPerFrameCapture,
		ColMaxCapturePct,
		ColBudgetUsedPerFrameProcessing,
		ColMaxProcessPct,
		ColAvDriftMsPerHour,
		ColumnCount
	};

	explicit NdiReceiverTableModel(QObject *parent = nullptr);

	void setReceivers(std::vector<std::shared_ptr<ReceiverInfo>> receivers);
	const ReceiverInfo *receiverAt(int row) const;

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
	std::vector<std::shared_ptr<ReceiverInfo>> m_receivers;
};

class NdiReceiverTableWidget : public QWidget {
	Q_OBJECT
public:
	explicit NdiReceiverTableWidget(QWidget *parent = nullptr);
	~NdiReceiverTableWidget();

	void setReceivers(const std::vector<std::shared_ptr<ReceiverInfo>> &receivers);

	QByteArray saveLayoutState() const;
	bool restoreLayoutState(const QByteArray &state);

private slots:
	void showHeaderContextMenu(const QPoint &pos);

private:
	QTableView *m_tableView;
	NdiReceiverTableModel *m_model;
	QSortFilterProxyModel *m_proxyModel;
	ChangeNotifier *m_changeNotifier = nullptr;
	// Auto-size columns only once, on first population - otherwise every
	// periodic data refresh would stomp on any column width the user dragged.
	bool m_columnsAutoSized = false;
};
