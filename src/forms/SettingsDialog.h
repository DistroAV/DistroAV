#pragma once

#include <QDialog>

#include "ui_SettingsDialog.h"

class SettingsDialog : public QDialog {
	Q_OBJECT

public:
	explicit SettingsDialog(QWidget *parent = 0);
	~SettingsDialog();
	void showEvent(QShowEvent *event);
	void hideEvent(QHideEvent *event);
	void ToggleShowHide();

private Q_SLOTS:
	void DialogButtonClicked(QAbstractButton *button);
	void SaveFormData();

private:
	Ui::SettingsDialog *ui;
};
