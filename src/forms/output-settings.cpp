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

OutputSettings::OutputSettings(QWidget *parent)
	: QDialog(parent),
	  ui(new Ui::OutputSettings)
{
	ui->setupUi(this);

	connect(ui->buttonBox, SIGNAL(accepted()), this,
		SLOT(onFormAccepted()));

	auto pluginVersionText =
		QString("%1 %2").arg(PLUGIN_DISPLAY_NAME).arg(PLUGIN_VERSION);
	ui->labelDistroAvVersion->setText(
		makeLink("#", QT_TO_UTF8(pluginVersionText)));
	connect(ui->labelDistroAvVersion, &QLabel::linkActivated,
		[this, pluginVersionText](const QString &) {
			QApplication::clipboard()->setText(pluginVersionText);
			QMessageBox::information(
				this,
				Str("NDIPlugin.OutputSettings.TextCopied"),
				Str("NDIPlugin.OutputSettings.TextCopiedToClipboard"));
		});

	connect(ui->pushButtonCheckForUpdate, &QPushButton::clicked, [this]() {
		// Whew! QProgressDialog is ugly on Windows!
		// TODO: Write our own.
		auto progressDialog = new QProgressDialog(
			QTStr("NDIPlugin.Update.CheckingForUpdate.Text")
				.arg(PLUGIN_DISPLAY_NAME),
			Str("NDIPlugin.Update.CheckingForUpdate.Cancel"), 0, 0,
			this);
		progressDialog->setAttribute(Qt::WA_DeleteOnClose, true);
		progressDialog->setWindowModality(Qt::WindowModal);
		connect(progressDialog, &QProgressDialog::canceled,
			[progressDialog]() {
				updateCheckStop();
				progressDialog->close();
			});
		auto checking = updateCheckStart([this, progressDialog](
							 const PluginUpdateInfo &
								 pluginUpdateInfo)
							 -> bool {
			progressDialog->close();

			if (!pluginUpdateInfo.errorData.isEmpty()) {
				QString errorData = pluginUpdateInfo.errorData;
				if (LOG_LEVEL >= LOG_DEBUG) {
					QJsonParseError parseError;
					QJsonDocument jsonDoc =
						QJsonDocument::fromJson(
							errorData.toUtf8(),
							&parseError);
					if (parseError.error ==
					    QJsonParseError::NoError) {
						QJsonObject jsonObject =
							jsonDoc.object();
						if (jsonObject.contains(
							    "error")) {
							errorData =
								jsonObject["error"]
									.toString();
						}
					}
				}

				auto errorText = QTStr(
					"NDIPlugin.Update.CheckingForUpdate.Error.Text");
				errorText +=
					QString("<pre>%1</pre>").arg(errorData);

				if (pluginUpdateInfo.httpStatusCode ==
					    404 // Not Found
				    || pluginUpdateInfo.httpStatusCode ==
					       412 // Precondition Failed
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

				QMessageBox::warning(
					this,
					Str("NDIPlugin.Update.CheckingForUpdate.Error.Title"),
					errorText);
				return false;
			}

			if (pluginUpdateInfo.versionLatest <=
				    pluginUpdateInfo.versionCurrent &&
			    Config::UpdateForce < 1) {
				QMessageBox::information(
					this,
					QTStr("NDIPlugin.Update.NoUpdateAvailable")
						.arg(PLUGIN_DISPLAY_NAME),
					QTStr("NDIPlugin.Update.YouAreUpToDate")
						.arg(PLUGIN_DISPLAY_NAME,
						     pluginUpdateInfo
							     .versionCurrent
							     .toString()));
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

	auto ndiVersionText = QString(ndiLib->version());
	ui->labelNdiVersion->setText(makeLink("#", QT_TO_UTF8(ndiVersionText)));
	connect(ui->labelNdiVersion, &QLabel::linkActivated,
		[this, ndiVersionText](const QString &) {
			QApplication::clipboard()->setText(ndiVersionText);
			QMessageBox::information(
				this,
				Str("NDIPlugin.OutputSettings.TextCopied"),
				Str("NDIPlugin.OutputSettings.TextCopiedToClipboard"));
		});

	ui->pushButtonNdi->setText(QString("%1 %2").arg(
		ui->pushButtonNdi->text(), NDI_OFFICIAL_WEB_URL));
	connect(ui->pushButtonNdi, &QPushButton::clicked, []() {
		QDesktopServices::openUrl(
			QUrl(rehostUrl(PLUGIN_REDIRECT_NDI_WEB_URL)));
	});

#if 1
	ui->pushButtonNdiTools->setVisible(false);
	ui->pushButtonNdiRedist->setVisible(false);
#else
	//
	// These are not useful to users that can see this Dialog because
	// they have already installed and successfully loaded the NDI SDK.
	// Keeping the code around for a little while longer...
	//
#ifdef NDI_OFFICIAL_TOOLS_URL
	ui->pushButtonNdiTools->setText(NDI_OFFICIAL_TOOLS_URL);
	connect(ui->pushButtonNdiTools, &QPushButton::clicked, []() {
		QDesktopServices::openUrl(
			QUrl(rehostUrl(PLUGIN_REDIRECT_NDI_TOOLS_URL)));
	});
#else
	ui->pushButtonNdiTools->setVisible(false);
#endif

#ifdef NDI_OFFICIAL_REDIST_URL
	ui->pushButtonNdiRedist->setText(NDI_OFFICIAL_REDIST_URL);
#else
	ui->pushButtonNdiRedist->setText(PLUGIN_REDIRECT_NDI_REDIST_URL);
#endif
	connect(ui->pushButtonNdiRedist, &QPushButton::clicked, []() {
		QDesktopServices::openUrl(
			QUrl(rehostUrl(PLUGIN_REDIRECT_NDI_REDIST_URL)));
	});
#endif

	ui->labelNdiRegisteredTrademark->setText(
		NDI_IS_A_REGISTERED_TRADEMARK_TEXT);

	ui->labelDonateUrl->setText(makeLink(PLUGIN_REDIRECT_DONATE_URL));
	connect(ui->labelDonateUrl, &QLabel::linkActivated,
		[this](const QString &url) {
			QDesktopServices::openUrl(QUrl(url));
		});

	ui->labelDiscordUrl->setText(makeLink(PLUGIN_REDIRECT_DISCORD_URL));
	connect(ui->labelDiscordUrl, &QLabel::linkActivated,
		[this](const QString &url) {
			QDesktopServices::openUrl(QUrl(url));
		});
}

void OutputSettings::onFormAccepted()
{
	auto config = Config::Current();

	config->OutputEnabled = ui->mainOutputGroupBox->isChecked();
	config->OutputName = ui->mainOutputName->text();
	config->OutputGroups = ui->mainOutputGroups->text();

	config->PreviewOutputEnabled = ui->previewOutputGroupBox->isChecked();
	config->PreviewOutputName = ui->previewOutputName->text();
	config->PreviewOutputGroups = ui->previewOutputGroups->text();

	config->TallyProgramEnabled = ui->tallyProgramCheckBox->isChecked();
	config->TallyPreviewEnabled = ui->tallyPreviewCheckBox->isChecked();

	config->AutoCheckForUpdates(
		ui->checkBoxAutoCheckForUpdates->isChecked());

	config->Save();

	main_output_init();
	preview_output_init();
}

void OutputSettings::showEvent(QShowEvent *)
{
	auto config = Config::Current();

	ui->mainOutputGroupBox->setChecked(config->OutputEnabled);
	ui->mainOutputName->setText(config->OutputName);
	ui->mainOutputGroups->setText(config->OutputGroups);

	ui->previewOutputGroupBox->setChecked(config->PreviewOutputEnabled);
	ui->previewOutputName->setText(config->PreviewOutputName);
	ui->previewOutputGroups->setText(config->PreviewOutputGroups);

	ui->tallyProgramCheckBox->setChecked(config->TallyProgramEnabled);
	ui->tallyPreviewCheckBox->setChecked(config->TallyPreviewEnabled);

	ui->checkBoxAutoCheckForUpdates->setChecked(
		config->AutoCheckForUpdates());
}

void OutputSettings::toggleShowHide()
{
	setVisible(!isVisible());
}
