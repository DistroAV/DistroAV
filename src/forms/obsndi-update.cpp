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
#include <QDialog>
#include <QMainWindow>
// #include <QMessageBox>
#include <QMetaEnum>
// #include <QNetworkAccessManager>
// #include <QNetworkReply>
// #include <QNetworkRequest>
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

#define UPDATE_TIMEOUT_SEC 10

template<typename QEnum> const char *qEnumToString(const QEnum value)
{
	return QMetaEnum::fromType<QEnum>().valueToKey(value);
}

PluginUpdateInfo::PluginUpdateInfo(const QString &responseData_,
				   const QString &errorData_)
{
	errorData = errorData_;
	if (!errorData.isEmpty()) {
		return;
	}

	responseData = responseData_;

	QJsonParseError jsonParseError;
	jsonDocument =
		QJsonDocument::fromJson(responseData.toUtf8(), &jsonParseError);
	if (jsonDocument.isNull()) {
		this->errorData = jsonParseError.errorString();
		return;
	}
	if (!jsonDocument.isObject()) {
		this->errorData = "jsonDocument is not an object";
		return;
	}

	jsonObject = jsonDocument.object();

	auto infoVersion_ = jsonObject["v"];
	if (infoVersion_.isUndefined()) {
		this->errorData = "v == undefined";
		return;
	}
	infoVersion = infoVersion_.toInt();

	auto releaseTag_ = jsonObject["releaseTag"];
	if (releaseTag_.isUndefined()) {
		this->errorData = "releaseTag == undefined";
		return;
	}
	releaseTag = releaseTag_.toString();

	releaseName = jsonObject["releaseName"].toString();

	releaseUrl = jsonObject["releaseUrl"].toString();

	releaseDate = jsonObject["releaseDate"].toString();

	releaseNotes = jsonObject["releaseNotes"].toString();
	if (releaseNotes.isEmpty()) {
		releaseNotes = Str("NDIPlugin.Update.ReleaseNotes.None");
	}

	uiDelayMillis = jsonObject["uiDelayMillis"].toInt(1000);

	versionCurrent = QVersionNumber::fromString(PLUGIN_VERSION);

#if 0
	// For testing purposes only
	this->fakeVersionLatest = true;
	versionLatest = QVersionNumber(versionCurrent.majorVersion(),
				       versionCurrent.minorVersion() + 1,
				       versionCurrent.microVersion());
#else
	versionLatest = QVersionNumber::fromString(releaseTag);
#endif
	if (versionLatest.isNull()) {
		this->errorData = "versionLatest == null";
		return;
	}
}

class ObsNdiUpdate : public QDialog {
	Q_OBJECT
private:
	std::unique_ptr<Ui::ObsNdiUpdate> ui;

public:
	explicit ObsNdiUpdate(const PluginUpdateInfo &pluginUpdateInfo,
			      QWidget *parent = nullptr)
		: QDialog(parent),
		  ui(new Ui::ObsNdiUpdate)
	{
		ui->setupUi(this);

		auto pluginDisplayName = QString(PLUGIN_DISPLAY_NAME);

		auto config = Config::Current();

		auto textTemp =
			QTStr("NDIPlugin.Update.Title").arg(pluginDisplayName);
		setWindowTitle(textTemp);

		textTemp =
			QString("<h2>%1</h2>")
				.arg(QTStr("NDIPlugin.Update.NewVersionAvailable")
					     .arg(pluginDisplayName)
					     .arg(pluginUpdateInfo.versionLatest
							  .toString()));
		ui->labelVersionNew->setText(textTemp);

		QVersionNumber yourVersion =
			QVersionNumber::fromString(PLUGIN_VERSION);
		textTemp = QString("<font size='+1'>%1</font>")
				   .arg(QTStr("NDIPlugin.Update.YourVersion")
						.arg(yourVersion.toString()));
		ui->labelVersionYours->setText(textTemp);

		textTemp = QString("<h3>%1</h3>")
				   .arg(Str("NDIPlugin.Update.ReleaseNotes"));
		ui->labelReleaseNotes->setText(textTemp);

		auto utcDateTime = QDateTime::fromString(
			pluginUpdateInfo.releaseDate, Qt::ISODate);
		utcDateTime.setTimeSpec(Qt::UTC);
		auto formattedUtcDateTime =
			utcDateTime.toString("yyyy-MM-dd hh:mm:ss 'UTC'");
		textTemp = QString("<h3>%1</h3>")
				   .arg(Str("NDIPlugin.Update.ReleaseDate"));
		ui->labelReleaseDate->setText(textTemp);
		ui->textReleaseDate->setText(formattedUtcDateTime);

		ui->textReleaseNotes->setMarkdown(
			pluginUpdateInfo.releaseNotes);

		ui->checkBoxAutoCheckForUpdates->setText(
			Str("NDIPlugin.Update.ContinueToCheckForUpdate"));
		ui->checkBoxAutoCheckForUpdates->setChecked(
			config->AutoCheckForUpdates());
		connect(ui->checkBoxAutoCheckForUpdates,
			&QCheckBox::stateChanged, this, [](int state) {
				Config::Current()->AutoCheckForUpdates(
					state == Qt::Checked);
			});

		ui->buttonSkipThisVersion->setText(
			Str("NDIPlugin.Update.SkipThisVersion"));
		connect(ui->buttonSkipThisVersion, &QPushButton::clicked, this,
			[this, pluginUpdateInfo]() {
				Config::Current()->SkipUpdateVersion(
					pluginUpdateInfo.versionLatest);
				this->reject();
			});

		ui->buttonRemindMeLater->setText(
			Str("NDIPlugin.Update.RemindMeLater"));
		connect(ui->buttonRemindMeLater, &QPushButton::clicked, this,
			[this]() {
				// do nothing; on next launch the plugin
				// will continue to check for updates as normal
				this->reject();
			});

		ui->buttonInstallUpdate->setText(
			Str("NDIPlugin.Update.InstallUpdate"));
#ifdef __APPLE__
		// TODO: auto defaultButtonBackgroundColor = MacOSColorHelper::getDefaultButtonColor();
		auto defaultButtonBackgroundColor =
			QColor::fromString("#2f65d4");
		ui->buttonInstallUpdate->setStyleSheet(
			QString("QPushButton:default { background-color: %1; }")
				.arg(defaultButtonBackgroundColor.name(
					QColor::HexRgb)));
#endif
		connect(ui->buttonInstallUpdate, &QPushButton::clicked,
			[this, pluginUpdateInfo]() {
				QDesktopServices::openUrl(
					QUrl(pluginUpdateInfo.releaseUrl));
				this->accept();
			});

		ui->labelDonate->setText(Str("NDIPlugin.Donate"));
		ui->labelDonateUrl->setText(
			makeLink(PLUGIN_REDIRECT_DONATE_URL));
		connect(ui->labelDonateUrl, &QLabel::linkActivated,
			[this](const QString &url) {
				QDesktopServices::openUrl(QUrl(url));
			});
	}
};

