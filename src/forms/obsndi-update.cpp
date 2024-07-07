/*
obs-ndi
Copyright (C) 2016-2024 OBS-NDI Project <obsndi@obsndiproject.com>

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
#include "obsndi-update.h"

#include "plugin-main.h"
#include "obs-support/shared-update.h"

#include <obs-frontend-api.h>

#include <QDesktopServices>
#include <QFile>
#include <QJsonDocument>
#include <QMainWindow>
#include <QMessageBox>
#include <QMetaEnum>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTimer>
#include <QUrlQuery>

//
// Some ideas came from
// https://github.com/occ-ai/obs-backgroundremoval/blob/7f045ccd014e1d8ed5a14d88ba2560007af5f87f/src/update-checker/UpdateDialog.hpp
// https://github.com/occ-ai/obs-backgroundremoval/blob/7f045ccd014e1d8ed5a14d88ba2560007af5f87f/src/update-checker/UpdateDialog.cpp
// https://github.com/obsproject/obs-studio/blob/master/UI/update/update-window.hpp
// https://github.com/obsproject/obs-studio/blob/master/UI/update/update-window.cpp
// https://github.com/obsproject/obs-studio/blob/master/UI/forms/OBSUpdate.ui
//

#define UPDATE_TIMEOUT 20000

template<typename QEnum> const char *qEnumToString(const QEnum value)
{
	return QMetaEnum::fromType<QEnum>().valueToKey(value);
}

ObsNdiUpdate::ObsNdiUpdate(const QJsonDocument &jsonResponse, QWidget *parent)
	: QDialog(parent),
	  ui(new Ui::ObsNdiUpdate)
{
	ui->setupUi(this);

	auto releaseTag = jsonResponse["releaseTag"].toString();
	//auto releaseName = jsonResponse["releaseName"].toString();
	auto releaseUrl = jsonResponse["releaseUrl"].toString();
	auto releaseDate = jsonResponse["releaseDate"].toString();
	auto releaseNotes = jsonResponse["releaseNotes"].toString();
	if (releaseNotes.isEmpty()) {
		releaseNotes = "No release notes available.";
	}

	auto releaseVersion = QVersionNumber::fromString(releaseTag);

	auto pluginDisplayName = QString(PLUGIN_DISPLAY_NAME);

	auto config = Config::Current();

	auto textTemp = QTStr("NDIPlugin.Update.Title").arg(pluginDisplayName);
	setWindowTitle(textTemp);

	textTemp = QString("<h2>%1</h2>")
			   .arg(QTStr("NDIPlugin.Update.NewVersionAvailable")
					.arg(pluginDisplayName)
					.arg(releaseVersion.toString()));
	ui->labelVersionNew->setText(textTemp);

	QVersionNumber yourVersion = QVersionNumber::fromString(PLUGIN_VERSION);
	textTemp = QString("<font size='+1'>%1</font>")
			   .arg(QTStr("NDIPlugin.Update.YourVersion")
					.arg(yourVersion.toString()));
	ui->labelVersionYours->setText(textTemp);

	textTemp =
		QString("<h3>%1</h3>").arg(Str("NDIPlugin.Update.ReleaseNotes"));
	ui->labelReleaseNotes->setText(textTemp);

	ui->textReleaseNotes->setMarkdown(releaseNotes);

	ui->checkBoxAutoCheckForUpdates->setText(
		Str("NDIPlugin.Update.ContinueToCheckForUpdates"));
	ui->checkBoxAutoCheckForUpdates->setChecked(
		config->AutoCheckForUpdates());
	connect(ui->checkBoxAutoCheckForUpdates, &QCheckBox::stateChanged, this,
		[](int state) {
			Config::Current()->AutoCheckForUpdates(state ==
							       Qt::Checked);
		});

	ui->buttonSkipThisVersion->setText(
		Str("NDIPlugin.Update.SkipThisVersion"));
	connect(ui->buttonSkipThisVersion, &QPushButton::clicked, this,
		[this, releaseVersion]() {
			Config::Current()->SkipUpdateVersion(releaseVersion);
			this->reject();
		});

	ui->buttonRemindMeLater->setText(Str("NDIPlugin.Update.RemindMeLater"));
	connect(ui->buttonRemindMeLater, &QPushButton::clicked, this, [this]() {
		// do nothing; on next launch the plugin
		// will continue to check for updates as normal
		this->reject();
	});

	ui->buttonInstallUpdate->setText(Str("NDIPlugin.Update.InstallUpdate"));
#ifdef __APPLE__
	// TODO: auto defaultButtonBackgroundColor = MacOSColorHelper::getDefaultButtonColor();
	auto defaultButtonBackgroundColor = QColor::fromString("#2f65d4");
	ui->buttonInstallUpdate->setStyleSheet(
		QString("QPushButton:default { background-color: %1; }")
			.arg(defaultButtonBackgroundColor.name(QColor::HexRgb)));
#endif
	connect(ui->buttonInstallUpdate, &QPushButton::clicked,
		[this, releaseUrl]() {
			QDesktopServices::openUrl(QUrl(releaseUrl));
			this->accept();
		});

	ui->labelDonate->setText(Str("NDIPlugin.Donate"));
	ui->labelDonateUrl->setText(
		QString("<a href=\"%1\">%1</a>").arg(PLUGIN_DONATE_URL));
	connect(ui->labelDonateUrl, &QLabel::linkActivated,
		[this](const QString &) {
			QDesktopServices::openUrl(QUrl(PLUGIN_DONATE_URL));
		});
}

ObsNdiUpdate::~ObsNdiUpdate()
{
	delete ui;
}

QNetworkRequest *update_request = nullptr;
QPointer<QNetworkReply> update_reply = nullptr;
QPointer<ObsNdiUpdate> update_dialog = nullptr;

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

		auto config = Config::Current();
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
				new ObsNdiUpdate(jsonResponse, main_window);
			update_dialog->setAttribute(Qt::WA_DeleteOnClose, true);
			update_dialog->exec();
		});
	} else {
		blog(LOG_WARNING,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: Error %s: %s",
		     qEnumToString(reply->error()),
		     reply->errorString().toStdString().c_str());
	}
}

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
	if (update_request) {
		delete update_request;
		update_request = nullptr;
	}
	blog(LOG_INFO, "[obs-ndi] -updateCheckStop()");
}

QString GetSHA256(const QString &filePath)
{
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly)) {
		blog(LOG_WARNING,
		     "[obs-ndi] GetSHA256: Failed to open file: %s",
		     filePath.toUtf8().constData());
		return "";
	}

	QCryptographicHash hash(QCryptographicHash::Sha256);
	if (!hash.addData(&file)) {
		blog(LOG_WARNING,
		     "[obs-ndi] GetSHA256: Failed to read data from file: %s",
		     filePath.toUtf8().constData());
		return "";
	}

	return hash.result().toHex();
}

QString GetObsCurrentModuleSHA256()
{
	auto module = obs_current_module();
	auto module_binary_path = obs_get_module_binary_path(module);
	blog(LOG_INFO,
	     "[obs-ndi] GetObsCurrentModuleSHA256: module_binary_path=%s",
	     module_binary_path);
	auto module_hash_sha256 = GetSHA256(module_binary_path);
	blog(LOG_INFO,
	     "[obs-ndi] GetObsCurrentModuleSHA256: module_hash_sha256=%s",
	     module_hash_sha256.toUtf8().constData());
	return module_hash_sha256;
}

void updateCheckStart(bool userRequested)
{
	blog(LOG_INFO, "[obs-ndi] +updateCheckStart(userRequested=%d)",
	     userRequested);

	auto config = Config::Current();
	if (!userRequested && !config->AutoCheckForUpdates()) {
		blog(LOG_INFO,
		     "[obs-ndi] updateCheckStart: AutoCheckForUpdates is disabled");

		blog(LOG_INFO, "[obs-ndi] -updateCheckStart(userRequested=%d)",
		     userRequested);
		return;
	}

	if (update_request || update_reply || update_dialog) {
		if (update_dialog) {
			update_dialog->raise();
		}
		blog(LOG_INFO,
		     "[obs-ndi] updateCheckStart: update pending or showing; ignoring");
		return;
	}

	update_request = new QNetworkRequest();

#if 0
	/*
	2024/07/06 paulpv confirmed w/ OBS RytoEX that OBS "accidentally" removed TLS support from Qt:
	paulpv Q: Why does obs-deps/deps.qt set -DINPUT_openssl:STRING=no?
    	Is there some problem letting Qt use OpenSSL?
    	When I try to call QNetworkAccessManager.get("https://www.github.com") in my plugin,
		I get a bunch of `No functional TLS backend was found`, `No TLS backend is available`, and
		`TLS initialization failed` errors; my code works fine on http urls.
	RytoEX A: We accidentally disabled copying the Qt TLS backends. It'll be fixed in a future release. 
		It's on my backlog of "local branches that fix this that haven't been PR'd".
		We decided intentionally to opt for the native TLS backends for Qt on Windows and macOS,
		but see other messages where copying it got accidentally disabled.
	
	**Until that is fixed** I can either:
	1. Compile my own Qt6 dep for OBS; no thanks!
	2. Don't make https requests; this is what I will be doing temporarily to distroav.org.
	3. Make https requests a non-QT way (ex: "libcurl"). Looking at how
		https://github.com/occ-ai/obs-backgroundremoval/tree/main/src/update-checker
		works... this isn't complicated but it still makes makes me want to not do it.
	4. Just use Firebase Functions; the problem with this is "how to cancel an pending function call"?
	*/
	qputenv("QT_LOGGING_RULES", "qt.network.ssl=true");
	blog(LOG_INFO, "[obs-ndi] updateCheckStart QSslSocket: `%s` `%s` `%s`",
	     QSslSocket::supportsSsl() ? "supportsSsl" : "noSsl",
	     QSslSocket::sslLibraryBuildVersionString().toStdString().c_str(),
	     QSslSocket::sslLibraryVersionString().toStdString().c_str());
