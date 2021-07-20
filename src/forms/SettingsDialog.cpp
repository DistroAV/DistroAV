#include <QtWidgets/QMessageBox>
#include <obs-module.h>
#include <obs-frontend-api.h>

#include "SettingsDialog.h"
#include "../obs-ndi.h"

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
}

SettingsDialog::~SettingsDialog()
{
	delete ui;
}

void SettingsDialog::showEvent(QShowEvent *event)
{
	//auto conf = GetConfig();
	//if (!conf) {
	//	blog(LOG_ERROR, "[SettingsDialog::showEvent] Unable to retreive config!");
	//	return;
	//}
}

void SettingsDialog::closeEvent(QCloseEvent *event)
{
}

void SettingsDialog::ToggleShowHide()
{
	if (!isVisible())
		setVisible(true);
	else
		setVisible(false);
}