#include "obsndi-update.moc"

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
	   I think the place to watch for changes that fix this is:
		* https://github.com/obsproject/obs-deps/blob/master/deps.qt/qt6.ps1
	   	* https://github.com/obsproject/obs-deps/blob/master/deps.qt/qt6.zsh
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
				   const QString &errorData,
				   UserRequestCallback userRequestCallback)
{
	auto log_debug = false;
#if 0
	// For testing purposes only
	log_debug = true;
#endif
	auto log_verbose = log_debug || Config::VerboseLog();
	if (log_debug) {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish(httpCode=%d, responseData=`%s`, errorData=`%s`, userRequestCallback=%s)",
		     httpCode, responseData.toUtf8().constData(),
		     errorData.toUtf8().constData(),
		     userRequestCallback ? "..." : "nullptr");
	} else {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish(httpCode=%d, responseData=..., errorData=`%s`, userRequestCallback=%s)",
		     httpCode, errorData.toUtf8().constData(),
		     userRequestCallback ? "..." : "nullptr");
	}

	auto pluginUpdateInfo = PluginUpdateInfo(responseData, errorData);
	if (!pluginUpdateInfo.errorData.isEmpty()) {
		blog(LOG_WARNING,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: Error! httpCode=%d, errorData=`%s`; ignoring response",
		     httpCode, pluginUpdateInfo.errorData.toUtf8().constData());
		if (userRequestCallback) {
			userRequestCallback(pluginUpdateInfo);
		}
		return;
	}

	if (log_verbose) {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: Success! httpCode=%d",
		     httpCode);
	}

	if (log_debug) {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: jsonDocument=`%s`",
		     pluginUpdateInfo.jsonDocument.toJson().constData());
	}
	if (log_verbose) {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: versionCurrent=%s",
		     pluginUpdateInfo.versionCurrent.toString()
			     .toUtf8()
			     .constData());
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: %sversionLatest=%s",
		     pluginUpdateInfo.fakeVersionLatest ? "FAKE " : "",
		     pluginUpdateInfo.versionLatest.toString()
			     .toUtf8()
			     .constData());
	}

	if (userRequestCallback) {
		// User requested update check ignores SkipUpdateVersion
		userRequestCallback(pluginUpdateInfo);
	} else {
		// Non-user requested update check respects SkipUpdateVersion
		auto versionSkip = Config::Current()->SkipUpdateVersion();
		if (pluginUpdateInfo.versionLatest == versionSkip) {
			blog(LOG_INFO,
			     "[obs-ndi] onCheckForUpdateNetworkFinish: versionLatest == versionSkip(%s); ignoring update",
			     versionSkip.toString().toUtf8().constData());
			return;
		}
	}

	// Both user requested and non-user requested update checks ignore versionLatest <= versionCurrent
	if (pluginUpdateInfo.versionLatest <= pluginUpdateInfo.versionCurrent) {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: versionLatest <= versionCurrent; ignoring update");
		return;
	}

	auto uiDelayMillis =
		userRequestCallback ? 0 : pluginUpdateInfo.uiDelayMillis;
	if (log_debug) {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: uiDelayMillis=%d",
		     uiDelayMillis);
	}

	QTimer::singleShot(uiDelayMillis, [pluginUpdateInfo]() {
		auto main_window = static_cast<QMainWindow *>(
			obs_frontend_get_main_window());
		if (main_window == nullptr) {
			blog(LOG_ERROR,
			     "onCheckForUpdateNetworkFinish: Failed to get main OBS window");
			return;
		}

		update_dialog = new ObsNdiUpdate(pluginUpdateInfo, main_window);
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
			//update_request->terminate();
			update_request->exit(1);
		}
		//update_request->deleteLater();
		//update_request = nullptr;
	}
	blog(LOG_INFO, "[obs-ndi] -updateCheckStop()");
}

