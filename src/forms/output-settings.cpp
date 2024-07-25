/******************************************************************************
	Copyright (C) 2016-2024 DistroAV <distroav@distroav.org>

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
		QString("%1 %2").arg(PLUGIN_NAME).arg(PLUGIN_VERSION);
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
		auto progressDialog = new QProgressDialog(
			QTStr("NDIPlugin.Update.CheckingForUpdate.Text")
				.arg(PLUGIN_NAME),
			Str("NDIPlugin.Update.CheckingForUpdate.Cancel"), 0, 0,
			this);
		progressDialog->setWindowModality(Qt::WindowModal);
		connect(progressDialog, &QProgressDialog::canceled,
			[progressDialog]() {
				updateCheckStop();
				progressDialog->close();
				progressDialog->deleteLater();
			});
		auto checking = updateCheckStart([this, progressDialog](
							 const PluginUpdateInfo &
								 pluginUpdateInfo)
							 -> bool {
			progressDialog->close();
			progressDialog->deleteLater();

			if (!pluginUpdateInfo.errorData.isEmpty()) {
				QMessageBox::warning(
					this,
					Str("NDIPlugin.Update.CheckingForUpdate.Error.Title"),
					QTStr("NDIPlugin.Update.CheckingForUpdate.Error.Text")
						.arg(pluginUpdateInfo
							     .errorData));
				return false;
			}

			if (pluginUpdateInfo.versionLatest <=
			    pluginUpdateInfo.versionCurrent) {
				QMessageBox::information(
					this,
					QTStr("NDIPlugin.Update.NoUpdateAvailable")
						.arg(PLUGIN_NAME),
					QTStr("NDIPlugin.Update.YouAreUpToDate")
						.arg(PLUGIN_NAME,
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
	auto conf = Config::Current();

	conf->OutputEnabled = ui->mainOutputGroupBox->isChecked();
	conf->OutputName = ui->mainOutputName->text();
	conf->OutputGroups = ui->mainOutputGroups->text();

	conf->PreviewOutputEnabled = ui->previewOutputGroupBox->isChecked();
	conf->PreviewOutputName = ui->previewOutputName->text();
	conf->PreviewOutputGroups = ui->previewOutputGroups->text();

	conf->TallyProgramEnabled = ui->tallyProgramCheckBox->isChecked();
	conf->TallyPreviewEnabled = ui->tallyPreviewCheckBox->isChecked();

	conf->Save();

	if (conf->OutputEnabled) {
		if (main_output_is_running()) {
			main_output_stop();
		}
		main_output_start(QT_TO_UTF8(ui->mainOutputName->text()),
				  QT_TO_UTF8(ui->mainOutputGroups->text()));
	} else {
		main_output_stop();
	}

	if (conf->PreviewOutputEnabled) {
		if (preview_output_is_enabled()) {
			preview_output_stop();
		}
		preview_output_start(
			QT_TO_UTF8(ui->previewOutputName->text()),
			QT_TO_UTF8(ui->previewOutputGroups->text()));
	} else {
		preview_output_stop();
	}
}

void OutputSettings::showEvent(QShowEvent *)
{
	auto conf = Config::Current()->Load();

	ui->mainOutputGroupBox->setChecked(conf->OutputEnabled);
	ui->mainOutputName->setText(conf->OutputName);
	ui->mainOutputGroups->setText(conf->OutputGroups);

	ui->previewOutputGroupBox->setChecked(conf->PreviewOutputEnabled);
	ui->previewOutputName->setText(conf->PreviewOutputName);
	ui->previewOutputGroups->setText(conf->PreviewOutputGroups);

	ui->tallyProgramCheckBox->setChecked(conf->TallyProgramEnabled);
	ui->tallyPreviewCheckBox->setChecked(conf->TallyPreviewEnabled);
}

void OutputSettings::ToggleShowHide()
{
	setVisible(!isVisible());
}

OutputSettings::~OutputSettings()
{
	delete ui;
}
