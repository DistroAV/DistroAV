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

// NdiNetworkConfigViewer
//
// Reads the NDI runtime configuration file (ndi-config.v1.json) and displays
// its network-related settings in a QWidget, using a widget type appropriate
// to each field (QLineEdit, QCheckBox, QSpinBox, QListWidget, ...).
//
// Config file locations (per NDI's "Configuration Files" docs):
//   Windows : %ProgramData%\NDI\ndi-config.v1.json
//   macOS   : $HOME/.ndi/ndi-config.v1.json
//   Linux   : $HOME/.ndi/ndi-config.v1.json  (older SDKs: $HOME/.newtek/ndi-config.v1.json)
//
// Schema covered (top-level "ndi" object):
//   machinename            : string   - override the machine's advertised name
//   groups.send            : string   - comma-separated list of groups to send into
//   groups.recv            : string   - comma-separated list of groups to receive from
//   networks.ips           : string   - comma-separated extra unicast IPs to discover from
//   networks.discovery     : string   - discovery server IP
//   tcp.recv.enable        : bool
//   unicast.recv.enable    : bool
//   rudp.recv.enable       : bool
//   multicast.send.enable  : bool
//   multicast.send.ttl     : int
//   multicast.send.netmask : string (dotted quad)
//   multicast.send.netprefix : string (dotted quad)
//   adapters.allowed       : array<string> - NICs (by IP) permitted for NDI traffic
//
// Any other keys present in the file (vendor id, quality overrides, metadata, etc.)
// are preserved untouched on save - we only rewrite the paths we know about.
//
// Build:
//   cmake -B build -S .
//   cmake --build build
//
// Run:
//   ./NdiNetworkConfigViewer [optional path to ndi-config.v1.json]

#include <QApplication>
#include <QWidget>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QRegularExpressionValidator>
#include <QMessageBox>
#include <QInputDialog>
#include <QFrame>
#include <QScrollArea>

#include "obs.h"
#include "plugin-support.h"

namespace {

// ---- Path resolution -------------------------------------------------

QString defaultConfigPath()
{
#if defined(Q_OS_WIN)
	QString programData = qEnvironmentVariable("ProgramData", "C:/ProgramData");
	return programData + "/NDI/ndi-config.v1.json";
#else
	const QString home = QDir::homePath();
	const QString modern = home + "/.ndi/ndi-config.v1.json";
	if (QFile::exists(modern))
		return modern;
	const QString legacy = home + "/.newtek/ndi-config.v1.json";
	if (QFile::exists(legacy))
		return legacy;
	return modern; // fall back to the modern path even if it doesn't exist yet
#endif
}

// ---- Small JSON helpers ------------------------------------------------

QJsonObject childObj(const QJsonObject &parent, const QString &key)
{
	return parent.value(key).toObject();
}

// Walk/create a chain of nested objects and set the final value, leaving
// every sibling key untouched. path = {"ndi","multicast","send","ttl"}
void setJsonPath(QJsonObject &root, const QStringList &path, const QJsonValue &value)
{
	if (path.isEmpty())
		return;
	if (path.size() == 1) {
		root[path.first()] = value;
		return;
	}
	QJsonObject nested = root.value(path.first()).toObject();
	QStringList rest = path;
	rest.removeFirst();
	setJsonPath(nested, rest, value);
	root[path.first()] = nested;
}

} // namespace

// ---- Main widget ---------------------------------------------------------

class NdiNetworkConfigWidget : public QWidget {
public:
	explicit NdiNetworkConfigWidget(QString configPath, QWidget *parent = nullptr)
		: QWidget(parent),
		  m_configPath(std::move(configPath))
	{
		buildUi();
		loadFromDisk();
	}