bool updateCheckStart(UserRequestCallback userRequestCallback)
{
	//auto log_debug = false;
#if 0
	// For testing purposes only
	log_debug = true;
#endif
	blog(LOG_INFO, "[obs-ndi] +updateCheckStart(userRequestCallback=%s)",
	     userRequestCallback ? "..." : "nullptr");

	auto config = Config::Current();
	if (!userRequestCallback && !config->AutoCheckForUpdates()) {
		blog(LOG_INFO,
		     "[obs-ndi] updateCheckStart: AutoCheckForUpdates is disabled");

		blog(LOG_INFO,
		     "[obs-ndi] -updateCheckStart(userRequestCallback=%p)",
		     userRequestCallback ? "..." : "nullptr");
		return false;
	}

	if (isUpdatePendingOrShowing()) {
		if (update_dialog) {
			update_dialog->raise();
		}
		blog(LOG_INFO,
		     "[obs-ndi] updateCheckStart: update pending or showing; ignoring");
		return false;
	}

	auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());

//#define DIRECT_REQUEST_GITHUB
#ifdef DIRECT_REQUEST_GITHUB
	// Used to test directly hitting github instead of going through distroav.org firebase hosting+functions.
	QUrl url(
		"https://api.github.com/repos/DistroAV/DistroAV/releases/latest");
#else
	QUrl url(rehostUrl(PLUGIN_UPDATE_URL));

	auto pluginVersion = QString(PLUGIN_VERSION);
	auto obsGuid = QString(GetProgramGUID().constData());
	auto module_hash_sha256 = GetObsCurrentModuleSHA256();
	// blog(LOG_INFO, "[obs-ndi] updateCheckStart: module_hash_sha256=`%s`",
	//      module_hash_sha256.toUtf8().constData());
	std::string postData;

	auto useEmulator = url.host() == "127.0.0.1";
	auto useQueryParams = useEmulator;
	if (useQueryParams) {
		// Local EMULATOR testing; a little easier to debug
		QUrlQuery query;
		query.addQueryItem("pluginVersion", pluginVersion);
		query.addQueryItem("obsGuid", obsGuid);
		query.addQueryItem("sha256", module_hash_sha256);
		url.setQuery(query);
	}
	if (true) { // log_debug) {
		blog(LOG_INFO, "[obs-ndi] updateCheckStart: url=`%s`",
		     url.toString().toUtf8().constData());
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
	QObject::connect(
		update_request, &RemoteTextThread::finished, main_window,
		[]() {
			update_request->deleteLater();
			update_request = nullptr;
		},
		Qt::QueuedConnection);
#endif

#ifndef DIRECT_REQUEST_GITHUB
	if (!useQueryParams) {
		auto userAgent = QString("DistroAV/%1 (OBS-GUID: %2); %3")
					 .arg(pluginVersion)
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
		[manager, timer, userRequestor](QNetworkReply *reply) {
			timer->stop();

			auto errorCode = reply->error();
			QString responseOrErrorString;
			if (errorCode == QNetworkReply::NoError) {
				responseOrErrorString = reply->readAll();
			} else {
				responseOrErrorString = reply->errorString();
			}

			onCheckForUpdateNetworkFinish(responseOrErrorString,
						      errorCode, userRequestor);
			updateCheckStop();
			timer->deleteLater();
			manager->deleteLater();
		});
	timer->start(UPDATE_TIMEOUT_SEC * 1000));
	update_reply = manager->get(*update_request);
#else
	QObject::connect(
		update_request, &RemoteTextThread::Result, main_window,
		[userRequestCallback](int httpCode, const QString &responseData,
				      const QString &errorData) {
#if 0
			blog(LOG_INFO,
			     "[obs-ndi] updateCheckStart: Result: httpCode=%d, responseData=`%s`, errorData=`%s`",
			     httpCode, responseData.toUtf8().constData(),
			     errorData.toUtf8().constData());
#endif
			onCheckForUpdateNetworkFinish(httpCode, responseData,
						      errorData,
						      userRequestCallback);
			//updateCheckStop();
		},
		Qt::QueuedConnection);
	update_request->start();
#endif
	blog(LOG_INFO, "[obs-ndi] -updateCheckStart(userRequestCallback=%s)",
	     userRequestCallback ? "..." : "nullptr");
	return true;
}
