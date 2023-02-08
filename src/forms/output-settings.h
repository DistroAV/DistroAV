#ifndef OUTPUTSETTINGS_H
#define OUTPUTSETTINGS_H

#include <QDialog>

#include "ui_output-settings.h"

class OutputSettings : public QDialog {
	Q_OBJECT
public:
	explicit OutputSettings(QWidget *parent = 0);
	~OutputSettings();
	void showEvent(QShowEvent *event);
	void ToggleShowHide();

private slots:
	void onFormAccepted();

private:
	Ui::OutputSettings *ui;
};

#endif // OUTPUTSETTINGS_H
