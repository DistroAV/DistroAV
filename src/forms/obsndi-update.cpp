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
/**
 * Other ideas came from:
 * * https://github.com/obsproject/obs-studio/tree/master/UI/update
 * * https://sparkle-project.org/
 * * https://github.com/occ-ai/obs-backgroundremoval/blob/7f045ccd014e1d8ed5a14d88ba2560007af5f87f/src/update-checker/
 * * https://github.com/occ-ai/obs-backgroundremoval/blob/7f045ccd014e1d8ed5a14d88ba2560007af5f87f/src/update-checker/UpdateDialog.cpp
 * * https://github.com/occ-ai/obs-backgroundremoval/blob/7f045ccd014e1d8ed5a14d88ba2560007af5f87f/src/update-checker/UpdateDialog.hpp
 * * https://github.com/occ-ai/obs-backgroundremoval/blob/main/src/update-checker/CurlClient/CurlClient.cpp
 * * https://github.com/occ-ai/obs-backgroundremoval/blob/main/src/update-checker/github-utils.cpp#L13
 */
#include "obsndi-update.h"

#include "plugin-main.h"
#include "obs-support/shared-update.hpp"

#include <obs-frontend-api.h>

#include <QDesktopServices>
#include <QJsonDocument>
#include <QMainWindow>
#include <QMessageBox>
#include <QMetaEnum>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QThread>
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

#define UPDATE_TIMEOUT_SEC 10

template<typename QEnum> const char *qEnumToString(const QEnum value)
{
	return QMetaEnum::fromType<QEnum>().valueToKey(value);
}

ObsNdiUpdate::ObsNdiUpdate(const QJsonObject &jsonObject, QWidget *parent)
	: QDialog(parent),
	  ui(new Ui::ObsNdiUpdate)
{
	ui->setupUi(this);

	auto releaseTag = jsonObject["releaseTag"].toString();
	//auto releaseName = jsonObject["releaseName"].toString();
	auto releaseUrl = jsonObject["releaseUrl"].toString();
	auto releaseDate = jsonObject["releaseDate"].toString();
	auto releaseNotes = jsonObject["releaseNotes"].toString();
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

	auto utcDateTime = QDateTime::fromString(releaseDate, Qt::ISODate);
	utcDateTime.setTimeSpec(Qt::UTC);
	auto formattedUtcDateTime =
		utcDateTime.toString("yyyy-MM-dd hh:mm:ss 'UTC'");
	textTemp =
		QString("<h3>%1</h3>").arg(Str("NDIPlugin.Update.ReleaseDate"));
	ui->labelReleaseDate->setText(textTemp);
	ui->textReleaseDate->setText(formattedUtcDateTime);

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
	ui->labelDonateUrl->setText(makeLink(PLUGIN_REDIRECT_DONATE_URL));
	connect(ui->labelDonateUrl, &QLabel::linkActivated,
		[this](const QString &url) {
			QDesktopServices::openUrl(QUrl(url));
		});
}

//
//
//

QString GetObsCurrentModuleSHA256()
{
	// NOTE: `obs_module_file(nullptr)` returns the plugin's "Resources" path and will not work.
	auto module = obs_current_module();
	auto module_binary_path = obs_get_module_binary_path(module);
#if 0
	blog(LOG_INFO,
	     "[obs-ndi] GetObsCurrentModuleSHA256: module_binary_path=`%s`",
	     module_binary_path);
#endif
	QString module_hash_sha256;
	auto success =
		CalculateFileHash(module_binary_path, module_hash_sha256);
#if 0
	blog(LOG_INFO,
	     "[obs-ndi] GetObsCurrentModuleSHA256: module_hash_sha256=`%s`",
	     module_hash_sha256.toUtf8().constData());
#endif
	return success ? module_hash_sha256 : "";
}

bool IsMainThread()
{
	return QThread::currentThread() ==
	       QCoreApplication::instance()->thread();
}

bool PostToMainThread(const char *text, std::function<void()> task)
{
	if (IsMainThread()) {
		task();
		return false;
	} else {
#if 0
		blog(LOG_INFO,
		     "[obs-ndi] obsndi-update: PostToMainThread(`%s`, task)",
		     text);
#else
		UNUSED_PARAMETER(text);
#endif
		QMetaObject::invokeMethod(QCoreApplication::instance(),
					  [task]() { task(); });
		return true;
	}
}

