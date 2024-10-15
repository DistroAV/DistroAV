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
/*
Inspiration(s):
* https://github.com/obsproject/obs-studio/tree/master/UI/update
* https://github.com/obsproject/obs-studio/blob/master/UI/update/update-window.hpp
* https://github.com/obsproject/obs-studio/blob/master/UI/update/update-window.cpp
* https://github.com/obsproject/obs-studio/blob/master/UI/forms/OBSUpdate.ui
* https://sparkle-project.org/
* https://github.com/occ-ai/obs-backgroundremoval/blob/7f045ccd014e1d8ed5a14d88ba2560007af5f87f/src/update-checker/
* https://github.com/occ-ai/obs-backgroundremoval/blob/7f045ccd014e1d8ed5a14d88ba2560007af5f87f/src/update-checker/UpdateDialog.cpp
* https://github.com/occ-ai/obs-backgroundremoval/blob/7f045ccd014e1d8ed5a14d88ba2560007af5f87f/src/update-checker/UpdateDialog.hpp
* https://github.com/occ-ai/obs-backgroundremoval/blob/main/src/update-checker/CurlClient/CurlClient.cpp
* https://github.com/occ-ai/obs-backgroundremoval/blob/main/src/update-checker/github-utils.cpp#L13
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
#include <QSslSocket>
#include <QTimer>
#include <QTimeZone>
#include <QUrlQuery>

#define UPDATE_TIMEOUT_SEC 10

template<typename QEnum> const char *qEnumToString(const QEnum value)
{
	return QMetaEnum::fromType<QEnum>().valueToKey(value);
}

PluginUpdateInfo::PluginUpdateInfo(const int httpStatusCode_,
				   const QString &responseData_,
				   const QString &errorData_)
{
	httpStatusCode = httpStatusCode_;

	errorData = errorData_;
	if (!errorData.isEmpty()) {
		return;
	}

	responseData = responseData_;

	QJsonParseError jsonParseError;
	jsonDocument =
		QJsonDocument::fromJson(responseData.toUtf8(), &jsonParseError);
	if (jsonParseError.error != QJsonParseError::NoError) {
		errorData = jsonParseError.errorString();
		return;
	}
	if (jsonDocument.isNull() || !jsonDocument.isObject()) {
		errorData = "jsonDocument is not an object";
		return;
	}

	jsonObject = jsonDocument.object();

	auto infoVersion_ = jsonObject["v"];
	if (infoVersion_.isUndefined()) {
		errorData = "v == undefined";
		return;
	}
	infoVersion = infoVersion_.toInt();

	auto releaseTag_ = jsonObject["releaseTag"];
	if (releaseTag_.isUndefined()) {
		errorData = "releaseTag == undefined";
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

	uiDelayMillis =
		jsonObject["uiDelayMillis"].toInt(DEFAULT_UI_DELAY_MILLIS);

	minAutoUpdateCheckIntervalSeconds =
		jsonObject["minAutoUpdateCheckIntervalSeconds"].toInt(
			DEFAULT_MIN_AUTO_UPDATE_CHECK_INTERVAL_SECONDS);

	versionCurrent = QVersionNumber::fromString(PLUGIN_VERSION);

#if 0
	// For testing purposes only
	fakeVersionLatest = true;
	versionLatest = QVersionNumber(versionCurrent.majorVersion(),
				       versionCurrent.minorVersion() + 1,
				       versionCurrent.microVersion());
#else
	versionLatest = QVersionNumber::fromString(releaseTag);
#endif
	if (versionLatest.isNull()) {
		errorData = "versionLatest == null";
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

		auto pluginDisplayName = QString(PLUGIN_NAME);

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
		utcDateTime.setTimeZone(QTimeZone::UTC);
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
			&QCheckBox::checkStateChanged, this, [](int state) {
				Config::Current()->AutoCheckForUpdates(
					state == Qt::Checked);
			});

		ui->buttonSkipThisVersion->setText(
			Str("NDIPlugin.Update.SkipThisVersion"));
		connect(ui->buttonSkipThisVersion, &QPushButton::clicked, this,
			[this, pluginUpdateInfo]() {
				Config::Current()->SkipUpdateVersion(
					pluginUpdateInfo.versionLatest);
				reject();
			});

		ui->buttonRemindMeLater->setText(
			Str("NDIPlugin.Update.RemindMeLater"));
		connect(ui->buttonRemindMeLater, &QPushButton::clicked, this,
			[this]() {
				// do nothing; on next launch the plugin
				// will continue to check for updates as normal
				reject();
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
				accept();
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
	     QT_TO_UTF8(module_hash_sha256));
#endif
	return success ? module_hash_sha256 : "";
}

//#define UPDATE_REQUEST_QT
#ifdef UPDATE_REQUEST_QT
/*
On 2024/07/06 @paulpv asked on OBS Discord #development and @RytoEX confirmed
that OBS removed TLS support from Qt:
https://discord.com/channels/348973006581923840/374636084883095554/1259188627699925167
> @paulpv Q: Why does obs-deps/deps.qt set -DINPUT_openssl:STRING=no?
>	Is there some problem letting Qt use OpenSSL?
>	When I try to call QNetworkAccessManager.get("https://www.github.com") in my plugin,
>	I get a bunch of `No functional TLS backend was found`, `No TLS backend is available`, and
>	`TLS initialization failed` errors; my code works fine on http urls.
https://discord.com/channels/348973006581923840/374636084883095554/1259193095652638879
> @RytoEX A: We accidentally disabled copying the Qt TLS backends. It'll be fixed in a future release. 
>	It's on my backlog of "local branches that fix this that haven't been PR'd".
>	We decided intentionally to opt for the native TLS backends for Qt on Windows and macOS,
>	but see other messages where copying it got accidentally disabled.

Options:
1. Wait for OBS to fix this: Unknown how long this will be.
	I think the place to watch for changes that fix this is:
	* https://github.com/obsproject/obs-deps/blob/master/deps.qt/qt6.ps1
	* https://github.com/obsproject/obs-deps/blob/master/deps.qt/qt6.zsh
2. Compile DistroAV's own Qt6 dep w/ TLS enabled: No thanks!
3. Make only http requests; Don't make https requests:
	Ignoring the security concerns, this works when testing against the emulator but won't work for
	non-emulator (actual cloud) due to http://distroav.org auto-redirecting to https://distroav.org.
	My original thought was that even though https requests are failing for TLS reasons, http
	requests are still working fine and this code can use those until the TLS issue is fixed.
	I could swear that http requests to firebase ussed to work fine.
	But latest testing shows that http requests are auto forwarding to https, so it looks like this
	https/TLS problem cannot be avoided by calling just http. :/
	Until OBS adds back TLS support, this code will have to resort to using non-Qt https requests.
4. Make https requests a non-Qt way (ex: "libcurl"):
	OBS does this:
		https://github.com/obsproject/obs-studio/tree/master/UI/update
		https://github.com/obsproject/obs-studio/blob/master/UI/update/shared-update.cpp
		https://github.com/obsproject/obs-studio/blob/master/UI/remote-text.cpp
	occ-ai/obs-backgroundremoval does things a little different:
		https://github.com/occ-ai/obs-backgroundremoval/tree/main/src/update-checker
	Neither of these are highly complicated, but it is silly this has to be done and I
	would prefer to write little to no knowlingly future throwaway code (see Option #1).
5. Direct calls to Firebase Functions: the problem with this is "how to cancel any pending function call"?
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

void onCheckForUpdateNetworkFinish(const int httpStatusCode,
				   const QString &responseData,
				   const QString &errorData,
				   UserRequestCallback userRequestCallback)
{
	auto logDebug = Config::LogDebug();
	auto logVerbose = logDebug || Config::LogVerbose();
	if (logDebug) {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish(httpStatusCode=%d, responseData=`%s`, errorData=`%s`, userRequestCallback=%s)",
		     httpStatusCode, QT_TO_UTF8(responseData),
		     QT_TO_UTF8(errorData),
		     userRequestCallback ? "..." : "nullptr");
	} else {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish(httpStatusCode=%d, responseData=..., errorData=`%s`, userRequestCallback=%s)",
		     httpStatusCode, QT_TO_UTF8(errorData),
		     userRequestCallback ? "..." : "nullptr");
	}

	auto pluginUpdateInfo =
		PluginUpdateInfo(httpStatusCode, responseData, errorData);
	if (!pluginUpdateInfo.errorData.isEmpty()) {
		blog(LOG_WARNING,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: Error! httpStatusCode=%d, errorData=`%s`; ignoring response",
		     httpStatusCode, QT_TO_UTF8(pluginUpdateInfo.errorData));
		if (userRequestCallback) {
			userRequestCallback(pluginUpdateInfo);
		}
		return;
	}

	if (logVerbose) {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: Success! httpStatusCode=%d",
		     httpStatusCode);
	}

	if (logDebug) {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: jsonDocument=`%s`",
		     pluginUpdateInfo.jsonDocument.toJson().constData());
	}
	if (logVerbose) {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: versionCurrent=%s",
		     QT_TO_UTF8(pluginUpdateInfo.versionCurrent.toString()));
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: %sversionLatest=%s",
		     pluginUpdateInfo.fakeVersionLatest ? "FAKE " : "",
		     QT_TO_UTF8(pluginUpdateInfo.versionLatest.toString()));
	}

	auto config = Config::Current();

	auto minAutoUpdateCheckIntervalSeconds =
		pluginUpdateInfo.minAutoUpdateCheckIntervalSeconds;
	config->MinAutoUpdateCheckIntervalSeconds(
		minAutoUpdateCheckIntervalSeconds);

	auto forceUpdate = Config::UpdateForce();

	if (userRequestCallback) {
		// User requested update check ignores SkipUpdateVersion
		userRequestCallback(pluginUpdateInfo);
	} else {
		// Non-user requested update check respects SkipUpdateVersion
		auto versionSkip = config->SkipUpdateVersion();
		if (!forceUpdate &&
		    pluginUpdateInfo.versionLatest == versionSkip) {
			blog(LOG_INFO,
			     "[obs-ndi] onCheckForUpdateNetworkFinish: versionLatest == versionSkip(%s); ignoring update",
			     QT_TO_UTF8(versionSkip.toString()));
			return;
		}
	}

	// Both user requested and non-user requested update checks ignore versionLatest <= versionCurrent
	if (!forceUpdate &&
	    pluginUpdateInfo.versionLatest <= pluginUpdateInfo.versionCurrent) {
		blog(LOG_INFO,
		     "[obs-ndi] onCheckForUpdateNetworkFinish: versionLatest <= versionCurrent; ignoring update");
		return;
	}

	auto uiDelayMillis =
		userRequestCallback ? 0 : pluginUpdateInfo.uiDelayMillis;
	if (logDebug) {
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
		// Our logic needs to set `update_dialog = nullptr` after it finishes...
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
			update_request->exit(1);
		}
	}
	blog(LOG_INFO, "[obs-ndi] -updateCheckStop()");
}

bool updateCheckStart(UserRequestCallback userRequestCallback)
{
	auto logDebug = Config::LogDebug();
	auto methodSignature =
		QString("updateCheckStart(userRequestCallback=%1)")
			.arg(userRequestCallback ? "..." : "nullptr");
	blog(LOG_INFO, "[obs-ndi] +%s", QT_TO_UTF8(methodSignature));

	auto isAutoCheck = userRequestCallback == nullptr;

	auto config = Config::Current();
	if (isAutoCheck && !config->AutoCheckForUpdates()) {
		blog(LOG_INFO,
		     "[obs-ndi] updateCheckStart: AutoCheckForUpdates is disabled; ignoring");
		blog(LOG_INFO, "[obs-ndi] -%s", QT_TO_UTF8(methodSignature));
		return false;
	}

	if (isUpdatePendingOrShowing()) {
		if (update_dialog) {
			update_dialog->raise();
		}
		blog(LOG_INFO,
		     "[obs-ndi] updateCheckStart: update pending or showing; ignoring");
		blog(LOG_INFO, "[obs-ndi] -%s", QT_TO_UTF8(methodSignature));
		return false;
	}

	//
	// Avoid hitting the server too often by limiting auto-checks to every minAutoUpdateCheckIntervalSeconds.
	//
	if (isAutoCheck && !Config::UpdateLastCheckIgnore()) {
		auto now = QDateTime::currentDateTime();

		auto minAutoUpdateCheckIntervalSeconds =
			config->MinAutoUpdateCheckIntervalSeconds();
		if (minAutoUpdateCheckIntervalSeconds > 0) {
			auto lastUpdateCheck = config->LastUpdateCheck();
			auto elapsedSeconds = lastUpdateCheck.secsTo(now);
			if (elapsedSeconds <
			    minAutoUpdateCheckIntervalSeconds) {
				blog(LOG_INFO,
				     "[obs-ndi] updateCheckStart: elapsedSeconds=%lld < minAutoUpdateCheckIntervalSeconds=%d; ignoring",
				     elapsedSeconds,
				     minAutoUpdateCheckIntervalSeconds);
				blog(LOG_INFO, "[obs-ndi] -%s",
				     QT_TO_UTF8(methodSignature));
				return false;
			}
		}
	}
	config->LastUpdateCheck(QDateTime::currentDateTime());

	auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());

//#define DIRECT_REQUEST_GITHUB
#ifdef DIRECT_REQUEST_GITHUB
	// Used to test directly hitting github instead of going through distroav.org firebase hosting+functions.
	QUrl url(
		"https://api.github.com/repos/DistroAV/DistroAV/releases/latest");
#else
	QUrl url(rehostUrl(PLUGIN_UPDATE_URL));
	if (logDebug) {
		blog(LOG_INFO, "[obs-ndi] updateCheckStart: url=`%s`",
		     QT_TO_UTF8(url.toString()));
	}

	auto pluginVersion = QString(PLUGIN_VERSION);
	auto obsGuid = GetProgramGUID();
	auto module_hash_sha256 = GetObsCurrentModuleSHA256();
	auto userAgent = QString("DistroAV/%1 (OBS/%2 %3; %4; %5; %6) %7")
				 .arg(pluginVersion)
				 .arg(obs_get_version_string())
				 .arg(obsGuid)
				 .arg(ndiLib->version())
				 .arg(QSysInfo::prettyProductName())
				 .arg(QSysInfo::currentCpuArchitecture())
				 .arg(module_hash_sha256);
	if (logDebug) {
		blog(LOG_INFO, "[obs-ndi] updateCheckStart: userAgent=`%s`",
		     QT_TO_UTF8(userAgent));
	}

	QJsonObject postObj;
	postObj["config"] =
		QJsonObject{{"autoCheck", config->AutoCheckForUpdates()}};
	postObj["request"] = QJsonObject{{"autoCheck", isAutoCheck}};
	std::string postData;
	if (!postObj.isEmpty()) {
		QJsonDocument postJson(postObj);
		postData = postJson.toJson().toStdString();
	}
#endif

	//qputenv("QT_LOGGING_RULES", "qt.network.ssl=true");
	blog(LOG_INFO,
	     "[obs-ndi] updateCheckStart: QSslSocket{ supportsSsl=%s, sslLibraryBuildVersionString=`%s`, sslLibraryVersionString=`%s`}",
	     QSslSocket::supportsSsl() ? "true" : "false",
	     QT_TO_UTF8(QSslSocket::sslLibraryBuildVersionString()),
	     QT_TO_UTF8(QSslSocket::sslLibraryVersionString()));
	/*
	The above should output something like:
	```
	info: [obs-ndi] updateCheckStart: QSslSocket{ supportsSsl=true, sslLibraryBuildVersionString=`OpenSSL 1.1.1l  24 Aug 2021`, sslLibraryVersionString=`OpenSSL 1.1.1l  24 Aug 2021`}	
	```
	Instead it is outputting:
	```
	warning: No functional TLS backend was found
	warning: No functional TLS backend was found
	warning: No functional TLS backend was found
	info: [obs-ndi] updateCheckStart: QSslSocket{ supportsSsl=false, sslLibraryBuildVersionString=``, sslLibraryVersionString=``}
	```
	This appears to confirm that OBS' Qt is not built with SSL support. :(
	*/

