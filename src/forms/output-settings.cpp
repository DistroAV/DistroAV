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

#include <obs-frontend-api.h>
#include "../plugin-main.h"
#include "../preview-output.h"

#include <QDesktopServices>
#include <QMessageBox>
#include <QPushButton>

// Copied from OBS UI/obs-app.hpp
inline const char *Str(const char *lookup)
{
	// One line tweak/override
	// to use obs_frontend_get_locale_string
	// instead of ((OBSApp*)App())->GetString(lookup)
	return obs_frontend_get_locale_string(lookup);
}

inline QString QTStr(const char *lookupVal)
{
	return QString::fromUtf8(Str(lookupVal));
}

// Copied from OBS UI/qt-wrappers.cpp OBSMessageBox::question
QMessageBox::StandardButton question(QWidget *parent, const QString &title,
				     const QString &text,
				     QMessageBox::StandardButtons buttons,
				     QMessageBox::StandardButton defaultButton)
{
	QMessageBox mb(QMessageBox::Question, title, text,
		       QMessageBox::NoButton, parent);
	mb.setDefaultButton(defaultButton);

	if (buttons & QMessageBox::Ok) {
		QPushButton *button = mb.addButton(QMessageBox::Ok);
		button->setText(QTStr("OK"));
	}
#define add_button(x)                                               \
	if (buttons & QMessageBox::x) {                             \
		QPushButton *button = mb.addButton(QMessageBox::x); \
		button->setText(QTStr(#x));                         \
	}
	add_button(Open);
	add_button(Save);
	add_button(Cancel);
	add_button(Close);
	add_button(Discard);
	add_button(Apply);
	add_button(Reset);
	add_button(Yes);
	add_button(No);
	add_button(Abort);
	add_button(Retry);
	add_button(Ignore);
#undef add_button
	return (QMessageBox::StandardButton)mb.exec();
}

// Copied from OBS UI/properties-view.cpp WidgetInfo::ButtonClicked()
// to behave like obs-ndi-filter & obs-ndi-source
void ButtonUrlClicked(QWidget *view, char *savedUrl)
{
	QUrl url(savedUrl, QUrl::StrictMode);
	if (url.isValid() && (url.scheme().compare("http") == 0 ||
			      url.scheme().compare("https") == 0)) {
		QString msg(QTStr("Basic.PropertiesView.UrlButton.Text"));
		msg += "\n\n";
		msg += QString(QTStr("Basic.PropertiesView.UrlButton.Text.Url"))
			       .arg(savedUrl);

		QMessageBox::StandardButton button = question(
			view->window(),
			QString(QTStr("Basic.PropertiesView.UrlButton.OpenUrl")),
			msg, QMessageBox::Yes | QMessageBox::No,
			QMessageBox::No);

		if (button == QMessageBox::Yes)
			QDesktopServices::openUrl(url);
	}
}

OutputSettings::OutputSettings(QWidget *parent)
	: QDialog(parent),
	  ui(new Ui::OutputSettings)
{
	ui->setupUi(this);

	auto pluginNameUpperCase = QString("%1").arg(PLUGIN_NAME).toUpper();

	setWindowTitle(QString("%1 %2 - %3")
			       .arg(pluginNameUpperCase, PLUGIN_VERSION,
				    windowTitle()));
	connect(ui->buttonBox, SIGNAL(accepted()), this,
		SLOT(onFormAccepted()));

	ui->ndiVersionLabel->setText(ndiLib->version());

	ui->discordHtml->setText(
		QString("<a href=\"%1\">%2 Discord</a>")
			.arg(PLUGIN_DISCORD, pluginNameUpperCase));
	ui->discordHtml->connect(
		ui->discordHtml, &QLabel::linkActivated,
		[this](const QString &savedUrl) {
			ButtonUrlClicked(this, savedUrl.toUtf8().data());
		});
}

void OutputSettings::onFormAccepted()
{
	auto conf = GetConfig().get();

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

void OutputSettings::showEvent(QShowEvent *)
{
	Config *conf = GetConfig().get();

	ui->mainOutputGroupBox->setChecked(conf->OutputEnabled);
	ui->mainOutputName->setText(conf->OutputName);

	ui->previewOutputGroupBox->setChecked(conf->PreviewOutputEnabled);
	ui->previewOutputName->setText(conf->PreviewOutputName);

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