	// Dump every section/value currently shown to the OBS log, one line per
	// field, matching the style of dumpAdapterReportToLog / dumpSenderReportToLog /
	// dumpReceiverReportToLog in network-monitor.cpp.
	void dumpConfigToLog() const
	{
		const char reportHeader[] = "NDI Config Report";
		obs_log(LOG_INFO, "%s ----------------------------------------", reportHeader);
		obs_log(LOG_INFO, "%s Config file: %s", reportHeader, m_configPath.toUtf8().constData());
		obs_log(LOG_INFO, "%s Machine name override: %s", reportHeader,
			m_machineName->text().toUtf8().constData());
		obs_log(LOG_INFO, "%s Send groups: %s", reportHeader, m_groupsSend->text().toUtf8().constData());
		obs_log(LOG_INFO, "%s Receive groups: %s", reportHeader, m_groupsRecv->text().toUtf8().constData());
		obs_log(LOG_INFO, "%s Extra IPs to discover: %s", reportHeader,
			m_extraIps->text().toUtf8().constData());
		obs_log(LOG_INFO, "%s Discovery server: %s", reportHeader,
			m_discoveryServer->text().toUtf8().constData());
		obs_log(LOG_INFO, "%s Allow TCP receive: %s", reportHeader,
			m_tcpRecvEnable->isChecked() ? "Yes" : "No");
		obs_log(LOG_INFO, "%s Allow unicast UDP receive: %s", reportHeader,
			m_unicastRecvEnable->isChecked() ? "Yes" : "No");
		obs_log(LOG_INFO, "%s Allow RUDP receive: %s", reportHeader,
			m_rudpRecvEnable->isChecked() ? "Yes" : "No");
		obs_log(LOG_INFO, "%s Enable multicast send: %s", reportHeader,
			m_multicastEnable->isChecked() ? "Yes" : "No");
		obs_log(LOG_INFO, "%s Multicast TTL: %d", reportHeader, m_multicastTtl->value());
		obs_log(LOG_INFO, "%s Multicast Netmask: %s", reportHeader,
			m_multicastNetmask->text().toUtf8().constData());
		obs_log(LOG_INFO, "%s Multicast Net prefix: %s", reportHeader,
			m_multicastNetprefix->text().toUtf8().constData());
		if (m_allowedAdapters->count() == 0) {
			obs_log(LOG_INFO, "%s Allowed Adapters: (none)", reportHeader);
		} else {
			for (int i = 0; i < m_allowedAdapters->count(); ++i)
				obs_log(LOG_INFO, "%s Allowed Adapter: %s", reportHeader,
					m_allowedAdapters->item(i)->text().toUtf8().constData());
		}
	}

	// Plain-ASCII dump of every section/value currently shown, for the dialog's
	// shared Copy button. No borders/alignment - just "Section" headings and
	// "Label: value" lines.
	QString getFormattedConfigText() const
	{
		auto yesNo = [](bool b) {
			return QString(b ? "Yes" : "No");
		};

		QStringList lines;
		lines << "NDI Network Configuration";
		lines << ("Config file: " + m_configPath);
		lines << "";

		lines << "Machine Identity";
		lines << ("  Machine name override: " + m_machineName->text());
		lines << "";

		lines << "Groups";
		lines << ("  Send groups: " + m_groupsSend->text());
		lines << ("  Receive groups: " + m_groupsRecv->text());
		lines << "";

		lines << "Networks && Discovery";
		lines << ("  Extra IPs to discover: " + m_extraIps->text());
		lines << ("  Discovery server: " + m_discoveryServer->text());
		lines << "";

		lines << "Connection Modes";
		lines << ("  Allow TCP receive: " + yesNo(m_tcpRecvEnable->isChecked()));
		lines << ("  Allow unicast UDP receive: " + yesNo(m_unicastRecvEnable->isChecked()));
		lines << ("  Allow RUDP receive: " + yesNo(m_rudpRecvEnable->isChecked()));
		lines << "";

		lines << "Multicast (send)";
		lines << ("  Enable multicast send: " + yesNo(m_multicastEnable->isChecked()));
		lines << ("  TTL: " + QString::number(m_multicastTtl->value()));
		lines << ("  Netmask: " + m_multicastNetmask->text());
		lines << ("  Net prefix: " + m_multicastNetprefix->text());
		lines << "";

		lines << "Allowed Network Adapters (IPs)";
		if (m_allowedAdapters->count() == 0) {
			lines << "  (none)";
		} else {
			for (int i = 0; i < m_allowedAdapters->count(); ++i)
				lines << ("  " + m_allowedAdapters->item(i)->text());
		}

		return lines.join('\n');
	}

private:
	// Widgets - one per field, each typed for what it actually represents.
	QLabel *m_pathLabel = nullptr;
	QLabel *m_statusLabel = nullptr;

