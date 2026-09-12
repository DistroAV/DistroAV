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

#include "ndi-network-report.h"

class QTableView;
class QSortFilterProxyModel;
class QCheckBox;
class QLabel;

// Role used to store the raw, typed value for a cell so QSortFilterProxyModel
// can sort correctly (numerically/booleanly) instead of comparing the
// human-readable display strings.
static constexpr int NdiAdapterSortRole = Qt::UserRole + 1;

// ---------------------------------------------------------------------------
// Model: wraps a std::vector<NdiAdapterInfo>. luid and lastSample are
// intentionally not exposed as columns.
// ---------------------------------------------------------------------------
class NdiAdapterTableModel : public QAbstractTableModel {
	Q_OBJECT
public:
	enum Column {
		ColNDI = 0,
		ColFriendlyName,
		ColDescription,
		ColIpv4Address,
		ColSubnetMask,
		ColIsUp,
		ColSupportsMulticast,
		ColJoin,
		ColRoundTrip,
		ColLooksVirtual,
		ColNetworkCategory,
		ColFirewallEnabled,
		ColMdnsPortOpen,
		ColNdiPortsOpen,
		ColFilePrinterSharing,
		ColIfType,
		ColInBps,
		ColOutBps,
		ColReceiveSpeed,
		ColTransmitSpeed,
		ColumnCount
	};

	explicit NdiAdapterTableModel(QObject *parent = nullptr);

	void setAdapters(std::vector<NdiAdapterInfo> adapters);
	const NdiAdapterInfo *adapterAt(int row) const;

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
	std::vector<NdiAdapterInfo> m_adapters;
};

// ---------------------------------------------------------------------------
// Widget: a QTableView over the model via a QSortFilterProxyModel.
//  - Columns are user-rearrangeable (drag header sections).
//  - Columns are user-toggleable via a right-click menu on the header.
//  - Clicking a header sorts by that column, ascending/descending.
//  - The view itself is read-only (no cell editing).
// ---------------------------------------------------------------------------
class NdiAdapterTableWidget : public QWidget {
	Q_OBJECT
public:
	explicit NdiAdapterTableWidget(QWidget *parent = nullptr);
	~NdiAdapterTableWidget();

	void setAdapters(const std::vector<NdiAdapterInfo> &adapters);

	// Persist/restore column order, widths, visibility, and sort indicator
	// (e.g. to/from obs_data or QSettings) so layout survives across sessions.
	QByteArray saveLayoutState() const;
	bool restoreLayoutState(const QByteArray &state);

signals:
	// Emitted whenever a column is resized, hidden/shown, sorted, or
	// reordered, so a listener can persist saveLayoutState().
	void layoutStateChanged();

private slots:
	void showHeaderContextMenu(const QPoint &pos);

private:
	void refreshDiagnosticFlags();

	QTableView *m_tableView;
	NdiAdapterTableModel *m_model;
	QSortFilterProxyModel *m_proxyModel;
	ChangeNotifier *m_changeNotifier = nullptr;
	QCheckBox *m_mdnsBindCheckBox = nullptr;
	QCheckBox *m_mdnsPortCheckBox = nullptr;
	QLabel *m_avahiStatusLabel = nullptr;
	// Auto-size columns only once, on first population - otherwise every
	// periodic data refresh would stomp on any column width the user dragged.
	bool m_columnsAutoSized = false;
};
