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

#include "network-monitor-dialog.h"
#include "network-monitor.h"
#include "ndi-adapter-table-widget.h"
#include "ndi-sender-table-widget.h"
#include "ndi-receiver-table-widget.h"
#include "ndi-config-widget.h"
#include "config.h"

#include <obs-frontend-api.h>

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QPushButton>
#include <QCheckBox>
#include <QSizePolicy>
#include <QWidget>
#include <QTabWidget>
#include <QPointer>
#include <QClipboard>
#include <QGuiApplication>
#include <QScreen>
#include <QEvent>
#include <QMessageBox>

namespace {
// Watches the dialog for moves/resizes and persists its geometry to the user
// config immediately, so the last position/size survives an OBS restart.
class NetworkMonitorGeometryWatcher : public QObject {
public:
	using QObject::QObject;

protected:
	bool eventFilter(QObject *watched, QEvent *event) override
	{
		if (event->type() == QEvent::Move || event->type() == QEvent::Resize) {
			if (auto *w = qobject_cast<QWidget *>(watched)) {
				Config::Current()->SetNetworkMonitorGeometry(w->x(), w->y(), w->width(), w->height());
			}
		}
		return QObject::eventFilter(watched, event);
	}
};
} // namespace

// This is used as a global by ndi-source.cpp / plugin-main.h; declared extern there.
extern NetworkMonitor *network_monitor;
static QPointer<QDialog> s_dialog;
// Set just before close_network_monitor_dialog() closes the dialog for OBS
// shutdown (OBS_FRONTEND_EVENT_EXIT), so the destroyed handler below knows not
// to clear NetworkMonitorUp in that case - otherwise the dialog would never
// auto-reopen on the next launch, since a normal OBS exit would always look
// identical to the user explicitly closing it.
static bool s_closingForAppExit = false;
bool open_network_monitor_dialog()
{
	Config::Current()->NetworkMonitorUp(true);

	if (s_dialog) {
		s_dialog->show();
		s_dialog->raise();
		s_dialog->activateWindow();
		return true;
	}

	// Get the NdiNetworkMonitor singleton instance and get the current network report.
	NdiNetworkReport report = network_monitor->getNetworkReport();

	// non-blocking dialog that will be deleted when closed
	QDialog *dialog = new QDialog();
	s_dialog = dialog;
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setWindowTitle("Network Monitor");
	dialog->setWindowModality(Qt::NonModal);
	dialog->setMinimumSize(400, 100);
	dialog->setSizeGripEnabled(true);
	QObject::connect(dialog, &QObject::destroyed, []() {
		s_dialog = nullptr;
		if (!s_closingForAppExit)
			Config::Current()->NetworkMonitorUp(false);
		s_closingForAppExit = false;
	});

	QVBoxLayout *layout = new QVBoxLayout(dialog);

	QTabWidget *tabWidget = new QTabWidget(dialog);

	NdiAdapterTableWidget *adapterTable = new NdiAdapterTableWidget(tabWidget);
	adapterTable->setAdapters(report.adapters);
	tabWidget->addTab(adapterTable, dialog->tr("Adapters"));
	const QString savedAdapterColumns = Config::Current()->NetworkMonitorAdapterColumns();
	if (!savedAdapterColumns.isEmpty())
		adapterTable->restoreLayoutState(QByteArray::fromBase64(savedAdapterColumns.toLatin1()));
	QObject::connect(adapterTable, &NdiAdapterTableWidget::layoutStateChanged, adapterTable, [adapterTable]() {
		Config::Current()->NetworkMonitorAdapterColumns(
			QString::fromLatin1(adapterTable->saveLayoutState().toBase64()));
	});

	NdiSenderTableWidget *senderTable = new NdiSenderTableWidget(tabWidget);
	senderTable->setSenders(network_monitor->getAllSenderInfo());
	tabWidget->addTab(senderTable, dialog->tr("Senders"));
	const QString savedSenderColumns = Config::Current()->NetworkMonitorSenderColumns();
	if (!savedSenderColumns.isEmpty())
		senderTable->restoreLayoutState(QByteArray::fromBase64(savedSenderColumns.toLatin1()));
	QObject::connect(senderTable, &NdiSenderTableWidget::layoutStateChanged, senderTable, [senderTable]() {
		Config::Current()->NetworkMonitorSenderColumns(
			QString::fromLatin1(senderTable->saveLayoutState().toBase64()));
	});

	NdiReceiverTableWidget *receiverTable = new NdiReceiverTableWidget(tabWidget);
	receiverTable->setReceivers(network_monitor->getAllReceiverInfo());
	tabWidget->addTab(receiverTable, dialog->tr("Receivers"));
	const QString savedReceiverColumns = Config::Current()->NetworkMonitorReceiverColumns();
	if (!savedReceiverColumns.isEmpty())
		receiverTable->restoreLayoutState(QByteArray::fromBase64(savedReceiverColumns.toLatin1()));
	QObject::connect(receiverTable, &NdiReceiverTableWidget::layoutStateChanged, receiverTable, [receiverTable]() {
		Config::Current()->NetworkMonitorReceiverColumns(
			QString::fromLatin1(receiverTable->saveLayoutState().toBase64()));
	});

	QString configPath;
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
	const QString home = qEnvironmentVariableIsSet("HOME") ? QString::fromLocal8Bit(qgetenv("HOME"))
							       : QDir::homePath();
	configPath = home + "/.ndi/ndi-config.v1.json";
#else
	configPath = "C:\\ProgramData\\NDI\\ndi-config.v1.json";
#endif
	NdiNetworkConfigWidget *configWidget = new NdiNetworkConfigWidget(configPath, tabWidget);
	tabWidget->addTab(configWidget, dialog->tr("Config"));

	// Restore the last active tab, then persist it whenever the user switches.
	const int savedTab = Config::Current()->NetworkMonitorActiveTab();
	if (savedTab >= 0 && savedTab < tabWidget->count())
		tabWidget->setCurrentIndex(savedTab);
	QObject::connect(tabWidget, &QTabWidget::currentChanged, dialog,
			 [](int index) { Config::Current()->NetworkMonitorActiveTab(index); });

	layout->addWidget(tabWidget);

	// Footer with OK buttons. OK closes the top-level window (dialog).
	QHBoxLayout *footer = new QHBoxLayout();
	footer->setContentsMargins(0, 6, 0, 0);
	footer->setSpacing(8);
	footer->addStretch();

	QPushButton *helpBtn = new QPushButton(dialog->tr("Help"), dialog);
	helpBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	footer->addWidget(helpBtn);
	QObject::connect(helpBtn, &QPushButton::clicked, dialog, []() {
		QDesktopServices::openUrl(
			QUrl("https://github.com/DistroAV/DistroAV/wiki/Network-Monitor-Documentation"));
	});
	QPushButton *dumpBtn = new QPushButton(dialog->tr("Dump to OBS Log"), dialog);
	dumpBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	footer->addWidget(dumpBtn);
	QObject::connect(dumpBtn, &QPushButton::clicked, dialog, [dialog, configWidget]() {
		auto result = QMessageBox::question(
			dialog, dialog->tr("Dump to OBS Log"),
			dialog->tr("Do you want to dump the contents of the Network Monitor to the current OBS "
				   "log? The log can be accessed through OBS -> Help -> Log Files."),
			QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
		if (result != QMessageBox::Yes)
			return;
		network_monitor->dumpNetworkReportToLog();
		configWidget->dumpConfigToLog();
	});
	QPushButton *copyBtn = new QPushButton(dialog->tr("Copy"), dialog);
	copyBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	footer->addWidget(copyBtn);
	QObject::connect(copyBtn, &QPushButton::clicked, dialog,
			 [tabWidget, adapterTable, senderTable, receiverTable, configWidget]() {
				 QWidget *current = tabWidget->currentWidget();
				 QString text;
				 if (current == adapterTable) {
					 text = QString::fromUtf8(network_monitor->getFormattedAdapterReport());
				 } else if (current == senderTable) {
					 text = QString::fromUtf8(network_monitor->getFormattedSenderReport());
				 } else if (current == receiverTable) {
					 text = QString::fromUtf8(network_monitor->getFormattedReceiverReport());
				 } else if (current == configWidget) {
					 text = configWidget->getFormattedConfigText();
				 }
				 QGuiApplication::clipboard()->setText(text);
			 });

	QPushButton *okBtn = new QPushButton(dialog->tr("Close"), dialog);
	okBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
	footer->addWidget(okBtn);
	// OK button closes the parent dialog/window
	QObject::connect(okBtn, &QPushButton::clicked, dialog, [dialog]() {
		QWidget *w = dialog->window();
		if (w)
			w->close();
	});
	footer->setAlignment(Qt::AlignRight);
	layout->addLayout(footer);
	layout->setAlignment(footer, Qt::AlignRight);

	// Restore the last saved size/position, if any. -1 (the default) means
	// "never saved" - leave Qt's own computed size/placement alone in that case.
	const int savedWidth = Config::Current()->NetworkMonitorWidth();
	const int savedHeight = Config::Current()->NetworkMonitorHeight();
	if (savedWidth > 0 && savedHeight > 0)
		dialog->resize(savedWidth, savedHeight);

	const int savedX = Config::Current()->NetworkMonitorLocX();
	const int savedY = Config::Current()->NetworkMonitorLocY();
	if (savedX != -1 && savedY != -1) {
		const QPoint savedPos(savedX, savedY);
		bool visibleOnAScreen = false;
		for (QScreen *screen : QGuiApplication::screens()) {
			if (screen->geometry().contains(savedPos)) {
				visibleOnAScreen = true;
				break;
			}
		}
		if (visibleOnAScreen) {
			dialog->move(savedPos);
		} else if (QScreen *primary = QGuiApplication::primaryScreen()) {
			// Saved location is off any current screen (e.g. a monitor was
			// unplugged) - fall back to centering on the primary screen.
			const QRect avail = primary->availableGeometry();
			dialog->move(avail.center() - QPoint(dialog->width() / 2, dialog->height() / 2));
		}
	}

	// Persist geometry changes (move/resize) as they happen.
	dialog->installEventFilter(new NetworkMonitorGeometryWatcher(dialog));

	// show() returns immediately properties dialog and OBS remain interactive
	dialog->show();
	dialog->raise();
	dialog->activateWindow();
	return true;
}

void close_network_monitor_dialog()
{
	if (s_dialog) {
		s_closingForAppExit = true;
		s_dialog->close();
	}
}
