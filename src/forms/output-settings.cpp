/******************************************************************************
	Copyright (C) 2016-2024 DistroAV <contact@distroav.org>

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

#include "output-settings.h"

#include "plugin-main.h"
#include "main-output.h"
#include "preview-output.h"
#include "update.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QMessageBox>
#include <QProgressDialog>
#include <QProcess>
#include <QPointer>
#include <QRegularExpression>

OutputSettings::OutputSettings(QWidget *parent) : QDialog(parent), ui(new Ui::OutputSettings)
{
	ui->setupUi(this);

	connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onFormAccepted()));

	// Requirements checks and status display
	// Global rules for color based on requirement checks: red for fail, green for pass. Text is set per check below.
	auto applyStatus = [](QLabel *label, bool ok, const QString &message) {
		label->setText(QString::fromUtf8("%1 %2").arg(ok ? "✓" : "✗", message));
		label->setStyleSheet(ok ? "QWidget { color: #2e7d32; }" : "QWidget { color: #c62828; }");
	};

	// OBS Version Check
	auto obsVersion = QString::fromUtf8(obs_get_version_string());
	ui->labelReqObsTitle->setText(makeLink("#", QT_TO_UTF8(ui->labelReqObsTitle->text())));
	connect(ui->labelReqObsTitle, &QLabel::linkActivated, [this, obsVersion](const QString &) {
		QApplication::clipboard()->setText(obsVersion);
		QMessageBox::information(this, Str("NDIPlugin.OutputSettings.TextCopied"),
					 Str("NDIPlugin.OutputSettings.TextCopiedToClipboard"));
	});

	auto obsVersionCheckResult = is_version_supported(QT_TO_UTF8(obsVersion), PLUGIN_MIN_OBS_VERSION);
	applyStatus(ui->labelReqObsStatus, obsVersionCheckResult,
		    obsVersionCheckResult ? QString("OK (%1 ≥ %2)").arg(obsVersion, PLUGIN_MIN_OBS_VERSION)
					  : QString("Too old (%1 < %2)").arg(obsVersion, PLUGIN_MIN_OBS_VERSION));

	// DistroAV Version Check
	auto pluginVersionText = QString("%1 %2").arg(PLUGIN_DISPLAY_NAME, PLUGIN_VERSION);
	ui->labelReqDistroTitle->setText(makeLink("#", QT_TO_UTF8(pluginVersionText)));
	connect(ui->labelReqDistroTitle, &QLabel::linkActivated, [this, pluginVersionText](const QString &) {
		QApplication::clipboard()->setText(pluginVersionText);
		QMessageBox::information(this, Str("NDIPlugin.OutputSettings.TextCopied"),
					 Str("NDIPlugin.OutputSettings.TextCopiedToClipboard"));
	});

	applyStatus(ui->labelReqDistroStatus, true, QString("Loaded (%1)").arg(PLUGIN_VERSION));

	// NDI Version Check
	QString ndiVersionFull;
	QString ndiVersionShort;
	if (ndiLib) {
		ndiVersionFull = QString(ndiLib->version());
		ndiVersionShort =
			QRegularExpression(R"((\d+\.\d+(\.\d+)?(\.\d+)?$))").match(ndiLib->version()).captured(1);
	}

	ui->labelReqNdiTitle->setText(makeLink("#", QT_TO_UTF8(ui->labelReqNdiTitle->text())));
	connect(ui->labelReqNdiTitle, &QLabel::linkActivated, [this, ndiVersionFull](const QString &) {
		QApplication::clipboard()->setText(ndiVersionFull);
		QMessageBox::information(this, Str("NDIPlugin.OutputSettings.TextCopied"),
					 Str("NDIPlugin.OutputSettings.TextCopiedToClipboard"));
	});

	auto ndiVersionCheckResult = !ndiVersionShort.isEmpty() &&
				     is_version_supported(QT_TO_UTF8(ndiVersionShort), PLUGIN_MIN_NDI_VERSION);
	applyStatus(ui->labelReqNdiStatus, ndiVersionCheckResult,
		    ndiVersionCheckResult
			    ? QString("OK (%1 ≥ %2)").arg(ndiVersionShort, PLUGIN_MIN_NDI_VERSION)
			    : (ndiVersionShort.isEmpty()
				       ? QString("Missing (need %1+)").arg(PLUGIN_MIN_NDI_VERSION)
				       : QString("Too old (%1 < %2)").arg(ndiVersionShort, PLUGIN_MIN_NDI_VERSION)));

	// DistroAV Section Logic
	// Check For Update Button
	connect(ui->pushButtonCheckForUpdate, &QPushButton::clicked, [this]() {
		// Whew! QProgressDialog is ugly on Windows!
		// TODO: Write our own.
		QPointer<QProgressDialog> progressDialog =
			new QProgressDialog(QTStr("NDIPlugin.Update.CheckingForUpdate.Text").arg(PLUGIN_DISPLAY_NAME),
					    Str("NDIPlugin.Update.CheckingForUpdate.Cancel"), 0, 0, this);
		progressDialog->setAttribute(Qt::WA_DeleteOnClose, true);
		progressDialog->setWindowModality(Qt::WindowModal);
		connect(progressDialog, &QProgressDialog::canceled, [progressDialog]() {
			updateCheckStop();
			progressDialog->close();
		});
		auto checking = updateCheckStart([this,
						  progressDialog](const PluginUpdateInfo &pluginUpdateInfo) -> bool {
			if (progressDialog != nullptr) {
				progressDialog->close();
			}

			if (!pluginUpdateInfo.errorData.isEmpty()) {
				QString errorData = pluginUpdateInfo.errorData;
				if (LOG_LEVEL >= LOG_DEBUG) {
					QJsonParseError parseError;
					QJsonDocument jsonDoc =
						QJsonDocument::fromJson(errorData.toUtf8(), &parseError);
					if (parseError.error == QJsonParseError::NoError) {
						QJsonObject jsonObject = jsonDoc.object();
						if (jsonObject.contains("error")) {
							errorData = jsonObject["error"].toString();
						}
					}
				}

				auto errorText = QTStr("NDIPlugin.Update.CheckingForUpdate.Error.Text");
				errorText += QString("<pre>%1</pre>").arg(errorData);

				if (pluginUpdateInfo.httpStatusCode == 404    // Not Found
				    || pluginUpdateInfo.httpStatusCode == 412 // Precondition Failed
				) {
					// Only someone building and loading their own plugin should see this.
					// This is effectively just a code comment to them/me in the UI.
					// English only text is OK here, just like it is for all code comments.
					errorText += R"(
The update server says you are not using an official release.<br>
<br>
Update checks are only supported for official releases.
)";
					if (LOG_LEVEL >= LOG_DEBUG) {
						errorText += R"(<br>
<br>
If you are running a local build, don't forget to add your build info to the update server.
)";
					}
				}

				QMessageBox::warning(this, Str("NDIPlugin.Update.CheckingForUpdate.Error.Title"),
						     errorText);
				return false;
			}

			if (pluginUpdateInfo.versionLatest <= pluginUpdateInfo.versionCurrent &&
			    Config::UpdateForce < 1) {
				QMessageBox::information(
					this, QTStr("NDIPlugin.Update.NoUpdateAvailable").arg(PLUGIN_DISPLAY_NAME),
					QTStr("NDIPlugin.Update.YouAreUpToDate")
						.arg(PLUGIN_DISPLAY_NAME, pluginUpdateInfo.versionCurrent.toString()));
				return false;
			}

			return false;
		});
		if (checking) {
			progressDialog->show();
		} else {
			progressDialog->deleteLater();
		}
	});

	// Auto re-install DistroAV Plugin Button
	connect(ui->pushButtonInstallPlugin, &QPushButton::clicked, [this]() {
		obs_log(LOG_DEBUG, "Re-Install DistroAV Plugin button clicked");
#if defined(Q_OS_MACOS)
		const auto script = QString(
			"tell application \"Terminal\"\n"
			"activate\n"
			"do script \"brew tap distroav/distroav && brew reinstall --cask distroav/distroav/distroav && exit\"\n"
			"end tell");

		if (!QProcess::startDetached("/usr/bin/osascript", QStringList() << "-e" << script)) {
			QMessageBox::warning(
				this, Str("NDIPlugin.OneclickInstallError.Title"),
					     Str("NDIPlugin.OneclickInstallError.Message") + "brew reinstall --cask distroav/distroav/distroav");
		}
#elif defined(Q_OS_WIN)
		if (!QProcess::startDetached(
				"cmd.exe",
				QStringList()
					<< "/c"
					<< "start"
					<< "cmd.exe"
					<< "/c"
					<< "winget install -e --id DistroAV.DistroAV --accept-package-agreements --accept-source-agreements || pause")) {
			QMessageBox::warning(this, Str("NDIPlugin.OneclickInstallError.Title"),
					     Str("NDIPlugin.OneclickInstallError.Message") + "winget install -e --id DistroAV.DistroAV");
			obs_log(LOG_DEBUG, "Install DistroAV button: something went wrong");
		}
#else
			QMessageBox::information(this, "Unsupported platform",
						"Automatic DistroAV installation is currently only supported on macOS and Windows.");
#endif
	});

	// NDI Library Section Logic
	connect(ui->pushButtonGetNdi, &QPushButton::clicked,
		[]() { QDesktopServices::openUrl(QUrl(rehostUrl(PLUGIN_REDIRECT_NDI_REDIST_URL))); });

	connect(ui->pushButtonInstallNdi, &QPushButton::clicked, [this]() {
		obs_log(LOG_DEBUG, "Install NDI button clicked");
#if defined(Q_OS_MACOS)
		const auto script = QString("tell application \"Terminal\"\n"
					    "activate\n"
					    "do script \"brew reinstall libndi && exit\"\n"
					    "end tell");

		if (!QProcess::startDetached("/usr/bin/osascript", QStringList() << "-e" << script)) {
			QMessageBox::warning(this, Str("NDIPlugin.OneclickInstallError.Title"),
					     Str("NDIPlugin.OneclickInstallError.Message") + "brew reinstall libndi");
		}
#elif defined(Q_OS_WIN)
		if (!QProcess::startDetached(
				"cmd.exe",
				QStringList()
					<< "/c"
					<< "start"
					<< "cmd.exe"
					<< "/c"
					<< "winget install -e --id NDI.NDIRuntime --accept-package-agreements --accept-source-agreements || pause")) {
			QMessageBox::warning(this, Str("NDIPlugin.OneclickInstallError.Title"),
					     Str("NDIPlugin.OneclickInstallError.Message") + "winget install -e --id NDI.NDIRuntime");
			obs_log(LOG_DEBUG, "Install NDI button: something went wrong");
		}
#else
		QMessageBox::information(this, "Unsupported platform",
					 "Automatic NDI installation is currently only supported on macOS and Windows.");
#endif
	});

	connect(ui->pushButtonDiscord, &QPushButton::clicked,
		[]() { QDesktopServices::openUrl(QUrl(rehostUrl(PLUGIN_REDIRECT_DISCORD_URL))); });

	connect(ui->pushButtonDonate, &QPushButton::clicked,
		[]() { QDesktopServices::openUrl(QUrl(rehostUrl(PLUGIN_REDIRECT_DONATE_URL))); });

	connect(ui->pushButtonWiki, &QPushButton::clicked,
		[]() { QDesktopServices::openUrl(QUrl(rehostUrl(PLUGIN_REDIRECT_HELP_URL))); });

	// Cosmetic display of NDI registered trademark info with link to NDI website
	ui->labelNdiRegisteredTrademark->setTextFormat(Qt::RichText);
	ui->labelNdiRegisteredTrademark->setTextInteractionFlags(Qt::TextBrowserInteraction);
	ui->labelNdiRegisteredTrademark->setOpenExternalLinks(true);
	ui->labelNdiRegisteredTrademark->setText(QString("%1 %2").arg(
		NDI_IS_A_REGISTERED_TRADEMARK_TEXT, QString(" - <a href=\"https://ndi.video\">ndi.video</a>")));
}

void OutputSettings::onFormAccepted()
{
	auto config = Config::Current();
	auto last_config = *config;

	config->OutputEnabled = ui->mainOutputGroupBox->isChecked();
	config->OutputName = ui->mainOutputName->text();
	config->OutputGroups = ui->mainOutputGroups->text();

	config->PreviewOutputEnabled = ui->previewOutputGroupBox->isChecked();
	config->PreviewOutputName = ui->previewOutputName->text();
	config->PreviewOutputGroups = ui->previewOutputGroups->text();

	config->TallyProgramEnabled = ui->tallyProgramCheckBox->isChecked();
	config->TallyPreviewEnabled = ui->tallyPreviewCheckBox->isChecked();

	config->AutoCheckForUpdates(ui->checkBoxAutoCheckForUpdates->isChecked());

	auto mainSupported = ui->mainOutputGroupBox->isEnabled();

	obs_log(LOG_INFO, "Main output supported='%d'", mainSupported);

	// Output settings for debugging & diagnosis
	obs_log(LOG_INFO,
		"Output Settings set to MainEnabled='%d', MainName='%s', MainGroup='%s', PreviewEnabled='%d', PreviewName='%s', PreviewGroup='%s'",
		config->OutputEnabled, config->OutputName.toUtf8().constData(),
		config->PreviewOutputGroups.toUtf8().constData(), config->PreviewOutputEnabled,
		config->PreviewOutputName.toUtf8().constData(), config->PreviewOutputGroups.toUtf8().constData());

	config->Save();

	if (mainSupported && config->OutputEnabled && !config->OutputName.isEmpty()) {
		if ((last_config.OutputEnabled != config->OutputEnabled) ||
		    (last_config.OutputName != config->OutputName) ||
		    (last_config.OutputGroups != config->OutputGroups)) {
			// The Output is supported and enabled, OutputName exists and a Name or GroupName has changed since last form submission
			obs_log(LOG_INFO, "Initializing Main output");
			main_output_init();
		}
	} else {
		main_output_deinit();
	}
	if (config->PreviewOutputEnabled && !config->PreviewOutputName.isEmpty()) {
		if ((last_config.PreviewOutputEnabled != config->PreviewOutputEnabled) ||
		    (last_config.PreviewOutputName != config->PreviewOutputName) ||
		    (last_config.PreviewOutputGroups != config->PreviewOutputGroups)) {
			// The Preview Output is enabled, OutputName exists and a Name or GroupName has changed since last form submission
			obs_log(LOG_INFO, "Initializing Preview output");
			preview_output_init();
		}
	} else {
		preview_output_deinit();
	}
}

void OutputSettings::showEvent(QShowEvent *)
{
	auto config = Config::Current();

	// Enable Output (Main & Preview) settings as long as Main Output can be supported.

	if (main_output_is_supported()) {
		ui->mainOutputGroupBox->setEnabled(true);
		ui->previewOutputGroupBox->setEnabled(true);
	} else {
		ui->mainOutputGroupBox->setEnabled(false);
		ui->previewOutputGroupBox->setEnabled(false);
	}

	ui->mainOutputGroupBox->setChecked(config->OutputEnabled);
	ui->mainOutputName->setText(config->OutputName);
	ui->mainOutputGroups->setText(config->OutputGroups);

	auto lastError = main_output_last_error();
	ui->mainOutputLastError->setText(lastError);
	if (lastError.isEmpty()) {
		ui->mainOutputLastError->setFixedHeight(0); // don't waste dialog space if error is no longer valid.
	} else {
		ui->mainOutputLastError->setFixedHeight(ui->mainOutputLastError->sizeHint().height());
	}

	ui->previewOutputGroupBox->setChecked(config->PreviewOutputEnabled);
	ui->previewOutputName->setText(config->PreviewOutputName);
	ui->previewOutputGroups->setText(config->PreviewOutputGroups);

	ui->tallyProgramCheckBox->setChecked(config->TallyProgramEnabled);
	ui->tallyPreviewCheckBox->setChecked(config->TallyPreviewEnabled);

	ui->checkBoxAutoCheckForUpdates->setChecked(config->AutoCheckForUpdates());
}

void OutputSettings::toggleShowHide()
{
	setVisible(!isVisible());
}
