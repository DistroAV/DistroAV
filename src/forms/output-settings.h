/*
	obs-ndi (NDI I/O in OBS Studio)
	Copyright (C) 2016 Stéphane Lepin <stephane.lepin@gmail.com>

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library. If not, see <https://www.gnu.org/licenses/>
*/

#ifndef OUTPUTSETTINGS_H
#define OUTPUTSETTINGS_H

#include <QDialog>

namespace Ui {
class OutputSettings;
}

class OutputSettings : public QDialog
{
	Q_OBJECT

public:
	explicit OutputSettings(QWidget *parent = 0);
	~OutputSettings();
	void showEvent(QShowEvent *event);
	void ToggleShowHide();

private Q_SLOTS:
	void FormAccepted();

private:
	Ui::OutputSettings *ui;
};

#endif // OUTPUTSETTINGS_H
