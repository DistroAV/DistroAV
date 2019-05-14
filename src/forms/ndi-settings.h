/*
obs-ndi
Copyright (C) 2016-2018 Stéphane Lepin <steph  name of author

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

#ifndef NDISETTINGS_H
#define NDISETTINGS_H

#include <QDialog>

#include "ui_ndi-settings.h"

class NDISettings : public QDialog {
  Q_OBJECT
  public:
	explicit NDISettings(QWidget* parent = 0);
	~NDISettings();
	void showEvent(QShowEvent* event);
	void ToggleShowHide();

  private slots:
	void onFormAccepted();

  private:
	Ui::NDISettings* ui;
};

#endif // NDISETTINGS_H