//#define UPDATE_REQUEST_QT
#ifdef UPDATE_REQUEST_QT
/*
	On 2024/07/06 @paulpv asked on OBS Discord #development and @RytoEX confirmed that OBS "accidentally" removed TLS support from Qt:
	>	@paulpv Q: Why does obs-deps/deps.qt set -DINPUT_openssl:STRING=no?
	>		Is there some problem letting Qt use OpenSSL?
	>		When I try to call QNetworkAccessManager.get("https://www.github.com") in my plugin,
	>		I get a bunch of `No functional TLS backend was found`, `No TLS backend is available`, and
	>		`TLS initialization failed` errors; my code works fine on http urls.
	>	@RytoEX A: We accidentally disabled copying the Qt TLS backends. It'll be fixed in a future release. 
	>		It's on my backlog of "local branches that fix this that haven't been PR'd".
	>		We decided intentionally to opt for the native TLS backends for Qt on Windows and macOS,
	>		but see other messages where copying it got accidentally disabled.

	Options:
	1. Wait for OBS to fix this: Unknown how long this will be.
	2. Compile DistroAV's own Qt6 dep w/ TLS enabled: No thanks!
	3. Make only http requests; Don't make https requests: Ignoring the security concerns, this works
	   when testing against the emulator but won't work for non-emulator (actual cloud) due to
	   http://distroav.org auto-redirecting to https://distroav.org.
	   My original thought was that even though https requests are failing for TLS reasons,
	   http requests are still working fine and this code can use those until the TLS issue is fixed.
	   I could swear that http requests to firebase ussed to work fine.
	   But latest testing shows that http requests are auto forwarding to https, so it looks like
	   this https/TLS problem cannot be avoided by calling just http. :/
	   The code will have to resort to using a non-QT way to make https requests.
	4. Make https requests a non-QT way (ex: "libcurl"):
		OBS does this:
			https://github.com/obsproject/obs-studio/tree/master/UI/update
			https://github.com/obsproject/obs-studio/blob/master/UI/update/shared-update.cpp
			https://github.com/obsproject/obs-studio/blob/master/UI/remote-text.cpp
		occ-ai/obs-backgroundremoval does things a little different:
	    	https://github.com/occ-ai/obs-backgroundremoval/tree/main/src/update-checker
		Neither of these are highly complicated, but it is silly this has to be done
		and I would prefer to write little to no knowlingly future throwaway code (see Option #1).
	5. Direct calls to Firebase Functions: the problem with this is "how to cancel an pending function call"?
*/
QNetworkRequest *update_request = nullptr;
QPointer<QNetworkReply> update_reply = nullptr;
#else
#include "obs-support/remote-text.hpp"
QPointer<RemoteTextThread> update_request = nullptr;
#endif
QPointer<ObsNdiUpdate> update_dialog = nullptr;

bool isUpdatePendingOrShowing()
{
	return update_request ||
#ifdef UPDATE_REQUEST_QT
	       update_reply ||
#endif
	       update_dialog;
}

