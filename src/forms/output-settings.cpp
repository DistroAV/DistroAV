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
#include "../main-output.h"
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
	_config.setMainOutputEnabled(ui->mainOutputGroupBox->isChecked());
	_config.setMainOutputName(ui->mainOutputName->text());

	_config.setPreviewOutputEnabled(ui->previewOutputGroupBox->isChecked());
	_config.setPreviewOutputName(ui->previewOutputName->text());

	_config.save();

	if (_config.mainOutputEnabled()) {
		if (main_output_is_running()) {
			main_output_stop();
		}
		main_output_start(QT_TO_UTF8(ui->mainOutputName->text()));
	} else {
		main_output_stop();
	}

	if (_config.previewOutputEnabled()) {
		if (preview_output_is_enabled()) {
			preview_output_stop();
		}
		preview_output_start(QT_TO_UTF8(ui->previewOutputName->text()));
	}
	else {
		preview_output_stop();
	}
}

void OutputSettings::showEvent(QShowEvent* event)
{
	ui->mainOutputGroupBox->setChecked(_config.mainOutputEnabled());
	ui->mainOutputName->setText(_config.mainOutputName());

	ui->previewOutputGroupBox->setChecked(_config.previewOutputEnabled());
	ui->previewOutputName->setText(_config.previewOutputName());
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
