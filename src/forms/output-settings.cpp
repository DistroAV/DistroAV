
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
#include "../config-notifier.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QIcon>
#include <QMessageBox>
#include <QProgressDialog>
#include <QProcess>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QStyle>
#include <QToolButton>

OutputSettings::OutputSettings(QWidget *parent) : QDialog(parent), ui(new Ui::OutputSettings)
{
	ui->setupUi(this);
	ui->toolButtonNdiDisclaimer->setIcon(QIcon::fromTheme("dialog-information-symbolic",
							      style()->standardIcon(QStyle::SP_MessageBoxInformation)));
	ui->toolButtonNdiDisclaimer->setToolTip(NDI_IS_A_REGISTERED_TRADEMARK_TEXT);
	ui->toolButtonNdiDisclaimer->setAccessibleName(NDI_IS_A_REGISTERED_TRADEMARK_TEXT);
	connect(ui->toolButtonNdiDisclaimer, &QToolButton::clicked, this, [this]() {
		QMessageBox::information(this, QTStr("NDIPlugin.OutputSettings.NdiDisclaimer.Title"),
					 QString::fromUtf8(NDI_IS_A_REGISTERED_TRADEMARK_TEXT));
	});
	auto loadModuleIcon = [](const char *relative_path) {
		char *module_path = obs_module_file(relative_path);
		if (!module_path)
			return QIcon();

		const QIcon icon(QString::fromUtf8(module_path));
		bfree(module_path);
		return icon;
	};
	ui->toolButtonWiki->setIcon(style()->standardIcon(QStyle::SP_DialogHelpButton));
	ui->toolButtonDiscord->setIcon(loadModuleIcon("icons/discord.svg"));
	ui->toolButtonGithub->setIcon(loadModuleIcon("icons/github.svg"));
	ui->toolButtonDonate->setIcon(loadModuleIcon("icons/currency.svg"));
	ui->toolButtonAutoUpdateInfo->setIcon(style()->standardIcon(QStyle::SP_MessageBoxInformation));
	const auto autoUpdatePrivacyText = QTStr("NDIPlugin.OutputSettings.AutoUpdate.Privacy");
	ui->toolButtonAutoUpdateInfo->setToolTip(autoUpdatePrivacyText);
	ui->toolButtonAutoUpdateInfo->setAccessibleName(autoUpdatePrivacyText);
	const bool darkTheme = palette().color(QPalette::Window).lightness() < 128;
	auto loadThemeIcon = [&loadModuleIcon, darkTheme](const char *light, const char *dark) {
		return loadModuleIcon(darkTheme ? dark : light);
	};
	ui->toolButtonCheckForUpdate->setIcon(loadThemeIcon("icons/update.svg", "icons/update-dark.svg"));
	ui->toolButtonInstallPlugin->setIcon(loadThemeIcon("icons/repair.svg", "icons/repair-dark.svg"));
	ui->toolButtonGetNdi->setIcon(loadThemeIcon("icons/link.svg", "icons/link-dark.svg"));
	ui->toolButtonInstallNdi->setIcon(loadThemeIcon("icons/repair.svg", "icons/repair-dark.svg"));
	const auto statusCheckIcon = style()->standardIcon(QStyle::SP_DialogApplyButton);
	const auto statusCrossIcon = style()->standardIcon(QStyle::SP_MessageBoxCritical);
	auto statusIcon = [statusCheckIcon, statusCrossIcon](bool ok) {
		return ok ? statusCheckIcon : statusCrossIcon;
	};
	auto updateAutoUpdateState = [this, statusIcon](bool enabled) {
		const auto description = QTStr(enabled ? "NDIPlugin.OutputSettings.AutoUpdate.Enabled"
						       : "NDIPlugin.OutputSettings.AutoUpdate.Disabled");
		ui->toolButtonAutoCheckForUpdates->setIcon(statusIcon(enabled));
		ui->toolButtonAutoCheckForUpdates->setToolTip(description);
		ui->toolButtonAutoCheckForUpdates->setAccessibleName(description);
	};
	ui->toolButtonAutoCheckForUpdates->setStyleSheet(
		"QToolButton:checked, QToolButton:checked:hover, QToolButton:checked:pressed { "
		"background-color: transparent; border: none; }");
	connect(ui->toolButtonAutoCheckForUpdates, &QToolButton::toggled, this, updateAutoUpdateState);
	updateAutoUpdateState(ui->toolButtonAutoCheckForUpdates->isChecked());

	connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onFormAccepted()));

	// Listen for external config changes so the dialog can update while visible.
	connect(ConfigNotifier::instance(), &ConfigNotifier::configChanged, this, &OutputSettings::onConfigChanged);

	// Requirement statuses use icons with localized descriptions for tooltips and accessibility.
	auto applyStatus = [statusIcon](QLabel *label, bool ok, const QString &description) {
		label->setPixmap(statusIcon(ok).pixmap(22, 22));
		label->setToolTip(description);
		label->setAccessibleName(description);
	};

	// OBS Version Check
	auto obsVersion = QString::fromUtf8(obs_get_version_string());
	auto obsVersionDisplay = QString("%1 %2").arg("OBS", QString::fromUtf8(obs_get_version_string()));
	ui->labelReqObsTitle->setText(makeLink("#", QT_TO_UTF8(obsVersion)));
	ui->labelReqObsMinimum->setText(PLUGIN_MIN_OBS_VERSION);
	connect(ui->labelReqObsTitle, &QLabel::linkActivated, [this, obsVersionDisplay](const QString &) {
		QApplication::clipboard()->setText(obsVersionDisplay);
		QMessageBox::information(this, Str("NDIPlugin.OutputSettings.TextCopied"),
					 Str("NDIPlugin.OutputSettings.TextCopiedToClipboard"));
	});

	auto obsVersionCheckResult = is_version_supported(QT_TO_UTF8(obsVersion), PLUGIN_MIN_OBS_VERSION);
	applyStatus(ui->labelReqObsStatus, obsVersionCheckResult,
		    QTStr(obsVersionCheckResult ? "NDIPlugin.OutputSettings.Status.MeetsMinimum"
						: "NDIPlugin.OutputSettings.Status.BelowMinimum"));

	// DistroAV Version Check
	auto pluginVersionText = QString("%1 %2").arg(PLUGIN_DISPLAY_NAME, PLUGIN_VERSION);
	ui->labelReqDistroTitle->setText(makeLink("#", PLUGIN_VERSION));
	ui->labelReqDistroMinimum->setText(QTStr("NDIPlugin.OutputSettings.NotApplicable"));
	connect(ui->labelReqDistroTitle, &QLabel::linkActivated, [this, pluginVersionText](const QString &) {
		QApplication::clipboard()->setText(pluginVersionText);
		QMessageBox::information(this, Str("NDIPlugin.OutputSettings.TextCopied"),
					 Str("NDIPlugin.OutputSettings.TextCopiedToClipboard"));
	});

	applyStatus(ui->labelReqDistroStatus, true, QTStr("NDIPlugin.OutputSettings.Status.Loaded"));

	// NDI Version Check
	QString ndiVersionFull;
	QString ndiVersionShort;
	if (ndiLib) {
		ndiVersionFull = QString(ndiLib->version());
		ndiVersionShort =
			QRegularExpression(R"((\d+\.\d+(\.\d+)?(\.\d+)?$))").match(ndiLib->version()).captured(1);
	} else {
		ndiVersionFull = QTStr("NDIPlugin.OutputSettings.VersionNotDetected");
	}

	const auto ndiVersionDisplay = ndiVersionShort.isEmpty() ? ndiVersionFull : ndiVersionShort;
	ui->labelReqNdiTitle->setText(makeLink("#", QT_TO_UTF8(ndiVersionDisplay)));
	ui->labelReqNdiMinimum->setText(PLUGIN_MIN_NDI_VERSION);
	connect(ui->labelReqNdiTitle, &QLabel::linkActivated, [this, ndiVersionFull](const QString &) {
		QApplication::clipboard()->setText(ndiVersionFull);
		QMessageBox::information(this, Str("NDIPlugin.OutputSettings.TextCopied"),
					 Str("NDIPlugin.OutputSettings.TextCopiedToClipboard"));
	});

	auto ndiVersionCheckResult = !ndiVersionShort.isEmpty() &&
				     is_version_supported(QT_TO_UTF8(ndiVersionShort), PLUGIN_MIN_NDI_VERSION);
	applyStatus(ui->labelReqNdiStatus, ndiVersionCheckResult,
		    ndiVersionShort.isEmpty()
			    ? QTStr("NDIPlugin.OutputSettings.Status.NotDetected")
			    : QTStr(ndiVersionCheckResult ? "NDIPlugin.OutputSettings.Status.MeetsMinimum"
							  : "NDIPlugin.OutputSettings.Status.BelowMinimum")
				      .arg(PLUGIN_MIN_NDI_VERSION));

	// DistroAV Section Logic
	// Check For Update Button
	connect(ui->toolButtonCheckForUpdate, &QToolButton::clicked, [this]() {
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
				auto errorText = QTStr("NDIPlugin.Update.CheckingForUpdate.Error.Text");
				if (LOG_LEVEL >= LOG_DEBUG) {
					QString errorData = pluginUpdateInfo.errorData;
					QJsonParseError parseError;
					QJsonDocument jsonDoc =
						QJsonDocument::fromJson(errorData.toUtf8(), &parseError);
					if (parseError.error == QJsonParseError::NoError) {
						QJsonObject jsonObject = jsonDoc.object();
						if (jsonObject.contains("error")) {
							errorData = jsonObject["error"].toString();
						}
					}
					errorText += QString("<pre>%1</pre>").arg(errorData.toHtmlEscaped());
				}

				const bool unsupportedBuild = pluginUpdateInfo.httpStatusCode == 404 ||
							      pluginUpdateInfo.httpStatusCode == 412;
				if (unsupportedBuild) {
					errorText += QString("<br>") +
						     QTStr("NDIPlugin.Update.CheckingForUpdate.Error.UnsupportedBuild");
					if (LOG_LEVEL >= LOG_DEBUG) {
						errorText +=
							QString("<br>") +
							QTStr("NDIPlugin.Update.CheckingForUpdate.Error.LocalBuildHint");
					}
				}

				if (pluginUpdateInfo.httpStatusCode == 400) {
					errorText += QString("<br>") +
						     QTStr("NDIPlugin.Update.CheckingForUpdate.Error.InvalidRequest");
				}

				QMessageBox errorDialog(QMessageBox::Warning,
							Str("NDIPlugin.Update.CheckingForUpdate.Error.Title"),
							errorText, QMessageBox::Ok, this);
				QPushButton *downloadButton = nullptr;
				if (unsupportedBuild) {
					downloadButton = errorDialog.addButton(
						QTStr("NDIPlugin.Update.CheckingForUpdate.Error.Download"),
						QMessageBox::ActionRole);
				}
				errorDialog.exec();
				if (errorDialog.clickedButton() == downloadButton) {
					QDesktopServices::openUrl(QUrl(rehostUrl(PLUGIN_REDIRECT_DOWNLOAD_URL)));
				}
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

	if (LOG_LEVEL >= LOG_DEBUG) {
		// Debug-only: stray button that shows the update dialog with a hostile
		// sample `releaseNotes` so the markdown-render HTML-escape in
		// PluginUpdate (see src/forms/update.cpp) can be visually verified
		// without waiting for a real release. Only added when log level is
		// debug or verbose (e.g. `--distroav-debug`).
		auto testButton = new QPushButton("Test Release Notes (debug)", this);
		testButton->setToolTip("Debug: show the update dialog with sample hostile releaseNotes "
				       "(HTML / script injection attempts) to verify the markdown escape.");
		ui->horizontalLayoutDebugFooter->addWidget(testButton);
		connect(testButton, &QPushButton::clicked, [] { showSampleUpdateDialog(); });
	}

	auto confirmAutomatedFix = [this](const QString &command) {
		QMessageBox prompt(QMessageBox::Warning, QTStr("NDIPlugin.OutputSettings.AutoFix.Title"),
				   QTStr("NDIPlugin.OutputSettings.AutoFix.Message").arg(command.toHtmlEscaped()),
				   QMessageBox::NoButton, this);
		prompt.setMaximumWidth(500);
		auto *proceedButton =
			prompt.addButton(QTStr("NDIPlugin.OutputSettings.AutoFix.Proceed"), QMessageBox::AcceptRole);
		prompt.addButton(QTStr("NDIPlugin.OutputSettings.AutoFix.Cancel"), QMessageBox::RejectRole);
		prompt.setDefaultButton(proceedButton);
		prompt.exec();
		return prompt.clickedButton() == proceedButton;
	};

	// Auto re-install DistroAV Plugin Button
	connect(ui->toolButtonInstallPlugin, &QToolButton::clicked, [this, confirmAutomatedFix]() {
		QString command;
#if defined(Q_OS_MACOS)
		command =
			QStringLiteral("brew tap distroav/tap && brew reinstall --cask distroav/tap/distroav && exit");
#elif defined(Q_OS_WIN)
		command = QStringLiteral(
			"winget install -e --id DistroAV.DistroAV --accept-package-agreements --accept-source-agreements --force || pause");
#else
		QMessageBox::information(this, QTStr("NDIPlugin.OutputSettings.AutoFix.Title"),
					 QTStr("NDIPlugin.OutputSettings.AutoFix.UnsupportedPlatform"));
			return;
#endif
		if (!confirmAutomatedFix(command))
			return;

		obs_log(LOG_DEBUG, "Re-Install DistroAV Plugin button clicked");
#if defined(Q_OS_MACOS)
		const auto script = QString("tell application \"Terminal\"\n"
					    "activate\n"
					    "do script \"%1\"\n"
					    "end tell")
					    .arg(command);

		if (!QProcess::startDetached("/usr/bin/osascript", QStringList() << "-e" << script)) {
			QMessageBox::warning(this, QTStr("NDIPlugin.OneclickInstallError.Title"),
					     QTStr("NDIPlugin.OneclickInstallError.Message") + command);
		}
#elif defined(Q_OS_WIN)
		if (!QProcess::startDetached(
				"cmd.exe",
				QStringList()
					<< "/c"
					<< "start"
					<< "cmd.exe"
					<< "/c" << command)) {
			QMessageBox::warning(this, QTStr("NDIPlugin.OneclickInstallError.Title"),
					 QTStr("NDIPlugin.OneclickInstallError.Message") + command);
			obs_log(LOG_DEBUG, "Install DistroAV button: something went wrong");
		}
#endif
	});

	// NDI Library Section Logic
	connect(ui->toolButtonGetNdi, &QToolButton::clicked,
		[]() { QDesktopServices::openUrl(QUrl(rehostUrl(PLUGIN_REDIRECT_NDI_REDIST_URL))); });

	connect(ui->toolButtonInstallNdi, &QToolButton::clicked, [this, confirmAutomatedFix]() {
		QString command;
#if defined(Q_OS_MACOS)
		command = QStringLiteral("brew reinstall libndi && exit");
#elif defined(Q_OS_WIN)
		command = QStringLiteral(
			"winget install -e --id NDI.NDIRuntime --accept-package-agreements --accept-source-agreements --force || pause");
#else
		QMessageBox::information(this, QTStr("NDIPlugin.OutputSettings.AutoFix.Title"),
					 QTStr("NDIPlugin.OutputSettings.AutoFix.UnsupportedPlatform"));
			return;
#endif
		if (!confirmAutomatedFix(command))
			return;

		obs_log(LOG_DEBUG, "Install NDI button clicked");
#if defined(Q_OS_MACOS)
		const auto script = QString("tell application \"Terminal\"\n"
					    "activate\n"
					    "do script \"%1\"\n"
					    "end tell")
					    .arg(command);

		if (!QProcess::startDetached("/usr/bin/osascript", QStringList() << "-e" << script)) {
			QMessageBox::warning(this, QTStr("NDIPlugin.OneclickInstallError.Title"),
					     QTStr("NDIPlugin.OneclickInstallError.Message") + command);
		}
#elif defined(Q_OS_WIN)
		if (!QProcess::startDetached(
				"cmd.exe",
				QStringList()
					<< "/c"
					<< "start"
					<< "cmd.exe"
					<< "/c" << command)) {
			QMessageBox::warning(this, QTStr("NDIPlugin.OneclickInstallError.Title"),
					 QTStr("NDIPlugin.OneclickInstallError.Message") + command);
			obs_log(LOG_DEBUG, "Install NDI button: something went wrong");
		}
#endif
	});

	connect(ui->toolButtonDiscord, &QToolButton::clicked,
		[]() { QDesktopServices::openUrl(QUrl(rehostUrl(PLUGIN_REDIRECT_DISCORD_URL))); });

	connect(ui->toolButtonGithub, &QToolButton::clicked,
		[]() { QDesktopServices::openUrl(QUrl("https://github.com/DistroAV/DistroAV")); });

	connect(ui->toolButtonDonate, &QToolButton::clicked,
		[]() { QDesktopServices::openUrl(QUrl(rehostUrl(PLUGIN_REDIRECT_DONATE_URL))); });
	connect(ui->toolButtonAutoUpdateInfo, &QToolButton::clicked,
		[]() { QDesktopServices::openUrl(QUrl(rehostUrl(PLUGIN_REDIRECT_PRIVACY_URL))); });

	connect(ui->toolButtonWiki, &QToolButton::clicked,
		[]() { QDesktopServices::openUrl(QUrl(rehostUrl(PLUGIN_REDIRECT_HELP_URL))); });

	auto donationText = QTStr("NDIPlugin.Donate");
	donationText.replace(QStringLiteral("DistroAV"), makeLink(PLUGIN_REDIRECT_HOME_URL, "DistroAV"));
	ui->labelDonate->setTextFormat(Qt::RichText);
	ui->labelDonate->setOpenExternalLinks(true);
	ui->labelDonate->setTextInteractionFlags(Qt::LinksAccessibleByKeyboard | Qt::LinksAccessibleByMouse);
	ui->labelDonate->setText(donationText);
}
void replace_invalid_filename_chars(QString *s)
{
	if (!s)
		return;
	const QString forbidden = "\\/:*?\"<>|";
	for (QChar &ch : *s) {
		if (forbidden.contains(ch))
			ch = ' ';
	}
}
void OutputSettings::onFormAccepted()
{
	auto config = Config::Current();
	auto last_config = *config;

	config->OutputEnabled = ui->mainOutputGroupBox->isChecked();
	config->OutputName = ui->mainOutputName->text();
	replace_invalid_filename_chars(&config->OutputName);
	config->OutputGroups = ui->mainOutputGroups->text();

	config->PreviewOutputEnabled = ui->previewOutputGroupBox->isChecked();
	config->PreviewOutputName = ui->previewOutputName->text();
	replace_invalid_filename_chars(&config->PreviewOutputName);
	config->PreviewOutputGroups = ui->previewOutputGroups->text();

	config->TallyProgramEnabled = ui->tallyProgramCheckBox->isChecked();
	config->TallyPreviewEnabled = ui->tallyPreviewCheckBox->isChecked();

	const bool auto_check_for_updates = ui->toolButtonAutoCheckForUpdates->isChecked();
	if (last_config.AutoCheckForUpdates() != auto_check_for_updates) {
		obs_log(LOG_INFO, "Automatic update checks changed to %s",
			auto_check_for_updates ? "enabled" : "disabled");
	}
	config->AutoCheckForUpdates(auto_check_for_updates);

	auto mainSupported = ui->mainOutputGroupBox->isEnabled();

	obs_log(LOG_INFO, "Main output supported='%d'", mainSupported);

	// Output settings for debugging & diagnosis
	obs_log(LOG_INFO,
		"Output Settings set to MainEnabled='%d', MainName='%s', MainGroup='%s', PreviewEnabled='%d', PreviewName='%s', PreviewGroup='%s'",
		config->OutputEnabled, config->OutputName.toUtf8().constData(),
		config->OutputGroups.toUtf8().constData(), config->PreviewOutputEnabled,
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
		main_output_stop();
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
		preview_output_stop();
	}
}

void OutputSettings::showEvent(QShowEvent *)
{
	refreshUI();
}

void OutputSettings::toggleShowHide()
{
	setVisible(!isVisible());
}

void OutputSettings::onConfigChanged()
{
	// Only update widgets while dialog is visible to avoid surprising background UI changes.
	if (!isVisible())
		return;

	refreshUI();
}

void OutputSettings::refreshUI()
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

	obs_data_t *main_settings = nullptr;
	main_output_get_settings(main_settings);
	if (main_settings) {
		config->OutputName = obs_data_get_string(main_settings, "ndi_name");
		config->OutputGroups = obs_data_get_string(main_settings, "ndi_groups");
		obs_data_release(main_settings);
		config->Save();
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

	obs_data_t *preview_settings = nullptr;
	preview_output_get_settings(preview_settings);
	if (preview_settings) {
		config->PreviewOutputName = obs_data_get_string(preview_settings, "ndi_name");
		config->PreviewOutputGroups = obs_data_get_string(preview_settings, "ndi_groups");
		obs_data_release(preview_settings);
		config->Save();
	}

	ui->previewOutputGroupBox->setChecked(config->PreviewOutputEnabled);
	ui->previewOutputName->setText(config->PreviewOutputName);
	ui->previewOutputGroups->setText(config->PreviewOutputGroups);

	ui->tallyProgramCheckBox->setChecked(config->TallyProgramEnabled);
	ui->tallyPreviewCheckBox->setChecked(config->TallyPreviewEnabled);

	ui->toolButtonAutoCheckForUpdates->setChecked(config->AutoCheckForUpdates());
}