void onCheckForUpdateNetworkFinish(int httpCode, const QString &responseData,
				   const QString &errorData, bool userRequested)
{
	auto verbose_log = Config::VerboseLog();
	if (verbose_log) {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish(httpCode=%d, responseData=`%s`, errorData=`%s`, userRequested=%d)",
		     httpCode, responseData.toUtf8().constData(),
		     errorData.toUtf8().constData(), userRequested);
	} else {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish(httpCode=%d, responseData=..., errorData=`%s`, userRequested=%d)",
		     httpCode, errorData.toUtf8().constData(), userRequested);
	}

	if (!errorData.isEmpty()) {
		blog(LOG_WARNING,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: Error! httpCode=%d, errorData=`%s`; ignoring response",
		     httpCode, errorData.toUtf8().constData());
		return;
	}

	blog(LOG_INFO,
	     "[obs-ndi] onCheckForUpdateNetworkFinish: Success! httpCode=%d",
	     httpCode);

	QJsonParseError jsonParseError;
	auto jsonResponse =
		QJsonDocument::fromJson(responseData.toUtf8(), &jsonParseError);
	if (verbose_log) {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: jsonResponse=`%s`",
		     jsonResponse.toJson().constData());
	}
	if (jsonResponse.isNull()) {
		blog(LOG_WARNING,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: jsonResponse=null; jsonParseError=`%s`; ignoring response",
		     jsonParseError.errorString().toUtf8().constData());
		return;
	}
	if (!jsonResponse.isObject()) {
		blog(LOG_WARNING,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: jsonResponse is not an object; ignoring response");
		return;
	}
	auto jsonObject = jsonResponse.object();

	auto releaseTag = jsonObject["releaseTag"];
	if (releaseTag.isUndefined()) {
		blog(LOG_WARNING,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: releaseTag is undefined; ignoring response");
		return;
	}

	auto latestVersion = QVersionNumber::fromString(releaseTag.toString());
	if (verbose_log) {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: latestVersion=%s",
		     latestVersion.toString().toUtf8().constData());
	}
	if (latestVersion.isNull()) {
		blog(LOG_WARNING,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: latestVersion is null; ignoring response");
		return;
	}

	auto currentVersion = QVersionNumber::fromString(PLUGIN_VERSION);
	if (verbose_log) {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: currentVersion=%s",
		     currentVersion.toString().toUtf8().constData());
	}

//#define TEST_FORCE_UPDATE
#ifdef TEST_FORCE_UPDATE
	UNUSED_PARAMETER(userRequested);
#else
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
#endif
	auto uiDelayMillis = jsonObject["uiDelayMillis"].toInt(1000);
	if (verbose_log) {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: uiDelayMillis=%d",
		     uiDelayMillis);
	}

	QTimer::singleShot(uiDelayMillis, [jsonObject]() {
		auto main_window =
			static_cast<QWidget *>(obs_frontend_get_main_window());
		if (main_window == nullptr) {
			blog(LOG_ERROR,
			     "onCheckForUpdateNetworkFinish: Failed to get main OBS window");
			return;
		}

		update_dialog = new ObsNdiUpdate(jsonObject, main_window);
		// Our logic needs to set update_dialog=nullptr after it closes...
		QObject::connect(update_dialog, &QDialog::finished, [](int) {
			update_dialog->deleteLater();
			update_dialog = nullptr;
		});
		update_dialog->show();
	});
}

void updateCheckStop()
{
	blog(LOG_INFO, "[obs-ndi] +updateCheckStop()");
#ifdef UPDATE_REQUEST_QT
	if (update_reply) {
		if (update_reply->isRunning()) {
			update_reply->abort();
		}
		update_reply->deleteLater();
		update_reply = nullptr;
	}
#endif
	if (update_request) {
		if (update_request->isRunning()) {
			update_request->exit(-1);
		}
		update_request->deleteLater();
		update_request = nullptr;
	}
	blog(LOG_INFO, "[obs-ndi] -updateCheckStop()");
}