#ifdef UPDATE_REQUEST_QT
	update_request = new QNetworkRequest(url);
#else
	update_request = new RemoteTextThread(url.toString().toStdString(),
					      "application/json", postData,
					      UPDATE_TIMEOUT_SEC);
	// Our logic needs to set `update_request = nullptr` after it finishes...
	QObject::connect(
		update_request, &RemoteTextThread::finished, main_window,
		[]() {
			update_request->deleteLater();
			update_request = nullptr;
		},
		Qt::QueuedConnection);
#endif

#ifndef DIRECT_REQUEST_GITHUB
#ifdef UPDATE_REQUEST_QT
	update_request->setRawHeader("User-Agent", userAgent.toUtf8());
#else
	userAgent = "User-Agent: " + userAgent;
	update_request->headers.push_back(userAgent.toStdString());
#endif
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
		[userRequestCallback](int httpStatusCode,
				      const QString &responseData,
				      const QString &errorData) {
#if 0
			blog(LOG_INFO,
			     "[obs-ndi] updateCheckStart: Result: httpStatusCode=%d, responseData=`%s`, errorData=`%s`",
			     httpCode, QT_TO_UTF8(responseData),
			     QT_TO_UTF8(errorData));
#endif
			onCheckForUpdateNetworkFinish(httpStatusCode,
						      responseData, errorData,
						      userRequestCallback);
		},
		Qt::QueuedConnection);
	update_request->start();
#endif
	blog(LOG_INFO, "[obs-ndi] -%s", QT_TO_UTF8(methodSignature));
	return true;
}
