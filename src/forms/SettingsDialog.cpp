#include <QPushButton>
#include <QMessageBox>
#include <obs-module.h>
#include <obs-frontend-api.h>

#include "SettingsDialog.h"
#include "../obs-ndi.h"
#include "../obs-ndi-config.h"

QString GetToolTipIconHtml()
{
	bool lightTheme = QApplication::palette().text().color().redF() < 0.5;
	QString iconFile = lightTheme ? ":toolTip/images/help.svg" : ":toolTip/images/help_light.svg";
	QString iconTemplate = "<html> <img src='%1' style=' vertical-align: bottom; ' /></html>";
	return iconTemplate.arg(iconFile);
}

SettingsDialog::SettingsDialog(QWidget* parent) :
	QDialog(parent, Qt::Dialog),
	ui(new Ui::SettingsDialog)
{
	ui->setupUi(this);

	// Remove the ? button on dialogs on Windows
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// Set the appropriate tooltip icon for the theme
	//ui->enableDebugLoggingToolTipLabel->setText(GetToolTipIconHtml());

	connect(ui->buttonBox, &QDialogButtonBox::clicked,
		this, &SettingsDialog::DialogButtonClicked);
}

SettingsDialog::~SettingsDialog()
{
	delete ui;
}

void SettingsDialog::showEvent(QShowEvent *event)
{
	auto conf = get_config();
	if (!conf) {
		blog(LOG_ERROR, "[SettingsDialog::showEvent] Unable to retreive config!");
		return;
	}
}

void SettingsDialog::hideEvent(QHideEvent *event)
{
}

void SettingsDialog::ToggleShowHide()
{
	if (!isVisible())
		setVisible(true);
	else
		setVisible(false);
}

void SettingsDialog::DialogButtonClicked(QAbstractButton *button)
{
	if (button != ui->buttonBox->button(QDialogButtonBox::Cancel))
		SaveFormData();
}

void SettingsDialog::SaveFormData()
{
	auto conf = get_config();
	if (!conf) {
		blog(LOG_ERROR, "[SettingsDialog::SaveFormData] Unable to retreive config!");
		return;
	}

	bool extra_ips_changed = conf->ndi_extra_ips == ui->ndiExtraIpsLineEdit->text().toStdString();
	conf->ndi_extra_ips = ui->ndiExtraIpsLineEdit->text().toStdString();

	conf->save();

	if (extra_ips_changed)
		restart_ndi_finder();
}