void updateCheckStart(bool userRequested)
{
	blog(LOG_INFO, "[obs-ndi] +updateCheckStart(userRequested=%d)",
	     userRequested);

	auto verbose_log = Config::VerboseLog();

	auto config = Config::Current();
	if (!userRequested && !config->AutoCheckForUpdates()) {
		blog(LOG_INFO,
		     "[obs-ndi] updateCheckStart: AutoCheckForUpdates is disabled");

		blog(LOG_INFO, "[obs-ndi] -updateCheckStart(userRequested=%d)",
		     userRequested);
		return;
	}

	if (isUpdatePendingOrShowing()) {
		if (update_dialog) {
			update_dialog->raise();
		}
		blog(LOG_INFO,
		     "[obs-ndi] updateCheckStart: update pending or showing; ignoring");
		return;
	}

//#define DIRECT_REQUEST_GITHUB
#ifdef DIRECT_REQUEST_GITHUB
	// Used to test directly hitting github instead of going through distroav.org firebase hosting+functions.
	QUrl url(
		"https://api.github.com/repos/DistroAV/DistroAV/releases/latest");
#else
	QUrl url(rehostUrl(PLUGIN_UPDATE_URL));

	auto obsndiVersion = QString(PLUGIN_VERSION);
	auto obsGuid = QString(GetProgramGUID().constData());
	auto module_hash_sha256 = GetObsCurrentModuleSHA256();
	// blog(LOG_INFO, "[obs-ndi] updateCheckStart: module_hash_sha256=`%s`",
	//      module_hash_sha256.toUtf8().constData());
	std::string postData;

	bool useEmulator = url.host() == "127.0.0.1";
	bool doGet = useEmulator;
	if (doGet) {
		// Local EMULATOR testing; a little easier to debug
		QUrlQuery query;
		query.addQueryItem("obsndiVersion", obsndiVersion);
		query.addQueryItem("obsGuid", obsGuid);
		query.addQueryItem("sha256", module_hash_sha256);
		url.setQuery(query);
	} else {
		QJsonObject jsonObject;
		jsonObject["obsndiVersion"] = obsndiVersion;
		jsonObject["obsGuid"] = obsGuid;
		jsonObject["sha256"] = module_hash_sha256;
		postData =
			QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
	}
	if (verbose_log) {
		blog(LOG_INFO, "[obs-ndi] updateCheckStart: url=`%s`",
		     url.toString().toUtf8().constData());
		if (!postData.empty()) {
			blog(LOG_INFO,
			     "[obs-ndi] updateCheckStart: postData=`%s`",
			     postData.c_str());
		}
	}
#endif

#ifdef UPDATE_REQUEST_QT
	// TODO: QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply;
	update_request = new QNetworkRequest(url);

	//qputenv("QT_LOGGING_RULES", "qt.network.ssl=true");
	blog(LOG_INFO, "[obs-ndi] updateCheckStart QSslSocket: `%s` `%s` `%s`",
	     QSslSocket::supportsSsl() ? "supportsSsl" : "noSsl",
	     QSslSocket::sslLibraryBuildVersionString().toUtf8().c_str(),
	     QSslSocket::sslLibraryVersionString().toUtf8().c_str());
	/*
	The above should output something like:
	[obs-ndi] updateCheckStart QSslSocket: `supportsSsl` `OpenSSL 1.1.1l  24 Aug 2021` `OpenSSL 1.1.1l  24 Aug 2021`
	Instead it is outputting:
	[obs-ndi] updateCheckStart QSslSocket: `noSsl` `` ``
	This appears to confirm that OBS' Qt is not built with SSL support. :()
	*/
#else
	update_request = new RemoteTextThread(url.toString().toStdString(),
					      "application/json", postData,
					      UPDATE_TIMEOUT_SEC);
#endif

#ifndef DIRECT_REQUEST_GITHUB
	if (url.host().startsWith("distroav")) {
		// Production; requires at tiny bit more effort to spoof
		auto userAgent = QString("obs-ndi/%1 (OBS-GUID: %2); %3")
					 .arg(obsndiVersion)
					 .arg(obsGuid)
					 .arg(module_hash_sha256);
#ifdef UPDATE_REQUEST_QT
		update_request->setRawHeader("User-Agent", userAgent.toUtf8());
#else
		userAgent = "User-Agent: " + userAgent;
		update_request->headers.push_back(userAgent.toStdString());
#endif
	}
#endif

#ifdef UPDATE_REQUEST_QT
	auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());

	auto manager = new QNetworkAccessManager(main_window);

	auto timer = new QTimer();
	timer->setSingleShot(true);
	QObject::connect(timer, &QTimer::timeout, []() {
		blog(LOG_WARNING,
		     "[obs-ndi] updateCheckStart: timer: Request timed out");
		PostToMainThread("timer->timeout", []() { updateCheckStop(); });
	});

	QObject::connect(
		manager, &QNetworkAccessManager::finished,
		[manager, timer, userRequested](QNetworkReply *reply) {
			timer->stop();

			auto errorCode = reply->error();
			QString responseOrErrorString;
			if (errorCode == QNetworkReply::NoError) {
				responseOrErrorString = reply->readAll();
			} else {
				responseOrErrorString = reply->errorString();
			}

			onCheckForUpdateNetworkFinish(responseOrErrorString,
						      errorCode, userRequested);
			updateCheckStop();
			timer->deleteLater();
			manager->deleteLater();
		});
	timer->start(UPDATE_TIMEOUT_SEC * 1000));
	update_reply = manager->get(*update_request);
#else
	QObject::connect(
		update_request, &RemoteTextThread::Result,
		[userRequested](int httpCode, const QString &responseData,
				const QString &errorData) {
			PostToMainThread("update_request->Result",
					 [httpCode, responseData, errorData,
					  userRequested]() {
						 onCheckForUpdateNetworkFinish(
							 httpCode, responseData,
							 errorData,
							 userRequested);
						 updateCheckStop();
					 });
		});
	update_request->start();
#endif
	blog(LOG_INFO, "[obs-ndi] -updateCheckStart(userRequested=%d)",
	     userRequested);
}
