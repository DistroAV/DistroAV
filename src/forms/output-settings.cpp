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

#include "../obs-ndi.h"
#include "../preview-output.h"

OutputSettings::OutputSettings(Config& config, QWidget *parent) :
	QDialog(parent),
	ui(new Ui::OutputSettings),
	_config(config)
{
	ui->setupUi(this);
	connect(ui->buttonBox, SIGNAL(accepted()),
		this, SLOT(onFormAccepted()));

	ui->ndiVersionLabel->setText(ndiLib->version());
}

void OutputSettings::onFormAccepted()
{
	_config.OutputEnabled = ui->mainOutputGroupBox->isChecked();
	_config.OutputName = ui->mainOutputName->text();

	_config.PreviewOutputEnabled = ui->previewOutputGroupBox->isChecked();
	_config.PreviewOutputName = ui->previewOutputName->text();

	_config.Save();

	if (_config.OutputEnabled) {
		if (main_output_is_running()) {
			main_output_stop();
		}
		main_output_start(ui->mainOutputName->text().toUtf8().constData());
	} else {
		main_output_stop();
	}

	if (_config.PreviewOutputEnabled) {
		if (preview_output_is_enabled()) {
			preview_output_stop();
		}
		preview_output_start(ui->previewOutputName->text().toUtf8().constData());
	}
	else {
		preview_output_stop();
	}
}

void OutputSettings::showEvent(QShowEvent* event)
{
	ui->mainOutputGroupBox->setChecked(_config.OutputEnabled);
	ui->mainOutputName->setText(_config.OutputName);

	ui->previewOutputGroupBox->setChecked(_config.PreviewOutputEnabled);
	ui->previewOutputName->setText(_config.PreviewOutputName);
}

void OutputSettings::ToggleShowHide()
{
	if (!isVisible())
		setVisible(true);
	else
		setVisible(false);
}

OutputSettings::~OutputSettings()
{
	delete ui;
}
