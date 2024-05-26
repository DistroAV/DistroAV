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

#include "output-settings.h"

#include "../plugin-main.h"
#include "../main-output.h"
#include "../preview-output.h"
#include "obsndi-update.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QMessageBox>

OutputSettings::OutputSettings(QWidget *parent)
	: QDialog(parent),
	  ui(new Ui::OutputSettings)
{
	ui->setupUi(this);
	connect(ui->buttonBox, SIGNAL(accepted()), this,
		SLOT(onFormAccepted()));

	auto obsNdiVersionText =
		QString("%1 %2").arg(PLUGIN_DISPLAY_NAME).arg(PLUGIN_VERSION);
	ui->labelObsNdiVersion->setText(
		QString("<a href=\"#\">%1</a>").arg(obsNdiVersionText));
	connect(ui->labelObsNdiVersion, &QLabel::linkActivated,
		[this, obsNdiVersionText](const QString &) {
			QApplication::clipboard()->setText(obsNdiVersionText);
			QMessageBox::information(
				this,
				Str("NDIPlugin.OutputSettings.TextCopied"),
				Str("NDIPlugin.OutputSettings.TextCopiedToClipboard"));
		});

	ui->pushButtonCheckForUpdate->setText(
		Str("NDIPlugin.OutputSettings.CheckForUpdate"));
	connect(ui->pushButtonCheckForUpdate, &QPushButton::clicked,
		[]() { updateCheckStart(true); });

	auto ndiVersionText = QString(ndiLib->version());
	ui->labelNdiVersion->setText(
		QString("<a href=\"#\">%1</a>").arg(ndiVersionText));
	connect(ui->labelNdiVersion, &QLabel::linkActivated,
		[this, ndiVersionText](const QString &) {
			QApplication::clipboard()->setText(ndiVersionText);
			QMessageBox::information(
				this,
				Str("NDIPlugin.OutputSettings.TextCopied"),
				Str("NDIPlugin.OutputSettings.TextCopiedToClipboard"));
		});
	connect(ui->pushButtonNdi, &QPushButton::clicked,
		[]() { QDesktopServices::openUrl(QUrl(NDI_WEB_URL)); });
#if defined(_WIN32) || defined(__APPLE__)
	connect(ui->pushButtonNdiTools, &QPushButton::clicked,
		[]() { QDesktopServices::openUrl(QUrl(NDI_TOOLS_URL)); });
#else
	ui->pushButtonNdiTools->setVisible(false);
#endif
	connect(ui->pushButtonNdiRedist, &QPushButton::clicked,
		[]() { QDesktopServices::openUrl(QUrl(NDILIB_REDIST_URL)); });

	ui->labelDonate->setText(Str("NDIPlugin.Donate"));
	ui->labelDonateUrl->setText(
		QString("<a href=\"%1\">%1</a>").arg(PLUGIN_DONATE_URL));
	connect(ui->labelDonateUrl, &QLabel::linkActivated,
		[this](const QString &) {
			QDesktopServices::openUrl(QUrl(PLUGIN_DONATE_URL));
		});

	ui->labelDiscordUrl->setText(
		QString("<a href=\"%1\">%1</a>").arg(PLUGIN_DISCORD_URL));
	connect(ui->labelDiscordUrl, &QLabel::linkActivated,
		[this](const QString &) {
			QDesktopServices::openUrl(QUrl(PLUGIN_DISCORD_URL));
		});
}

OutputSettings::~OutputSettings()
{
	delete ui;
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

	if (main_output_is_running()) {
		main_output_stop();
	}
	main_output_start();

	if (preview_output_is_enabled()) {
		preview_output_stop();
	}
	preview_output_start();
}

void OutputSettings::showEvent(QShowEvent *)
{
	auto conf = Config::Current();

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
