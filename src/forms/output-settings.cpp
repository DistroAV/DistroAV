#include "output-settings.h"

#include "../Config.h"
#include "../obs-ndi.h"
#include "../preview-output.h"

OutputSettings::OutputSettings(QWidget *parent)
	: QDialog(parent), ui(new Ui::OutputSettings)
{
	ui->setupUi(this);
	connect(ui->buttonBox, SIGNAL(accepted()), this,
		SLOT(onFormAccepted()));

	ui->ndiVersionLabel->setText(ndiLib->version());
}

void OutputSettings::onFormAccepted()
{
	Config *conf = Config::Current();

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
		main_output_start(
			ui->mainOutputName->text().toUtf8().constData());
	} else {
		main_output_stop();
	}

	if (conf->PreviewOutputEnabled) {
		if (preview_output_is_enabled()) {
			preview_output_stop();
		}
		preview_output_start(
			ui->previewOutputName->text().toUtf8().constData());
	} else {
		preview_output_stop();
	}
}

void OutputSettings::showEvent(QShowEvent *event)
{
	Config *conf = Config::Current();

	ui->mainOutputGroupBox->setChecked(conf->OutputEnabled);
	ui->mainOutputName->setText(conf->OutputName);

	ui->previewOutputGroupBox->setChecked(conf->PreviewOutputEnabled);
	ui->previewOutputName->setText(conf->PreviewOutputName);

	ui->tallyProgramCheckBox->setChecked(conf->TallyProgramEnabled);
	ui->tallyPreviewCheckBox->setChecked(conf->TallyPreviewEnabled);
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
