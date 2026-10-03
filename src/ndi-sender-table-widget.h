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

#include "ndi-sender-report.h"

class QTableView;
class QSortFilterProxyModel;

static constexpr int NdiSenderSortRole = Qt::UserRole + 2;

class NdiSenderTableModel : public QAbstractTableModel {
	Q_OBJECT
public:
	enum Column {
		ColNDIName = 0,
		ColFormat,
		ColReceivers,
		ColDiscoverable,
		ColFps,
		ColTsFps,
		ColFpsDeficit,
		ColSpsDeficit,
		ColJitterRatio,
		ColBudgetUsedBlocking,
		ColMaxSendBlockPct,
		ColBudgetUsedProcessing,
		ColMaxProcessPct,
		ColReceiverChanges,
		ColDiscoverableChanges,
		ColumnCount
	};

	explicit NdiSenderTableModel(QObject *parent = nullptr);

	void setSenders(std::vector<std::shared_ptr<SenderInfo>> senders);
	const SenderInfo *senderAt(int row) const;

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
	std::vector<std::shared_ptr<SenderInfo>> m_senders;
};

class NdiSenderTableWidget : public QWidget {
	Q_OBJECT
public:
	explicit NdiSenderTableWidget(QWidget *parent = nullptr);
	~NdiSenderTableWidget();

	void setSenders(const std::vector<std::shared_ptr<SenderInfo>> &senders);

	QByteArray saveLayoutState() const;
	bool restoreLayoutState(const QByteArray &state);

signals:
	// Emitted whenever a column is resized, hidden/shown, sorted, or
	// reordered, so a listener can persist saveLayoutState().
	void layoutStateChanged();

private slots:
	void showHeaderContextMenu(const QPoint &pos);

private:
	QTableView *m_tableView;
	NdiSenderTableModel *m_model;
	QSortFilterProxyModel *m_proxyModel;
	ChangeNotifier *m_changeNotifier = nullptr;
	// Auto-size columns only once, on first population - otherwise every
	// periodic data refresh would stomp on any column width the user dragged.
	bool m_columnsAutoSized = false;
};
