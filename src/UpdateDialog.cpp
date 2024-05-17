/*
obs-ndi
Copyright (C) 2016-2023 Stéphane Lepin <stephane.lepin@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/
#include "UpdateDialog.h"
#include "Config.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>

#include <plugin-support.h>
#include <obs-frontend-api.h>
#include <plugin-main.h>

//
// Some ideas came from
// https://github.com/occ-ai/obs-backgroundremoval/blob/7f045ccd014e1d8ed5a14d88ba2560007af5f87f/src/update-checker/UpdateDialog.hpp
// https://github.com/occ-ai/obs-backgroundremoval/blob/7f045ccd014e1d8ed5a14d88ba2560007af5f87f/src/update-checker/UpdateDialog.cpp
// https://github.com/obsproject/obs-studio/blob/master/UI/update/update-window.hpp
// https://github.com/obsproject/obs-studio/blob/master/UI/update/update-window.cpp
// https://github.com/obsproject/obs-studio/blob/master/UI/forms/OBSUpdate.ui
//

#define EMULATOR 1

#define UPDATE_TIMEOUT 20000

UpdateDialog::UpdateDialog(const QJsonDocument &jsonResponse, QWidget *parent)
	: QDialog(parent),
	  layout(new QVBoxLayout)
{
	auto releaseTag = jsonResponse["releaseTag"].toString();
	//auto releaseName = jsonResponse["releaseName"].toString();
	auto releaseUrl = jsonResponse["releaseUrl"].toString();
	auto releaseDate = jsonResponse["releaseDate"].toString();
	auto releaseNotes = jsonResponse["releaseNotes"].toString();
	if (releaseNotes.isEmpty()) {
		releaseNotes = "No release notes available.";
	}

	auto releaseVersion = QVersionNumber::fromString(releaseTag);

	auto pluginDisplayName = QString(PLUGIN_NAME).toUpper();

	auto config = Config::Current();

	auto textTemp = QTStr("NDIPlugin.Update.Title").arg(pluginDisplayName);
	setWindowTitle(textTemp);
	setMinimumSize(640, 480);
	resize(640, 480);

	setLayout(layout);

	textTemp = QString("<h2>%1</h2>")
			   .arg(QTStr("NDIPlugin.Update.NewVersionAvailable")
					.arg(pluginDisplayName)
					.arg(releaseVersion.toString()));
	auto labelTitle = new QLabel(textTemp);
	labelTitle->setTextFormat(Qt::RichText);
	layout->addWidget(labelTitle);

	QVersionNumber yourVersion = QVersionNumber::fromString(PLUGIN_VERSION);
	textTemp = QString("<font size='+1'>%1</font>")
			   .arg(QTStr("NDIPlugin.Update.YourVersion")
					.arg(yourVersion.toString()));
	auto labelMessage = new QLabel(textTemp);
	labelMessage->setTextFormat(Qt::RichText);
	layout->addWidget(labelMessage);

	auto labelReleaseNotes =
		new QLabel(QString("<h3>%1</h3>")
				   .arg(Str("NDIPlugin.Update.ReleaseNotes")));
	labelReleaseNotes->setTextFormat(Qt::RichText);
	layout->addWidget(labelReleaseNotes);

	auto scrollArea = new QScrollArea();
	auto labelScrollArea = new QLabel(releaseNotes);
	labelScrollArea->setOpenExternalLinks(true);
	labelScrollArea->setTextInteractionFlags(Qt::TextBrowserInteraction);
	labelScrollArea->setTextFormat(Qt::MarkdownText);
	labelScrollArea->setWordWrap(false);
	labelScrollArea->setStyleSheet("padding: 8px;");
	scrollArea->setStyleSheet("background: #212121;");
	scrollArea->setWidget(labelScrollArea);
	scrollArea->setWidgetResizable(true);

	layout->addWidget(scrollArea);

	auto checkBox = new QCheckBox(
		Str("NDIPlugin.Update.ContinueToCheckForUpdates"));
	checkBox->setChecked(config->AutoCheckForUpdates());
	connect(checkBox, &QCheckBox::stateChanged, this, [](int state) {
		Config::Current()->AutoCheckForUpdates(state == Qt::Checked);
	});
	layout->addWidget(checkBox);

	auto skipButton =
		new QPushButton(Str("NDIPlugin.Update.SkipThisVersion"));
	connect(skipButton, &QPushButton::clicked, this,
		[this, releaseVersion]() {
			Config::Current()->SkipUpdateVersion(releaseVersion);
			this->reject();
		});

	auto spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding,
				      QSizePolicy::Minimum);

	auto remindButton =
		new QPushButton(Str("NDIPlugin.Update.RemindMeLater"));
	connect(remindButton, &QPushButton::clicked, this, [this]() {
		// do nothing; on next launch the plugin
		// will continue to check for updates as normal
		this->reject();
	});

	auto updateButton =
		new QPushButton(Str("NDIPlugin.Update.InstallUpdate"));
	updateButton->setAutoDefault(true);
	updateButton->setDefault(true);
	updateButton->setFocus();
	updateButton->setStyleSheet(
		"QPushButton:default { background-color: #2f65d4; }");
	connect(updateButton, &QPushButton::clicked, [this, releaseUrl]() {
		QDesktopServices::openUrl(QUrl(releaseUrl));
		this->accept();
	});

	auto buttonLayout = new QHBoxLayout();
	buttonLayout->addWidget(skipButton);
	buttonLayout->addItem(spacer);
	buttonLayout->addWidget(remindButton);
	buttonLayout->addWidget(updateButton);
	layout->addLayout(buttonLayout);
}

UpdateDialog *update_dialog = nullptr;

template<typename QEnum> const char *qEnumToString(const QEnum value)
{
	return QMetaEnum::fromType<QEnum>().valueToKey(value);
}

void onCheckForUpdateNetworkFinish(QNetworkReply *reply, bool userRequested)
{
	if (reply->error() == QNetworkReply::NoError) {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: Success");

		auto response = reply->readAll();
		auto jsonResponse = QJsonDocument::fromJson(response);

		auto verbose_log = Config::VerboseLog();
		if (verbose_log) {
			blog(LOG_INFO,
			     "[obs-ndi] onCheckForUpdateNetworkFinish: jsonResponse=%s",
			     jsonResponse.toJson().toStdString().c_str());
		}

		auto latestVersion = QVersionNumber::fromString(
			jsonResponse["releaseTag"].toString());
		if (verbose_log) {
			blog(LOG_INFO,
			     "[obs-ndi] onCheckForUpdateNetworkFinish: latestVersion=%s",
			     latestVersion.toString().toStdString().c_str());
		}

		auto currentVersion =
			QVersionNumber::fromString(PLUGIN_VERSION);
		if (verbose_log) {
			blog(LOG_INFO,
			     "[obs-ndi] onCheckForUpdateNetworkFinish: currentVersion=%s",
			     currentVersion.toString().toStdString().c_str());
		}

		auto skipUpdateVersion = config->SkipUpdateVersion();
		if (!userRequested && latestVersion == skipUpdateVersion) {
			blog(LOG_INFO,
			     "[obs-ndi] onCheckForUpdateNetworkFinish: latestVersion == skipUpdateVersion; ignoring update");
			return;
		}

		if (latestVersion <= currentVersion) {
			blog(LOG_INFO,
			     "[obs-ndi] onCheckForUpdateNetworkFinish: latestVersion <= currentVersion; ignoring update");
			if (userRequested) {
				auto main_window = static_cast<QWidget *>(
					obs_frontend_get_main_window());
				QMessageBox::information(
					main_window,
					Str("NDIPlugin.Update.NoUpdateAvailable"),
					QTStr("NDIPlugin.Update.YouAreUpToDate")
						.arg(currentVersion.toString()));
			}
			return;
		}

		auto uiDelayMillis = jsonResponse["uiDelayMillis"].toInt();
		if (verbose_log) {
			blog(LOG_INFO,
			     "[obs-ndi] onCheckForUpdateNetworkFinish: uiDelayMillis=%d",
			     uiDelayMillis);
		}
		QTimer::singleShot(uiDelayMillis, [jsonResponse]() {
			auto main_window = static_cast<QWidget *>(
				obs_frontend_get_main_window());
			if (main_window == nullptr) {
				blog(LOG_ERROR,
				     "onCheckForUpdateNetworkFinish: Failed to get main OBS window");
				return;
			}
			update_dialog =
				new UpdateDialog(jsonResponse, main_window);
			update_dialog->setAttribute(Qt::WA_DeleteOnClose, true);
			update_dialog->show();
			update_dialog->raise();
		});
	} else {
		blog(LOG_WARNING,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: Error %s: %s",
		     qEnumToString(reply->error()),
		     reply->errorString().toStdString().c_str());
	}
}

QNetworkReply *update_reply = nullptr;

void updateCheckStop()
{
	blog(LOG_INFO, "[obs-ndi] +updateCheckStop()");
	if (update_reply) {
		if (update_reply->isRunning()) {
			update_reply->abort();
		}
		update_reply->deleteLater();
		update_reply = nullptr;
	}
	blog(LOG_INFO, "[obs-ndi] -updateCheckStop()");
}

void updateCheckStart(bool userRequested)
{
	blog(LOG_INFO, "[obs-ndi] +updateCheckStart(userRequested=%d)",
	     userRequested);

	updateCheckStop();

	auto config = Config::Current();
	if (!userRequested && !config->AutoCheckForUpdates()) {
		blog(LOG_INFO,
		     "[obs-ndi] updateCheckStart: AutoCheckForUpdates is disabled");

		blog(LOG_INFO, "[obs-ndi] -updateCheckStart(userRequested=%d)",
		     userRequested);
		return;
	}

	QString obsndiVersion = PLUGIN_VERSION;
	QString obsGuid = config->GetProgramGUID();

#if EMULATOR
	QUrl url("http://127.0.0.1:5002/update");
#else
	QUrl url("https://obsndiproject.com/update");
#endif

	QNetworkRequest request;
	if (url.host() == "127.0.0.1") {
		QUrlQuery query;
		query.addQueryItem("obsndiVersion", obsndiVersion);
		query.addQueryItem("obsGuid", obsGuid);
		url.setQuery(query);
		blog(LOG_INFO, "[obs-ndi] updateCheckStart: url=%s",
		     url.toString().toStdString().c_str());
	} else {
		request.setRawHeader("User-Agent",
				     QString("obs-ndi/%1 (OBS-GUID: %2)")
					     .arg(obsndiVersion)
					     .arg(obsGuid)
					     .toUtf8());
	}
	request.setUrl(url);

	auto timer = new QTimer();
	timer->setSingleShot(true);
	QObject::connect(timer, &QTimer::timeout, []() {
		blog(LOG_WARNING,
		     "[obs-ndi] updateCheckStart: Request timed out");
		updateCheckStop();
	});

	auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto manager = new QNetworkAccessManager(main_window);
	QObject::connect(manager, &QNetworkAccessManager::finished,
			 [manager, timer, userRequested](QNetworkReply *reply) {
				 timer->stop();
				 updateCheckStop();
				 onCheckForUpdateNetworkFinish(reply,
							       userRequested);
				 timer->deleteLater();
				 manager->deleteLater();
			 });
	update_reply = manager->get(request);
	timer->start(UPDATE_TIMEOUT);

	blog(LOG_INFO, "[obs-ndi] -updateCheckStart(userRequested=%d)",
	     userRequested);
}