	QLineEdit *m_machineName = nullptr;

	QLineEdit *m_groupsSend = nullptr;
	QLineEdit *m_groupsRecv = nullptr;

	QLineEdit *m_extraIps = nullptr;
	QLineEdit *m_discoveryServer = nullptr;

	QCheckBox *m_tcpRecvEnable = nullptr;
	QCheckBox *m_unicastRecvEnable = nullptr;
	QCheckBox *m_rudpRecvEnable = nullptr;

	QCheckBox *m_multicastEnable = nullptr;
	QSpinBox *m_multicastTtl = nullptr;
	QLineEdit *m_multicastNetmask = nullptr;
	QLineEdit *m_multicastNetprefix = nullptr;

	QListWidget *m_allowedAdapters = nullptr;

	QString m_configPath;
	QJsonObject m_rootJson; // full document, so unrelated keys survive a save

	static QLineEdit *ipv4LineEdit(QWidget *parent = nullptr)
	{
		auto *edit = new QLineEdit(parent);
		static const QRegularExpression ipRe(R"(^(\d{1,3}\.){3}\d{1,3}$)");
		edit->setValidator(new QRegularExpressionValidator(ipRe, edit));
		edit->setPlaceholderText("e.g. 255.255.0.0");
		return edit;
	}

	void buildUi()
	{
		// Everything lives in an inner content widget placed inside a QScrollArea,
		// so the tab (and therefore the dialog) can be shrunk arbitrarily small -
		// the content just scrolls instead of forcing a large minimum size.
		auto *outerLayout = new QVBoxLayout(this);
		outerLayout->setContentsMargins(0, 0, 0, 0);

		auto *scrollArea = new QScrollArea(this);
		scrollArea->setWidgetResizable(true);
		scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

		auto *contentWidget = new QWidget(scrollArea);
		auto *root = new QVBoxLayout(contentWidget);

		// -- header: which file we're looking at --
		m_pathLabel = new QLabel(this);
		m_pathLabel->setWordWrap(true);
		m_pathLabel->setStyleSheet("font-weight: 600;");
		root->addWidget(m_pathLabel);

		auto *columns = new QHBoxLayout();
		root->addLayout(columns);

		auto *leftCol = new QVBoxLayout();
		auto *rightCol = new QVBoxLayout();
		columns->addLayout(leftCol, 1);
		columns->addLayout(rightCol, 1);

		// -- Identity --
		auto *identityBox = new QGroupBox("Machine Identity", this);
		auto *identityForm = new QFormLayout(identityBox);
		m_machineName = new QLineEdit(identityBox);
		m_machineName->setPlaceholderText("(use system hostname)");
		m_machineName->setReadOnly(true);
		identityForm->addRow("Machine name override:", m_machineName);
		leftCol->addWidget(identityBox);

		// -- Groups --
		auto *groupsBox = new QGroupBox("Groups", this);
		auto *groupsForm = new QFormLayout(groupsBox);
		m_groupsSend = new QLineEdit(groupsBox);
		m_groupsSend->setPlaceholderText("Public,");
		m_groupsSend->setReadOnly(true);
		m_groupsRecv = new QLineEdit(groupsBox);
		m_groupsRecv->setPlaceholderText("Public,");
		m_groupsRecv->setReadOnly(true);
		groupsForm->addRow("Send groups:", m_groupsSend);
		groupsForm->addRow("Receive groups:", m_groupsRecv);
		leftCol->addWidget(groupsBox);

		// -- Networks / discovery --
		auto *networksBox = new QGroupBox("Networks && Discovery", this);
		auto *networksForm = new QFormLayout(networksBox);
		m_extraIps = new QLineEdit(networksBox);
		m_extraIps->setPlaceholderText("192.168.1.20,192.168.1.21");
		m_extraIps->setReadOnly(true);
		m_discoveryServer = new QLineEdit(networksBox);
		m_discoveryServer->setPlaceholderText("(none)");
		m_discoveryServer->setReadOnly(true);
		networksForm->addRow("Extra IPs to discover:", m_extraIps);
		networksForm->addRow("Discovery server:", m_discoveryServer);
		leftCol->addWidget(networksBox);

		// -- Connection modes --
		auto *modesBox = new QGroupBox("Connection Modes", this);
		auto *modesForm = new QFormLayout(modesBox);
		m_tcpRecvEnable = new QCheckBox("Allow TCP receive", modesBox);
		m_tcpRecvEnable->setEnabled(false);
		m_unicastRecvEnable = new QCheckBox("Allow unicast UDP receive", modesBox);
		m_unicastRecvEnable->setEnabled(false);
		m_rudpRecvEnable = new QCheckBox("Allow RUDP receive", modesBox);
		m_rudpRecvEnable->setEnabled(false);
		modesForm->addRow(m_tcpRecvEnable);
		modesForm->addRow(m_unicastRecvEnable);
		modesForm->addRow(m_rudpRecvEnable);
		leftCol->addWidget(modesBox);
		leftCol->addStretch(1);

		// -- Multicast --
		auto *multicastBox = new QGroupBox("Multicast (send)", this);
		auto *multicastForm = new QFormLayout(multicastBox);
		m_multicastEnable = new QCheckBox("Enable multicast send", multicastBox);
		m_multicastEnable->setEnabled(false);
		m_multicastTtl = new QSpinBox(multicastBox);
		m_multicastTtl->setRange(0, 255);
		m_multicastTtl->setEnabled(false);
		m_multicastNetmask = ipv4LineEdit(multicastBox);
		m_multicastNetmask->setReadOnly(true);
		m_multicastNetprefix = ipv4LineEdit(multicastBox);
		m_multicastNetprefix->setReadOnly(true);
		multicastForm->addRow(m_multicastEnable);
		multicastForm->addRow("TTL:", m_multicastTtl);
		multicastForm->addRow("Netmask:", m_multicastNetmask);
		multicastForm->addRow("Net prefix:", m_multicastNetprefix);
		rightCol->addWidget(multicastBox);

		// -- Adapters --
		auto *adaptersBox = new QGroupBox("Allowed Network Adapters (IPs)", this);
		auto *adaptersLayout = new QVBoxLayout(adaptersBox);
		m_allowedAdapters = new QListWidget(adaptersBox);
		// Make list read-only (no editing of items)
		m_allowedAdapters->setEditTriggers(QAbstractItemView::NoEditTriggers);
		adaptersLayout->addWidget(m_allowedAdapters);

		rightCol->addWidget(adaptersBox);
		rightCol->addStretch(1);

		// -- footer: reload / save / status --
		auto *line = new QFrame(this);
		line->setFrameShape(QFrame::HLine);
		root->addWidget(line);

		auto *footer = new QHBoxLayout();
		auto *reloadBtn = new QPushButton("Reload from Disk", this);

		m_statusLabel = new QLabel(this);
		m_statusLabel->setStyleSheet("color: #666;");
		footer->addWidget(reloadBtn);
		footer->addStretch(1);
		footer->addWidget(m_statusLabel);
		root->addLayout(footer);

		connect(reloadBtn, &QPushButton::clicked, this, &NdiNetworkConfigWidget::loadFromDisk);

		scrollArea->setWidget(contentWidget);
		outerLayout->addWidget(scrollArea);

		setWindowTitle("NDI Network Configuration");
		resize(760, 560);
	}

