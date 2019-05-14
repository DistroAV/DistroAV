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

#include "ndi-settings.h"

#include "../Config.h"
#include "../obs-ndi.h"
#include "../ndi-source-finder.h"
#include "../main-output.h"
#include "../preview-output.h"

NDISettings::NDISettings(QWidget *parent) :
        QDialog(parent),
        ui(new Ui::NDISettings)
{
    ui->setupUi(this);
    connect(ui->buttonBox, SIGNAL(accepted()),
            this, SLOT(onFormAccepted()));

    ui->ndiVersionLabel->setText(ndiLib->NDIlib_version());
}

void NDISettings::onFormAccepted() {
    Config* conf = Config::Current();

    conf->FinderExtraIps = ui->extraIps->text().trimmed();

    conf->OutputEnabled = ui->mainOutputGroupBox->isChecked();
    conf->OutputName = ui->mainOutputName->text();

    conf->PreviewOutputEnabled = ui->previewOutputGroupBox->isChecked();
    conf->PreviewOutputName = ui->previewOutputName->text();

    conf->Save();

    update_ndi_finder(conf->FinderExtraIps.toUtf8().constData());

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

void NDISettings::showEvent(QShowEvent* event) {
    Config* conf = Config::Current();

    ui->extraIps->setText(conf->FinderExtraIps);
    ui->mainOutputGroupBox->setChecked(conf->OutputEnabled);
    ui->mainOutputName->setText(conf->OutputName);

    ui->previewOutputGroupBox->setChecked(conf->PreviewOutputEnabled);
    ui->previewOutputName->setText(conf->PreviewOutputName);
}

void NDISettings::ToggleShowHide() {
    if (!isVisible())
        setVisible(true);
    else
        setVisible(false);
}

NDISettings::~NDISettings() {
    delete ui;
}
