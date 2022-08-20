#include <QPushButton>
#include <QMessageBox>
#include <obs-module.h>
#include <obs-frontend-api.h>

#include "SettingsDialog.h"
#include "../obs-ndi.h"
#include "../config.h"
#include "../output-manager.h"

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent, Qt::Dialog), ui(new Ui::SettingsDialog)
{
	ui->setupUi(this);

	// Remove the ? button on dialogs on Windows
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &SettingsDialog::DialogButtonClicked);
}

SettingsDialog::~SettingsDialog()
{
	delete ui;
}

void SettingsDialog::showEvent(QShowEvent *)
{
	auto conf = get_config();
	if (!conf)
		return;

	ui->ndiExtraIpsLineEdit->setText(QString::fromStdString(conf->ndi_extra_ips));
	ui->programOutputGroupBox->setChecked(conf->program_output_enabled);
	ui->programOutputSenderNameLineEdit->setText(QString::fromStdString(conf->program_output_name));
	ui->previewOutputGroupBox->setChecked(conf->preview_output_enabled);
	ui->previewOutputSenderNameLineEdit->setText(QString::fromStdString(conf->preview_output_name));
}

void SettingsDialog::hideEvent(QHideEvent *) {}

void SettingsDialog::ToggleShowHide()
{
	setVisible(!isVisible());
}

void SettingsDialog::DialogButtonClicked(QAbstractButton *button)
{
	if (button != ui->buttonBox->button(QDialogButtonBox::Cancel))
		SaveFormData();
}

void SettingsDialog::SaveFormData()
{
	auto conf = get_config();
	if (!conf)
		return;

	auto output_manager = get_output_manager();
	if (!output_manager)
		return;

	bool extra_ips_changed = conf->ndi_extra_ips != ui->ndiExtraIpsLineEdit->text().toStdString();
	conf->ndi_extra_ips = ui->ndiExtraIpsLineEdit->text().toStdString();

	conf->program_output_enabled = ui->programOutputGroupBox->isChecked();
	conf->program_output_name = ui->programOutputSenderNameLineEdit->text().toStdString();

	conf->save();

	if (extra_ips_changed)
		restart_ndi_finder();

	output_manager->update_program_output();
}