	void loadFromDisk()
	{
		m_pathLabel->setText("Config file: " + m_configPath);

		QFile file(m_configPath);
		if (!file.exists()) {
			m_rootJson = QJsonObject();
			m_statusLabel->setText("File not found - showing NDI defaults.");
			applyDefaults();
			return;
		}
		if (!file.open(QIODevice::ReadOnly)) {
			m_statusLabel->setText("Could not open file: " + file.errorString());
			return;
		}

		QJsonParseError parseError{};
		const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
		file.close();

		if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
			m_statusLabel->setText("JSON parse error: " + parseError.errorString());
			return;
		}

		m_rootJson = doc.object();
		populateFromJson();
		m_statusLabel->setText("Loaded.");
	}

	void applyDefaults()
	{
		// Viewer mode: leave fields blank when no file exists yet.
		m_machineName->clear();
		m_groupsSend->clear();
		m_groupsRecv->clear();
		m_extraIps->setText("(none)");
		m_discoveryServer->clear();
		m_tcpRecvEnable->setChecked(false);
		m_unicastRecvEnable->setChecked(false);
		m_rudpRecvEnable->setChecked(false);
		m_multicastEnable->setChecked(false);
		m_multicastTtl->setValue(0);
		m_multicastNetmask->clear();
		m_multicastNetprefix->clear();
		m_allowedAdapters->clear();
	}

	void populateFromJson()
	{
		const QJsonObject ndi = childObj(m_rootJson, "ndi");

		m_machineName->setText(ndi.value("machinename").toString());

		const QJsonObject groups = childObj(ndi, "groups");
		m_groupsSend->setText(groups.value("send").toString());
		m_groupsRecv->setText(groups.value("recv").toString());

		const QJsonObject networks = childObj(ndi, "networks");
		const QString extraIps = networks.value("ips").toString();
		m_extraIps->setText(extraIps.isEmpty() ? "(none)" : extraIps);
		m_discoveryServer->setText(networks.value("discovery").toString());

		const QJsonObject tcp = childObj(ndi, "tcp");
		m_tcpRecvEnable->setChecked(childObj(tcp, "recv").value("enable").toBool(true));

		const QJsonObject unicast = childObj(ndi, "unicast");
		m_unicastRecvEnable->setChecked(childObj(unicast, "recv").value("enable").toBool(true));

		const QJsonObject rudp = childObj(ndi, "rudp");
		m_rudpRecvEnable->setChecked(childObj(rudp, "recv").value("enable").toBool(true));

		const QJsonObject multicast = childObj(ndi, "multicast");
		const QJsonObject mcSend = childObj(multicast, "send");
		m_multicastEnable->setChecked(mcSend.value("enable").toBool(true));
		m_multicastTtl->setValue(mcSend.value("ttl").toInt(1));
		m_multicastNetmask->setText(mcSend.value("netmask").toString("255.255.0.0"));
		m_multicastNetprefix->setText(mcSend.value("netprefix").toString("239.255.0.0"));

		m_allowedAdapters->clear();
		const QJsonArray allowed = childObj(ndi, "adapters").value("allowed").toArray();
		for (const QJsonValue v : allowed)
			m_allowedAdapters->addItem(v.toString());
	}

	void saveToDisk()
	{
		// Saving is disabled in viewer mode; do nothing.
		QMessageBox::information(this, "Read-only Viewer", "Saving is disabled in the NDI config viewer.");
	}
};

/*
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const QString configPath = (argc > 1) ? QString::fromLocal8Bit(argv[1])
                                           : defaultConfigPath();

    NdiNetworkConfigWidget widget(configPath);
    widget.show();

    return app.exec();
}
*/
