/*
obs-ndi
Copyright (C) 2016-2018 St�phane Lepin <steph  name of author

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; If not, see <https://www.gnu.org/licenses/>
*/

#include "output-settings.h"

#include "../Config.h"
#include "../obs-ndi.h"
#include "../preview-output.h"

OutputSettings::OutputSettings(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::OutputSettings)
{
	ui->setupUi(this);
	connect(ui->buttonBox, SIGNAL(accepted()),
		this, SLOT(onFormAccepted()));

	ui->ndiVersionLabel->setText(ndiLib->version());
}

void OutputSettings::onFormAccepted() {
	Config* conf = Config::Current();

	conf->OutputEnabled = ui->mainOutputGroupBox->isChecked();
	conf->OutputName = ui->mainOutputName->text();

	conf->PreviewOutputEnabled = ui->previewOutputGroupBox->isChecked();
	conf->PreviewOutputName = ui->previewOutputName->text();

	conf->TallyProgramEnabled = ui->tallyProgramCheckBox->isChecked();
	conf->TallyPreviewEnabled = ui->tallyPreviewCheckBox->isChecked();

	conf->Save();

	if (conf->OutputEnabled) {
		if (main_output_is_running()) {
			main_output_stop();
		}
		main_output_start(ui->mainOutputName->text().toUtf8().constData());
	} else {
		main_output_stop();
	}

	if (conf->PreviewOutputEnabled) {
		if (preview_output_is_enabled()) {
			preview_output_stop();
		}
		preview_output_start(ui->previewOutputName->text().toUtf8().constData());
	}
	else {
		preview_output_stop();
	}
}

void OutputSettings::showEvent(QShowEvent* event) {
	Config* conf = Config::Current();

	ui->mainOutputGroupBox->setChecked(conf->OutputEnabled);
	ui->mainOutputName->setText(conf->OutputName);

	ui->previewOutputGroupBox->setChecked(conf->PreviewOutputEnabled);
	ui->previewOutputName->setText(conf->PreviewOutputName);

	ui->tallyProgramCheckBox->setChecked(conf->TallyProgramEnabled);
	ui->tallyPreviewCheckBox->setChecked(conf->TallyPreviewEnabled);
}

void OutputSettings::ToggleShowHide() {
	if (!isVisible())
		setVisible(true);
	else
		setVisible(false);
}

OutputSettings::~OutputSettings() {
	delete ui;
}