#endif

#if 0
	QUrl url(
		"https://api.github.com/repos/DistroAV/DistroAV/releases/latest");
#else
	auto obsndiVersion = QString(PLUGIN_VERSION);
	auto obsGuid = QString(GetProgramGUID().c_str());
	auto module_hash_sha256 = GetObsCurrentModuleSHA256();

	QString updateHost(PLUGIN_UPDATE_HOST);
	if (updateHost == "127.0.0.1") {
		updateHost += ":5002";
	}

	QUrl url(QString("http://%1/update").arg(updateHost));
	if (url.host() == "127.0.0.1") {
		// Local EMULATOR testing; a little easier to debug
		QUrlQuery query;
		query.addQueryItem("obsndiVersion", obsndiVersion);
		query.addQueryItem("obsGuid", obsGuid);
		query.addQueryItem("sha256", module_hash_sha256);
		url.setQuery(query);
		blog(LOG_INFO, "[obs-ndi] updateCheckStart: url=%s",
		     url.toString().toStdString().c_str());
	} else {
		// Production
		update_request->setRawHeader(
			"User-Agent", QString("obs-ndi/%1 (OBS-GUID: %2); %3")
					      .arg(obsndiVersion)
					      .arg(obsGuid)
					      .arg(module_hash_sha256)
					      .toUtf8());
	}
#endif
	update_request->setUrl(url);

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
				 onCheckForUpdateNetworkFinish(reply,
							       userRequested);
				 updateCheckStop();
				 timer->deleteLater();
				 manager->deleteLater();
			 });
	update_reply = manager->get(*update_request);
	timer->start(UPDATE_TIMEOUT);

	blog(LOG_INFO, "[obs-ndi] -updateCheckStart(userRequested=%d)",
	     userRequested);
}
